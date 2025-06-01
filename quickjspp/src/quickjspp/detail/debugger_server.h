#pragma once
#include "line_tunnel.h"
#include "tcp_server.h"
#include <atomic>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include "tcp_tunnel.h"
#include <variant>
#include <mutex>
#include <queue>
#include <cstdio>
#include "semaphore.h"
#include <format>
#include <winuser.h>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // 禁用安全函数警告
#endif
#ifdef SendMessage
#undef SendMessage
#endif
namespace qjs::detail
{
struct BreakLine
{
    uint32_t line;
    std::string fileName;
};
struct ReadVariable
{
    std::string name;
};
struct Continue
{
};

using Command = std::variant<std::monostate, BreakLine, ReadVariable, Continue>;
struct Variable
{
    std::string name;
    std::string value;
};
struct BreakHit
{
    std::string filename;
    uint32_t line;
};
using Message = std::variant<std::monostate, Variable, BreakHit>;

class DebuggerServer
{
public:
    using Error = std::string;
    DebuggerServer()
    {
    }
    std::optional<Error> Run(uint16_t port = 8173, const char* host = "0.0.0.0")
    {
        m_ServerStop.store(false);
        if (!m_Server.Start(port, host))
        {
            return "Failed to start TCP server";
        }
        while (IsRunning())
        {
            m_ClientStop = false;
            socket_t client = m_Server.Accept();
            if (client == INVALID_SOCKET)
            {
                LogE("failed to accept client connection");
                continue;
            }
            auto error = HandleClient(client);
            //close socket
            if (closesocket(client) == SOCKET_ERROR)
            {
                LogE("Failed to close client socket: {}", std::strerror(errno));
            }
            if (error.has_value())
            {
                LogE("Error handling client: {}", error.value());
            }
            else
            {
                LogE("Client disconnected or handled successfully");
            }
        }

        return std::nullopt;
    }
    void SetLogError(const std::function<void(const char* msg, size_t size)>& loge)
    {
        m_LogE = loge;
    }
    void Shutdown()
    {
        m_ServerStop.store(true);
        m_CommandSemaphore.Signal(); // 唤醒所有等待的线程
    }
    bool IsRunning() const
    {
        return !m_ServerStop.load();
    }

    Semaphore& GetCommandSemaphore()
    {
        return m_CommandSemaphore;
    }
    bool HasCommand()
    {
        std::lock_guard<std::mutex> lock(m_CommandsMutex);
        return !m_Commands.empty();
    }
    Command PopCommand()
    {
        std::lock_guard<std::mutex> lock(m_CommandsMutex);
        if (m_Commands.empty())
        {
            return Command{}; // 返回一个空的Command
        }
        Command cmd = std::move(m_Commands.front());
        m_Commands.pop();
        return cmd;
    }
private:
    struct SendMessageImpl
    {
        LineTunnel& tunnel;
        template <typename T>
        bool operator()(const T&)
        {
            return true;
        }
        template <>
        bool operator()(const BreakHit& breakHit)
        {
            std::string msg = "hit " + breakHit.filename + ":" + std::to_string(breakHit.line) + "\n";
            return tunnel.WriteLine(msg);
        }
        template<>
        bool operator()(const Variable& variable)
        {
            std::string msg = "variable " + variable.name + " = " + variable.value + "\n";
            return tunnel.WriteLine(msg);
        }
    };
public:
    void SendMessage(const Message& msg)
    {
        std::visit(SendMessageImpl{*m_CurrentTunnel}, msg);
    }

private:
   
   

