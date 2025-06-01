#include <quickjspp/quickjspp.h>
#include <print>
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
JS_BOOL debug_handler(JSContext *ctx, JSAtom file_name, uint32_t line_no, const uint8_t *pc)
{
std::print("lint: {}",line_no);
return true;
}
void test_eval()
{
    qjs::Runtime runtime = qjs::Runtime::Create().value();
    qjs::Context context = qjs::Context::Create(runtime).value();

    // 创建 JS 函数对象，3 表示最多参数个数（任意写）
    qjs::Value func = context.CreateFunction( js_print, "print", 1);
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
int main()
{
    
    return 0;
}
