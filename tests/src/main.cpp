#include <quickjspp/quickjspp.h>
#include <print>
#include <quickjspp/detail/debugger/debugger_server.h>
#include <thread>
#include <tuple>
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
    global_obj.SetPropertyStr("print", std::move(func));
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
    global_obj.SetPropertyStr("print", std::move(func));

    qjs::Value sleep_func = context.CreateFunction(js_sleep, "sleep", 1);
    global_obj.SetPropertyStr("sleep", std::move(sleep_func));
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
    global_obj.SetPropertyStr("print", std::move(func));

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
    context.GetGlobalObject().SetPropertyStr("test", std::move(closure));
    context.Eval("test()");
    qjs::Value closure1 = context.CreateClosure([a](uint32_t x, const std::string& msg) {
        std::println("Closure1 called with a={} x={} and msg={}", a, x, msg);
        return x + a;
    });
    context.GetGlobalObject().SetPropertyStr("test1",std::move( closure1));
    qjs::Value res = context.Eval("let res=test1(5,'hello');print(res);");
}
void test_exception()
{
    // init context
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();
    // register function
    auto global_obj = context.GetGlobalObject();
    auto test = context.CreateClosure([]() {
        std::print("test closure called\n");
        return qjs::Exception{"This is a test exception"};
    });
    global_obj.SetPropertyStr("test", std::move(test));
    std::string code = R"(

Error.prototype.toJSON = function () {
  return {
    name: this.name,
    message: this.message,
    stack: this.stack
  };
};

    try{
    test();
    }
    catch(e){
    console.log("Caught exception:", JSON.stringify(e,null,4));
    }
    finally{
    console.log("Finally block executed");
    }
    )";
    context.Eval(code.c_str(), code.size(), "<input>");
}
struct Vec3f
{
    float x, y, z;
    float Norm() const
    {
        return std::sqrt(x * x + y * y + z * z);
    }
    float TestFunction(int32_t a, float b, uint32_t c, std::string& d)
    {
        std::print("TestFunction called with param: {} {} {} {}\n", a, b, c, d);
        return a + b;
    }
};


struct Vec4f
{
    float x, y, z, w;
    void CopyFromVec3f(const Vec3f& v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }
};

void test_class()
{
    // init context
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    {
qjs::ClassRegistry<Vec3f> registry;

    registry.Begin("Vec3f")
        .Property("x", [](Vec3f& t) { return t.x; }, [](Vec3f& t, float v) { t.x = v; })
        .Property("y", [](Vec3f& t) { return t.y; }, [](Vec3f& t, float v) { t.y = v; })
        .Property("z", [](Vec3f& t) { return t.z; }, [](Vec3f& t, float v) { t.z = v; })
        .Method("Norm", [](Vec3f& t) { return t.Norm(); })
        .Method("TestFunction", [](Vec3f& t, int a, float b, uint32_t c, std::string& d) { return t.TestFunction(a, b, c, d); })
        .End();
    }
    
    {
    qjs::ClassRegistry<Vec4f>  registry;
    registry.Begin("Vec4f")
        .Property("x", [](Vec4f& t) { return t.x; }, [](Vec4f& t, float v) { t.x = v; })
        .Property("y", [](Vec4f& t) { return t.y; }, [](Vec4f& t, float v) { t.y = v; })
        .Property("z", [](Vec4f& t) { return t.z; }, [](Vec4f& t, float v) { t.z = v; })
        .Property("w", [](Vec4f& t) { return t.w; }, [](Vec4f& t, float v) { t.w = v; })
        .Method("CopyFromVec3f", [](Vec4f& t, Vec3f& v) { t.CopyFromVec3f(v); })
        .End();
    }
    qjs::Context context = qjs::Context::Create(runtime).value();
    {
std::string code = R"(
    try{
    let v=new Vec3f();
    v.x=1.0;
    v.y=2.0;
    v.z=3.0;
    console.log("call Norm:",v.Norm());
    console.log("call TestFunction:",v.TestFunction(1,2.0,3,"hello"));
    }
    catch(e){
        console.log("Caught exception:", e,e.stack);
    }
    )";
    context.Eval(code.c_str(), code.size(), "<input>");
    }
    {
        std::string code = R"(
    try{
    let v4=new Vec4f();
    let v3=new Vec3f();
    v3.x=1.0;
    v3.y=2.0;
    v3.z=3.0;
    v4.CopyFromVec3f(v3);
    console.log("Vec4f after CopyFromVec3f:",v4.x,v4.y,v4.z,v4.w);
    }
    catch(e){
        console.log("Caught exception:", e,e.stack);
    }
    )";
    context.Eval(code.c_str(), code.size(), "<input>");
    }
    {
        std::string code = R"(
        let v1=new Vec3f();
        let v2=new Vec3f();
        v1.x=1.0;
        v1.y=2.0;
        v1.z=3.0;
        v2.x=4.0;
        v2.y=5.0;
        v2.z=6.0;
        console.log("v1:",v1.x,v1.y,v1.z);
        console.log("v2:",v2.x,v2.y,v2.z);
    )";
