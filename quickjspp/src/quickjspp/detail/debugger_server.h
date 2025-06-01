#pragma once
#include "line_tunnel.h"
#include "tcp_server.h"
#include <memory>
#include <optional>
#include "tcp_tunnel.h"
#include <variant>
namespace qjs::detail
{
    struct SetBreakLine
    {
        uint32_t line;
        std::string fileName;
    };
    struct ReadVariable
    {
        std::string variableName;
    };
    using Command = std::variant<std::monostate,SetBreakLine, ReadVariable>;
    class DebuggerServer
    {
    public:
        using Error=std::string;
        DebuggerServer()
        {
            
        }
        std::optional<Error> Run(uint16_t port=8173,const char* host="0.0.0.0")
        {
            if(!m_Server.Start(port, host))
            {
                return "Failed to start TCP server";
            }
            return std::nullopt;
        }
        
    private:
        std::optional<Error> HandleClient(socket_t client_socket)
        {
            TcpTunnel tcp{client_socket};
            LineTunnel tunnel;
            tunnel.readByte = [&tcp](uint8_t& byte) -> bool {
                if (!tcp.Read(&byte, sizeof(byte))) {
                    return false; // 读取失败
                }
                return true; // 成功读取一个字节
            };
            tunnel.writeBytes = [&tcp](const uint8_t* buffer, size_t size) -> bool {
                return tcp.Write(buffer, size); // 写入数据
            };
            std::string line;
            bool res= tunnel.ReadLine(line);
            if(!res)
            {
                return "Failed to read line from client";
            }
            HandleCommand handler{.tunnel=tunnel};
            while(true)
            {
                auto cmd=ParseCommand(line);
                std::visit(handler, cmd);
                if(handler.stop)
                {
                    break; // 停止处理命令
                }
                res= tunnel.ReadLine(line);
                if(!res)
                {
                    return "Failed to read line from client";
                }
            }
            return std::nullopt; // 成功处理完所有命令
        }
        Command ParseCommand(const std::string& line)
        {
            return Command();
        }
        struct HandleCommand
        {
            bool stop=false;

            LineTunnel& tunnel;
            template<typename T>
            void operator()(const T& command)
            {

            }
        };
        
        TcpServer m_Server;
    };
}