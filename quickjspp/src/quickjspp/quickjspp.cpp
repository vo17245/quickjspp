#include "quickjspp.h"
#include "detail/debugger_server.h"
#include "quickjs.h"
#include <memory>
#include <unordered_map>
#include <type_traits>
namespace
{
using namespace qjs;

std::unordered_map<JSContext*, void*> g_DebugUserdata;
JS_BOOL debug_handler(JSContext* ctx, JSAtom file_name, uint32_t line_no, const uint8_t* pc)
{
    void* userdata = g_DebugUserdata[ctx];
    assert(userdata && "Debugger userdata is not set. Please ensure you set the userdata before using the debugger.");
    Context* context = static_cast<Context*>(userdata);
    auto& serverHandle = context->GetDebuggerServer();
    assert(serverHandle.handle != nullptr && "Debugger server is not initialized.");
    auto* server = static_cast<qjs::detail::DebuggerServer*>(serverHandle.handle);
    // break hit?
    bool block = false;
    std::string filename;
    {
        const char* s = JS_AtomToCString(ctx, file_name);
        filename = s;
        JS_FreeCString(ctx, s);
    }
    for (const auto& bp : context->GetBreakPoints())
    {
        if (bp.line == line_no && bp.filename == filename)
        {
            block = true;
            break;
        }
    }
    if (block)
    {
        server->SendMessage(detail::BreakHit{filename, line_no});
    }
    auto handleCommand = [&](const auto& cmd) {
        using CmdType = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<CmdType, detail::BreakLine>)
        {
            context->GetBreakPoints().push_back({cmd.fileName, cmd.line});
        }
        else if constexpr (std::is_same_v<CmdType, detail::Continue>)
        {
            block = false;
        }
        else if constexpr (std::is_same_v<CmdType, detail::ReadVariable>)
        {
            std::string code=std::format("JSON.stringify({})",cmd.name);
            JSValue var = JS_Eval(context->GetRaw(), code.c_str(), code.size(), "<input>", JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(var))
            {
                server->SendMessage(detail::Variable{cmd.name, "undefined"});
            }
            else
            {
                const char* value = JS_ToCString(ctx, var);
                if (value)
                {
                    server->SendMessage(detail::Variable{cmd.name, value});
                    JS_FreeCString(ctx, value);
                }
                else
                {
                    server->SendMessage(detail::Variable{cmd.name, "error converting to string"});
                }
                JS_FreeValue(ctx, var);
            }
        }
        else
        {
        }
    };
    while (server->HasCommand())
    {
        auto cmd = server->PopCommand();
        std::visit(handleCommand, cmd);
    }
    while (block && server->IsRunning())
    {
        server->GetCommandSemaphore().Wait();
        while (server->HasCommand())
        {
            auto cmd = server->PopCommand();
            std::visit(handleCommand, cmd);
        }
    }
    return true;
}

} // namespace
namespace qjs
{
DebuggerServerHandle::~DebuggerServerHandle()
{
    Destroy();
}
void DebuggerServerHandle::Destroy()
{
    if (handle)
    {
        qjs::detail::DebuggerServer* server = static_cast<qjs::detail::DebuggerServer*>(handle);
        delete server; // 假设handle是new出来的
        handle = nullptr;
    }
}
void Context::SetDebuggerMode(int mode)
{
#ifdef CONFIG_DEBUGGER
    JS_SetDebuggerMode(m_Context, mode);
    if (mode)
    {
        JS_SetBreakpointHandler(m_Context, debug_handler);
        g_DebugUserdata[m_Context] = this; // 设置用户数据
        m_Server.Destroy();
        auto* server = new qjs::detail::DebuggerServer();
        m_Server.handle = server;
        m_ServerThread = std::make_unique<std::thread>(
            [server]() {
                server->Run();
            });
    }
    else
    {
        JS_SetBreakpointHandler(m_Context, nullptr);
        g_DebugUserdata.erase(m_Context); // 清除用户数据
        if (m_Server.handle)
        {
            auto* server = static_cast<qjs::detail::DebuggerServer*>(m_Server.handle);
            server->Shutdown();
            m_ServerThread->join();
            m_Server.Destroy();
            m_ServerThread.reset();
        }
    }
#else
    assert(false && "QuickJS is not compiled with debugger support. Please enable CONFIG_DEBUGGER in your QuickJS build.");
#endif
}

} // namespace qjs