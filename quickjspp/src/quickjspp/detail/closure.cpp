#include "closure.h"
#include <cstdint>
#include <quickjs.h>
namespace qjs::detail
{
struct JsClosure
{
    int64_t ptr; // Closure的指针
};
// 全局 class id
thread_local static JSClassID g_JsClosureClassId;

// 析构函数：JS GC 调用
static void JsClosure_Finalizer(JSRuntime* rt, JSValue val)
{
    JsClosure* obj = (JsClosure*)JS_GetOpaque(val, g_JsClosureClassId);
    if (obj)
    {
        delete obj;
    }
}

// 初始化一次：注册 class
void InitJsClosureClass(JSRuntime* rt)
{
    JS_NewClassID(&g_JsClosureClassId);

    JSClassDef def = {
        .class_name = "JsClosure",
        .finalizer = JsClosure_Finalizer,
    };
    JS_NewClass(rt, g_JsClosureClassId, &def);
}
static JSValue CreateJsClosure(JSContext* ctx, Closure* closure)
{
    // 创建 JsClosure 对象
    JsClosure* obj = new JsClosure();
    obj->ptr = reinterpret_cast<int64_t>(closure);

    // 创建 JSValue
    JSValue js_val = JS_NewObjectClass(ctx, g_JsClosureClassId);
    if (JS_IsException(js_val))
    {
        delete obj; // 发生异常时清理内存
        return js_val;
    }

    // 设置 Opaque 数据
    JS_SetOpaque(js_val, obj);
    return js_val;
}
static JSValue CallClosure(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic, JSValue* func_data)
{
    auto* jsclosure=(JsClosure*)JS_GetOpaque(func_data[0], g_JsClosureClassId);
    if (!jsclosure)
    {
        return JS_EXCEPTION;
    }
    Closure* closure = reinterpret_cast<Closure*>(jsclosure->ptr);
    return (*closure)(ctx, this_val, argc, argv);
}
JSValue CreateClosure(JSContext* ctx, Closure* closure)
{
    JSValue jsClosure=CreateJsClosure(ctx, closure);
    auto res=JS_NewCFunctionData(ctx, CallClosure, 1, 0, 1, &jsClosure);
    JS_FreeValue(ctx, jsClosure); // 释放 jsClosure 的引用
    return res;
}
} // namespace qjs::detail