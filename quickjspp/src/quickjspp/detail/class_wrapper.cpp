#include "class_wrapper.h"
#include "closure.h"
#include "quickjs.h"
#include <print>
#include <vector>
#include <cassert>
namespace qjs::detail
{
thread_local static JSClassID g_JsClassClassId;
thread_local static std::vector<Class> g_RegisteredClasses; // 全局注册的类列表
struct JsClass
{
    uint32_t classIndex;
    void* handle;
    // 是否拥有 handle 的所有权,如果为true,对象由 JS GC 管理，析构时会调用 destructor
    // 如果为 false, 则 handle 由 C++ 管理，js gc析构时不会调用 destructor
    bool owned = true; 
};
// 析构函数：JS GC 调用
static void JsClass_Finalizer(JSRuntime* rt, JSValue val)
{
    JsClass* obj = (JsClass*)JS_GetOpaque(val, g_JsClassClassId);
    if (obj->handle&&obj->owned)
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
uint32_t RegisterClass(Class&& clazz)
{
    g_RegisteredClasses.push_back(std::move(clazz));
    return static_cast<uint32_t>(g_RegisteredClasses.size() - 1);
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


static JSValue DummyConstructor(JSContext* ctx, JSValueConst this_val,
                      int argc, JSValueConst* argv)
{
    JSValue proto = JS_GetPropertyStr(ctx, this_val,"prototype");
    JSValue js_class_index = JS_GetPropertyStr(ctx, proto, "__c_class_index");
    uint32_t class_index;
    if (JS_ToUint32(ctx, &class_index, js_class_index) < 0)
    {
        JS_FreeValue(ctx, js_class_index);
        JS_FreeValue(ctx, proto);
        return JS_EXCEPTION; // 转换失败
    }
    JS_FreeValue(ctx, js_class_index);


    JSValue obj = JS_NewObjectProtoClass(ctx, proto,g_JsClassClassId); // 或用 JS_NewObjectProto
    JS_FreeValue(ctx, proto);
    auto& clazz = g_RegisteredClasses[class_index];
    JsClass* c_this = new JsClass{.classIndex = class_index, .handle = clazz.constructor()};
    JS_SetOpaque(obj, c_this);
    return obj;
}
void EnableCreator(JSContext* ctx)
{
    
    // register class
    for(size_t classIndex=0;classIndex<g_RegisteredClasses.size();++classIndex)
    {
        auto& clazz=g_RegisteredClasses[classIndex];
        if(!(clazz.properties.size()==clazz.setter.size() && clazz.properties.size()==clazz.getter.size()))
        {
            assert(false && "properties, getter and setter size must be equal");
        }
        JSValue js_ctor = JS_NewCFunction2(ctx, DummyConstructor, "DummyConstructor", 1, JS_CFUNC_constructor, 0);
        // create prototype(method and properties getter/setter)
        JSValue proto = JS_NewObject(ctx);
        
        
        // set getter setter 
        for(size_t propertyIndex=0;propertyIndex<clazz.properties.size();++propertyIndex)
        {
            auto& propertyName=clazz.properties[propertyIndex];
            auto& getter=clazz.getter[propertyIndex];
            auto& setter=clazz.setter[propertyIndex];
            // getter
            Closure* closure_getter=new Closure([getter](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                return getter(context, obj->handle);
            });
            JSValue js_getter = CreateClosure(ctx, closure_getter);
            // getter
            Closure* closure_setter=new Closure([setter](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                if(argc!=1)
                {
                    return JS_ThrowTypeError(context, "setter must set one param");
                }
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                setter(context,obj->handle,argv[0]);
                return JS_UNDEFINED;
            });
            JSValue js_setter=CreateClosure(ctx, closure_setter);
            JSAtom propertyNameAtom = JS_NewAtom(ctx, propertyName.c_str());
            JS_DefineProperty(ctx, proto, propertyNameAtom, JS_UNDEFINED, js_getter,js_setter, JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, propertyNameAtom);
            JS_FreeValue(ctx, js_getter);
            JS_FreeValue(ctx, js_setter);
        }
        // set methods
        for(auto& [name, func]:clazz.methods)
        {
            Closure* closure_method=new Closure([func](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                return func(context,obj->handle,argc,argv);
            });
            JSValue js_method=CreateClosure(ctx, closure_method);
            JS_SetPropertyStr(ctx, proto, name.c_str(), js_method);
            //JS_FreeValue(ctx, js_method);
        }
        // store class index in prototype
        JS_SetPropertyStr(ctx, proto, "__c_class_index", JS_NewUint32(ctx, classIndex));
        // set opaque getter setter
        {
            Closure* closure_get_opaque = new Closure([](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                return JS_NewInt64(context, (int64_t)obj->handle);
            });
            JSValue js_get_opaque = CreateClosure(ctx, closure_get_opaque);
            Closure* closure_set_opaque = new Closure([](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                if(argc!=1)
                {
                    return JS_ThrowTypeError(context, "setOpaque must set one param");
                }
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                int64_t handle;
                if (JS_ToInt64(context, &handle, argv[0]) < 0)
                {
                    return JS_EXCEPTION; // 转换失败
                }
                obj->handle = reinterpret_cast<void*>(handle);
                return JS_UNDEFINED;
            });
            JSValue js_set_opaque = CreateClosure(ctx, closure_set_opaque);
            JSAtom opaque_atom = JS_NewAtom(ctx, "__c_opaque");
            JS_DefineProperty(ctx, proto, opaque_atom, JS_UNDEFINED, js_get_opaque, js_set_opaque, JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, opaque_atom);
            JS_FreeValue(ctx, js_get_opaque);
            JS_FreeValue(ctx, js_set_opaque);
        }
        // set owned getter setter
        {
            Closure* closure_get_owned = new Closure([](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                return JS_NewBool(context, obj->owned);
            });
            JSValue js_get_owned = CreateClosure(ctx, closure_get_owned);
            Closure* closure_set_owned = new Closure([](JSContext* context,
                                      JSValueConst /*this*/ this_val,
                                      int /*argc*/ argc,
                                      JSValueConst* /*argv*/ argv)->JSValue{
                if(argc!=1)
                {
                    return JS_ThrowTypeError(context, "setOwned must set one param");
                }
                JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
                bool owned=JS_ToBool(context, argv[0]);
                obj->owned = owned; // 设置 owned 属性
                return JS_UNDEFINED; // 返回 undefined
            });
            JSValue js_set_owned = CreateClosure(ctx, closure_set_owned);
            JSAtom owned_atom = JS_NewAtom(ctx, "__c_owned");
            JS_DefineProperty(ctx, proto, owned_atom, JS_UNDEFINED, js_get_owned, js_set_owned, JS_PROP_HAS_GET | JS_PROP_HAS_SET | JS_PROP_ENUMERABLE);
            JS_FreeAtom(ctx, owned_atom);
            JS_FreeValue(ctx, js_get_owned);
            JS_FreeValue(ctx, js_set_owned);
        }

        JS_SetConstructor(ctx, js_ctor, proto);
        JS_SetPropertyStr(ctx, js_ctor, "prototype", proto);
        //JS_FreeValue(ctx, proto);
        JSValue global= JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, clazz.className.c_str(), js_ctor);
        JS_FreeValue(ctx, global);
    }
    
}
bool IsRegisteredClass(JSContext* ctx,JSValue val)
{
    JSValue js_class_index = JS_GetPropertyStr(ctx, val, "__c_class_index");
    if(JS_IsUndefined(js_class_index))
    {
        JS_FreeValue(ctx, js_class_index);
        return false; // 不是注册的类
    }
    JS_FreeValue(ctx, val);
    return true;
}
void* GetOpaque(JSContext* ctx,JSValue val)
{
    JsClass* obj = (JsClass*)JS_GetOpaque(val, g_JsClassClassId);
    if(!obj)
    {
        assert(false && "GetOpaque failed, val is not a JsClass");
        return nullptr;
    }
    return obj->handle;
}
uint32_t GetClassIndex(const char* className)
{
    for (size_t i = 0; i < g_RegisteredClasses.size(); ++i)
    {
        if (g_RegisteredClasses[i].className == className)
        {
            return static_cast<uint32_t>(i);
        }
    }
    assert(false && "Class not found");
    return UINT32_MAX; // 如果没有找到，返回一个无效的索引
}

} // namespace qjs::detail