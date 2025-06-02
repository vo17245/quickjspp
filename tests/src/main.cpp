#include <quickjspp/quickjspp.h>
#include <print>
#include <quickjspp/detail/debugger/debugger_server.h>
#include <thread>
#include "net_context.h"
#include "quickjs.h"
#include "quickjs-libc.h"
// 1. C 函数实现，参数和返回值都是 JSValue
static JSValue js_print(JSContext* ctx, JSValueConst this_val, int argc,
                        JSValueConst* argv)
{
    for (int i = 0; i < argc; i++)
    {
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str)
        {
            printf("%s", str);
            JS_FreeCString(ctx, str);
        }
        if (i != argc - 1)
            printf(" ");
    }
    printf("\n");
    return JS_UNDEFINED;
}
static JSValue js_sleep(JSContext* ctx, JSValueConst this_val, int argc,
                        JSValueConst* argv)
{
    if (argc != 1)
    {
        std::print("js_sleep: expected 1 argument, got {}\n", argc);
        return JS_EXCEPTION;
    }
    int64_t ms;
    if (JS_ToInt64(ctx, &ms, argv[0]) < 0)
    {
        std::print("js_sleep: argument is not a valid number\n");
        return JS_EXCEPTION;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return JS_UNDEFINED;
}
JS_BOOL debug_handler(JSContext* ctx, JSAtom file_name, uint32_t line_no, const uint8_t* pc)
{
    std::print("lint: {}", line_no);
    return true;
}
void test_eval()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();

    // 创建 JS 函数对象，3 表示最多参数个数（任意写）
    qjs::Value func = context.CreateFunction(js_print, "print", 1);
    auto global_obj = context.GetGlobalObject();
    global_obj.SetPropertyStr("print", func);
    JS_SetBreakpointHandler(context.GetRaw(), debug_handler);
    JS_SetDebuggerMode(context.GetRaw(), 1);
    std::string code = R"(
    let a = 1;
    print("Hello, QuickJS!");
    print("The value of a is:", a);
    )";
    {
        qjs::Value res = context.Eval(code.c_str(), code.size(), "<input>");
        auto s = res.Convert<std::string>();
        std::println("Result: {}", s);
    }
}
void test_debugger_server()
{
    qjs::detail::DebuggerServer server;
    std::thread t([&server]() {
        server.Run();
    });

    while (server.IsRunning())
    {
        server.GetCommandSemaphore().Wait();
        auto command = server.PopCommand();
        std::println("Received command: {}", command.index());
    }
    t.join();
}
void test_debug()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();
    auto global_obj = context.GetGlobalObject();
    qjs::Value func = context.CreateFunction(js_print, "print", 1);
    global_obj.SetPropertyStr("print", func);

    qjs::Value sleep_func = context.CreateFunction(js_sleep, "sleep", 1);
    global_obj.SetPropertyStr("sleep", sleep_func);
    context.SetDebuggerMode(1);
    std::string code = R"(let a=0;
    while(true){
print("Hello, Debugger! ",a);
a++;
sleep(1000); // 每秒打印一次
}
)";
    context.Eval(code.c_str(), code.size(), "test_debug.js");
}

void test_builtin()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();

    // 创建 JS 函数对象，3 表示最多参数个数（任意写）
    qjs::Value func = context.CreateFunction(js_print, "print", 1);
    auto global_obj = context.GetGlobalObject();
    global_obj.SetPropertyStr("print", func);

    std::string code = R"(
    let a = 1;
    print(JSON);
    print(JSON.stringify(a));
    JSON.stringify({a:1,b:"1"})
    )";
    {
        qjs::Value res = context.Eval(code.c_str(), code.size(), "<input>");
        auto s = res.Convert<std::string>();
        std::println("Result: {}", s);
    }
}
void test_closure()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();
    int a = 10;
    qjs::Value closure = context.CreateClosureNoWrap(
        [a](JSContext*,
            JSValueConst /*this*/,
            int /*argc*/,
            JSValueConst* /*argv*/) -> JSValue {
            std::println("Closure called with a={}", a);
            return JS_UNDEFINED;
        });
    context.GetGlobalObject().SetPropertyStr("test", closure);
    context.Eval("test()");
    qjs::Value closure1=context.CreateClosure([a](uint32_t x,const std::string& msg){
        std::println("Closure1 called with a={} x={} and msg={}", a, x,msg);
    });
    context.GetGlobalObject().SetPropertyStr("test1", closure1);
    qjs::Value res = context.Eval("test1(5,'hello')");
    
}
int main()
{
    NetContext::Init();
    // test_eval();
    // test_debugger_server();
    //test_debug();
    //test_builtin();
    test_closure();
    NetContext::Cleanup();
    return 0;
}