context.Eval(code.c_str(), code.size(), "<input>");
    }
    
}
struct Timer
{
    std::chrono::steady_clock::time_point start_time;
    Timer() :
        start_time(std::chrono::steady_clock::now())
    {
    }
    void Reset()
    {
        start_time = std::chrono::steady_clock::now();
    }
    double ElapsedSeconds() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    }
};
void benchmark_class()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::ClassRegistry<Vec3f> registry;

    registry.Begin("Vec3f")
        .Property("x", [](Vec3f& t) { return t.x; }, [](Vec3f& t, float v) { t.x = v; })
        .Property("y", [](Vec3f& t) { return t.y; }, [](Vec3f& t, float v) { t.y = v; })
        .Property("z", [](Vec3f& t) { return t.z; }, [](Vec3f& t, float v) { t.z = v; })
        .Method("Norm", [](Vec3f& t) { return t.Norm(); })
        .End();
    {
        std::print("Benchmarking Vec3f class Norm function call and property set\n");
        double native_time = 0;
        {
            Timer timer;
            Vec3f v;
            for (int i = 0; i < 1000000; ++i)
            {
                v.x = 1.0f;
                v.y = 1.0f;
                v.z = 1.0f;
                auto norm = v.Norm();
            }
            native_time = timer.ElapsedSeconds();
        }
        std::print("Native time: {} seconds\n", native_time);
        qjs::Context context = qjs::Context::Create(runtime).value();
        double js_time = 0;
        {
            Timer timer;
            std::string code = R"(
        try
        {
        let v=new Vec3f();
        for (let i = 0; i < 1000000; ++i) {
            
            v.x=1.0;
            v.y=1.0;
            v.z=1.0;
            let norm = v.Norm();
        }
        }

        catch(e)
        {
            console.log("Caught exception:", e,e.stack);
        }
        )";
            context.Eval(code.c_str(), code.size(), "<input>");
            js_time = timer.ElapsedSeconds();
        }
        std::print("JS time: {} seconds\n", js_time);
        std::print("js/native ratio: {}\n", js_time / native_time);
    }
    {
        std::print("Benchmarking Vec3f class Norm function call ,class constructor and property set\n");
        double native_time = 0;
        {
            Timer timer;

            for (int i = 0; i < 1000000; ++i)
            {
                Vec3f v;
                v.x = 1.0f;
                v.y = 1.0f;
                v.z = 1.0f;
                auto norm = v.Norm();
            }
            native_time = timer.ElapsedSeconds();
        }
        std::print("Native time: {} seconds\n", native_time);
        qjs::Context context = qjs::Context::Create(runtime).value();
        double js_time = 0;
        {
            Timer timer;
            std::string code = R"(
        try
        {
        for (let i = 0; i < 1000000; ++i) {
            let v=new Vec3f();
            v.x=1.0;
            v.y=1.0;
            v.z=1.0;
            let norm = v.Norm();
        }
        }
        catch(e)
        {
            console.log("Caught exception:", e,e.stack);
        }
        
        )";
            context.Eval(code.c_str(), code.size(), "<input>");
            js_time = timer.ElapsedSeconds();
        }
        std::print("JS time: {} seconds\n", js_time);
        std::print("js/native ratio: {}\n", js_time / native_time);
    }
}
void test_closure2()
{
    qjs::ClassRegistry<Vec3f> registry;
    registry.Begin("Vec3f")
        .Property("x", [](Vec3f& t) { return t.x; }, [](Vec3f& t, float v) { t.x = v; })
        .Property("y", [](Vec3f& t) { return t.y; }, [](Vec3f& t, float v) { t.y = v; })
        .Property("z", [](Vec3f& t) { return t.z; }, [](Vec3f& t, float v) { t.z = v; })
        .Method("Norm", [](Vec3f& t) { return t.Norm(); })
        .End();
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();
    

    
    auto closure=context.CreateClosure([](Vec3f& v){
        std::println("x {} y {} z {}",v.x, v.y, v.z);
    });
    context.GetGlobalObject().SetPropertyStr("PrintVec3f", std::move(closure));
    std::string code = R"(
    try{
    let v=new Vec3f();
    v.x=1.0;
    v.y=2.0;
    v.z=3.0;
    PrintVec3f(v);
    }
    catch(e){
        console.log("Caught exception:", e,e.stack);
    }
    )";
    context.Eval(code.c_str(), code.size(), "<input>");
}
void test_js_function_call()
{
    qjs::ClassRegistry<Vec3f> registry;
    registry.Begin("Vec3f")
        .Property("x", [](Vec3f& t) { return t.x; }, [](Vec3f& t, float v) { t.x = v; })
        .Property("y", [](Vec3f& t) { return t.y; }, [](Vec3f& t, float v) { t.y = v; })
        .Property("z", [](Vec3f& t) { return t.z; }, [](Vec3f& t, float v) { t.z = v; })
        .Method("Norm", [](Vec3f& t) { return t.Norm(); })
        .End();
    qjs::Runtime runtime=qjs::Runtime::Create().value();
    qjs::Context context=qjs::Context::Create(runtime).value();
    auto global_obj = context.GetGlobalObject();
    std::string code=R"(
    function print(s)
    {
        console.log(s);
    }
    function add(a, b)
    {
        return a + b;
    }
    function printVec3f(v)
    {
        console.log("x: " + v.x + ", y: " + v.y + ", z: " + v.z);
    }
    )";
    context.Eval(code.c_str(), code.size(), "<input>");
    // 获取函数对象
    auto print=global_obj.GetProperty("print");
    print.Call(std::string("string from c++"));
    auto add=global_obj.GetProperty("add");
    auto res=add.Call(1, 2);
    std::println("add result: {}",res.Convert<int>());
    
    Vec3f v;
    v.x = 1.0f;
    v.y = 2.0f;
    v.z = 3.0f;
    auto v_ref=context.Eval(R"(
Object.create(Vec3f.prototype);
        )");

}
int main()
{
    NetContext::Init();
    // test_eval();
    // test_debugger_server();
    // test_debug();
    // test_builtin();
    // test_closure();
    // test_exception();
    //test_class();
    // benchmark_class();
    //test_closure2();
    test_js_function_call();
    NetContext::Cleanup();
    return 0;
}
