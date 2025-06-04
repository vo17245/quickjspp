#include "class_wrapper.h"
#include "closure.h"
#include "quickjs.h"
#include <vector>
namespace qjs::detail
{
thread_local static JSClassID g_JsClassClassId;
thread_local static std::vector<Class> g_RegisteredClasses; // 全局注册的类列表
struct JsClass
{
    uint32_t classIndex;
    void* handle;
};
// 析构函数：JS GC 调用
static void JsClass_Finalizer(JSRuntime* rt, JSValue val)
{
    JsClass* obj = (JsClass*)JS_GetOpaque(val, g_JsClassClassId);
    if (obj->handle)
    {
        auto& clazz = g_RegisteredClasses[obj->classIndex];
        if (clazz.destructor)
        {
            clazz.destructor(obj->handle); // 调用析构函数
        }
    }
    if (obj)
    {
        delete obj;
    }
}
// 初始化一次：注册 class
void InitJsClassClass(JSRuntime* rt)
{
    JS_NewClassID(&g_JsClassClassId);

    JSClassDef def = {
        .class_name = "JsClass",
        .finalizer = JsClass_Finalizer,
    };
    JS_NewClass(rt, g_JsClassClassId, &def);
}
void RegisterClass(Class&& clazz)
{
    g_RegisteredClasses.push_back(std::move(clazz));
}
JSValue CreateJsClass(JSContext* ctx, JsClass* clazz)
{
    // 创建 JSValue
    JSValue js_val = JS_NewObjectClass(ctx, g_JsClassClassId);
    if (JS_IsException(js_val))
    {
        delete clazz; // 发生异常时清理内存
        return js_val;
    }

    // 设置 Opaque 数据
    JS_SetOpaque(js_val, clazz);
    return js_val;
}
void EnableCreator(JSContext* ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    for (size_t i = 0; i < g_RegisteredClasses.size(); ++i)
    {
        auto& clazz = g_RegisteredClasses[i];
        Closure* creator = new Closure([i, ctx, clazz](JSContext*,
                                                       JSValueConst /*this*/,
                                                       int /*argc*/,
                                                       JSValueConst* /*argv*/) -> JSValue {
            void* handle = clazz.constructor();
            JSValue obj = CreateJsClass(ctx, new JsClass{.classIndex = static_cast<uint32_t>(i), .handle = handle});
            // set type name
            {
                JSValue type = JS_NewString(ctx, clazz.className.c_str());
                JS_SetPropertyStr(ctx, obj, "Type", type);
            }
            // set getter setter
            for (size_t j = 0; j < clazz.properties.size(); ++j)
            {
                // getter
                Closure* getter = new Closure([j, clazz](JSContext* context,
                                                         JSValueConst /*this*/ this_val,
                                                         int /*argc*/ argc,
                                                         JSValueConst* /*argv*/ argv) -> JSValue {
                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                    return clazz.getter[j](context, obj->handle);
                });
                JSValue js_getter = CreateClosure(ctx, getter);
                std::string getterName = std::string("Get") + clazz.properties[j];
                JS_SetPropertyStr(ctx, obj, getterName.c_str(), js_getter);
                // setter
                Closure* setter = new Closure([j, clazz](JSContext* context,
                                                         JSValueConst /*this*/ this_val,
                                                         int /*argc*/ argc,
                                                         JSValueConst* /*argv*/ argv) -> JSValue {
                    if(argc!=1)
                    {
                        return JS_ThrowTypeError(context, "setter must set one param");
                    }
                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                    clazz.setter[j](context,obj->handle,argv[0]);
                    return JS_UNDEFINED;
                });
                JSValue js_setter=CreateClosure(ctx, setter);
                std::string setterName = std::string("Set") + clazz.properties[j];
                JS_SetPropertyStr(ctx, obj, setterName.c_str(), js_setter);
            }
            // set method
            for(auto&  [name,func]:clazz.methods)
            {
                Closure* c=new Closure([func](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                    return func(context,obj->handle,argc,argv);
                });
                JSValue js_c=CreateClosure(ctx, c);
                JS_SetPropertyStr(ctx, obj, name.c_str(), js_c);
            }
            return obj;
        });
        JSValue js_creator = CreateClosure(ctx, creator);
        std::string creatorName = std::string("Create") + clazz.className;
        JS_SetPropertyStr(ctx, global, creatorName.c_str(), js_creator);
    }
    JS_FreeValue(ctx, global);
}
} // namespace qjs::detail