    template <typename... Ts>
    void LogE(const char* fmt, const Ts&... args)
    {
        if (m_LogE)
        {
            std::string s = std::vformat(fmt, std::make_format_args(args...));
            m_LogE(s.data(), s.size());
        }
    }
    std::optional<Error> HandleClient(socket_t client_socket)
    {
        TcpTunnel tcp{client_socket};
        {
            LineTunnel tunnel;
            tunnel.readByte = [&tcp](uint8_t& byte) -> bool {
                if (!tcp.Read(&byte, sizeof(byte)))
                {
                    return false; // 读取失败
                }
                return true; // 成功读取一个字节
            };
            tunnel.writeBytes = [&tcp](const uint8_t* buffer, size_t size) -> bool {
                return tcp.Write(buffer, size); // 写入数据
            };
            m_CurrentTunnel = std::make_unique<LineTunnel>(std::move(tunnel));
        }
        auto& tunnel = *m_CurrentTunnel;

        std::string line;
        bool res = tunnel.ReadLine(line);
        if (!res)
        {
            return "Failed to read line from client";
        }
        while (true)
        {
            ParseCommand(line);
            if (m_ClientStop)
            {
                break; // 停止处理命令
            }
            res = tunnel.ReadLine(line);
            if (!res)
            {
                return "Failed to read line from client";
            }
        }
        m_CurrentTunnel.reset();
        return std::nullopt; // 成功处理完所有命令
    }

    void ParseCommand(const std::string& line)
    {
        /**
         * command:=break|read|stop|continue
         * break:="break" | "b" filename ":" line
         * read:="read" | "r" variableName
         * stop:="stop" | "s"
         * continue:="continue" | "c"
         */
        size_t pos = 0;
        auto consume = [&]() -> char {
            char res = line[pos];
            pos++;
            return res;
        };
        auto peek = [&]() -> char {
            return line[pos];
        };
        auto consumeAndCheck = [&](char c) -> bool {
            char t = consume();
            if (c != t)
            {
                return false;
            }
            return true;
        };
        auto peekAndCheck = [&](char c) -> bool {
            char t = peek();
            if (c != t)
            {
                return false;
            }
            return true;
        };
        auto readWord = [&]() -> std::string {
            std::string word;
            while (pos < line.size() && peek() != ' ' && peek() != '\n')
            {
                word += consume();
            }
            return word;
        };
        auto parseBreak = [&]() {
            std::string point = readWord();
            auto p = point.find(":");
            if (p == std::string::npos)
            {
                // todo: handle error
                return;
            }
            std::string fileName = point.substr(0, p);
            std::string lineStr = point.substr(p + 1);
            int32_t lineNumber = -1;
            try
            {
                lineNumber = std::stoi(lineStr, nullptr, 10);
            }
            catch (...)
            {
            }
            if (lineNumber == -1)
            {
                // todo: handle error
                return;
            }
            {
                BreakLine breakLine{static_cast<uint32_t>(lineNumber), fileName};
                PushCommand(std::move(breakLine));
            }
        };
        auto parseRead = [&]() {
            std::string variableName = readWord();
            ReadVariable readVariable{variableName};
            PushCommand(std::move(readVariable));
        };
        auto eatSpace = [&]() {
            while (pos < line.size() && peek() == ' ')
            {
                consume();
            }
        };

        auto word = readWord();
        if (word == "break" || word == "b")
        {
            eatSpace();
            parseBreak();
        }
        else if (word == "stop" || word == "s")
        {
            m_ClientStop = true;
        }
        else if (word == "read" || word == "r")
        {
            eatSpace();
            parseRead();
        }
        else if (word == "continue" || word == "c")
        {
            Continue continueCommand;
            PushCommand(std::move(continueCommand));
        }
        else
        {
            // todo: handle error
        }
    }
    template <typename T>
    void PushCommand(T&& t)
    {
        {
            std::lock_guard<std::mutex> lock(m_CommandsMutex);
            m_Commands.push(std::forward<T>(t));
        }
        m_CommandSemaphore.Signal();
    }
    bool m_ClientStop = false;             // 停止当前客户端的命令处理
    std::atomic_bool m_ServerStop = false; // 关闭server
    TcpServer m_Server;
    std::queue<Command> m_Commands;
    std::mutex m_CommandsMutex;
    Semaphore m_CommandSemaphore{0};
    std::function<void(const char* msg, size_t size)> m_LogE;
    std::unique_ptr<LineTunnel> m_CurrentTunnel; // 当前client连接的隧道
};
} // namespace qjs::detail