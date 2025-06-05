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
//static void EnableCreator1(JSContext* ctx)
//{
//    JSValue global = JS_GetGlobalObject(ctx);
//    for (size_t i = 0; i < g_RegisteredClasses.size(); ++i)
//    {
//        auto& clazz = g_RegisteredClasses[i];
//        Closure* creator = new Closure([i, ctx, clazz](JSContext*,
//                                                       JSValueConst /*this*/,
//                                                       int /*argc*/,
//                                                       JSValueConst* /*argv*/) -> JSValue {
//            void* handle = clazz.constructor();
//            JSValue obj = CreateJsClass(ctx, new JsClass{.classIndex = static_cast<uint32_t>(i), .handle = handle});
//            // set type name
//            {
//                JSValue type = JS_NewString(ctx, clazz.className.c_str());
//                JS_SetPropertyStr(ctx, obj, "Type", type);
//            }
//            // set getter setter
//            for (size_t j = 0; j < clazz.properties.size(); ++j)
//            {
//                // getter
//                Closure* getter = new Closure([j, clazz](JSContext* context,
//                                                         JSValueConst /*this*/ this_val,
//                                                         int /*argc*/ argc,
//                                                         JSValueConst* /*argv*/ argv) -> JSValue {
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    return clazz.getter[j](context, obj->handle);
//                });
//                JSValue js_getter = CreateClosure(ctx, getter);
//                std::string getterName = std::string("Get") + clazz.properties[j];
//                JS_SetPropertyStr(ctx, obj, getterName.c_str(), js_getter);
//                // setter
//                Closure* setter = new Closure([j, clazz](JSContext* context,
//                                                         JSValueConst /*this*/ this_val,
//                                                         int /*argc*/ argc,
//                                                         JSValueConst* /*argv*/ argv) -> JSValue {
//                    if(argc!=1)
//                    {
//                        return JS_ThrowTypeError(context, "setter must set one param");
//                    }
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    clazz.setter[j](context,obj->handle,argv[0]);
//                    return JS_UNDEFINED;
//                });
//                JSValue js_setter=CreateClosure(ctx, setter);
//                std::string setterName = std::string("Set") + clazz.properties[j];
//                JS_SetPropertyStr(ctx, obj, setterName.c_str(), js_setter);
//            }
//            // set method
//            for(auto&  [name,func]:clazz.methods)
//            {
//                Closure* c=new Closure([func](JSContext* context,
//                                      JSValueConst /*this*/ this_val,
//                                      int /*argc*/ argc,
//                                      JSValueConst* /*argv*/ argv)->JSValue{
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    return func(context,obj->handle,argc,argv);
//                });
//                JSValue js_c=CreateClosure(ctx, c);
//                JS_SetPropertyStr(ctx, obj, name.c_str(), js_c);
//            }
//            return obj;
//        });
//        JSValue js_creator = CreateClosure(ctx, creator);
//        //std::string creatorName = std::string("Create") + clazz.className;
//        JS_SetPropertyStr(ctx, global,  clazz.className.c_str(), js_creator);
//    }
//    JS_FreeValue(ctx, global);
//}
//static void EnableCreator2(JSContext* ctx)
//{
//    JSValue global = JS_GetGlobalObject(ctx);
//    for (size_t i = 0; i < g_RegisteredClasses.size(); ++i)
//    {
//        auto& clazz = g_RegisteredClasses[i];
//        JSValue proto;
//        {
//            void* handle = clazz.constructor();
//            JSValue obj = CreateJsClass(ctx, new JsClass{.classIndex = static_cast<uint32_t>(i), .handle = handle});
//            // set type name
//            {
//                JSValue type = JS_NewString(ctx, clazz.className.c_str());
//                JS_SetPropertyStr(ctx, obj, "Type", type);
//            }
//            // set getter setter
//            for (size_t j = 0; j < clazz.properties.size(); ++j)
//            {
//                // getter
//                Closure* getter = new Closure([j, clazz](JSContext* context,
//                                                         JSValueConst /*this*/ this_val,
//                                                         int /*argc*/ argc,
//                                                         JSValueConst* /*argv*/ argv) -> JSValue {
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    return clazz.getter[j](context, obj->handle);
//                });
//                JSValue js_getter = CreateClosure(ctx, getter);
//                std::string getterName = std::string("Get") + clazz.properties[j];
//                JS_SetPropertyStr(ctx, obj, getterName.c_str(), js_getter);
//                // setter
//                Closure* setter = new Closure([j, clazz](JSContext* context,
//                                                         JSValueConst /*this*/ this_val,
//                                                         int /*argc*/ argc,
//                                                         JSValueConst* /*argv*/ argv) -> JSValue {
//                    if(argc!=1)
//                    {
//                        return JS_ThrowTypeError(context, "setter must set one param");
//                    }
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    clazz.setter[j](context,obj->handle,argv[0]);
//                    return JS_UNDEFINED;
//                });
//                JSValue js_setter=CreateClosure(ctx, setter);
//                std::string setterName = std::string("Set") + clazz.properties[j];
//                JS_SetPropertyStr(ctx, obj, setterName.c_str(), js_setter);
//            }
//            // set method
//            for(auto&  [name,func]:clazz.methods)
//            {
//                Closure* c=new Closure([func](JSContext* context,
//                                      JSValueConst /*this*/ this_val,
//                                      int /*argc*/ argc,
//                                      JSValueConst* /*argv*/ argv)->JSValue{
//                    JsClass* obj = (JsClass*)JS_GetOpaque(this_val, g_JsClassClassId);
//                    return func(context,obj->handle,argc,argv);
//                });
//                JSValue js_c=CreateClosure(ctx, c);
//                JS_SetPropertyStr(ctx, obj, name.c_str(), js_c);
//            }
//            proto = obj;
//        }
//        Closure* ctor=new Closure([](JSContext* context,
//                                      JSValueConst /*this*/ this_val,
//                                      int /*argc*/ argc,
//                                      JSValueConst* /*argv*/ argv)->JSValue{
//            // 获取 prototype
//    JSValue proto = JS_GetPropertyStr(context, this_val, "prototype");
//    // 手动创建新对象
//    JSValue obj = JS_NewObjectProto(context, proto);
//    
//    // 设置成员（等效于 this.x = 123）
//    JS_SetPropertyStr(context, obj, "x", JS_NewInt32(context, 123));
//                                        return obj;
//        });
//        JSValue js_ctor = CreateClosure(ctx, ctor);
//        JS_SetConstructor(ctx, js_ctor, proto);
//        JS_SetPropertyStr(ctx, global,  clazz.className.c_str(), js_ctor);
//        JS_FreeValue(ctx, proto);
//    }
//    JS_FreeValue(ctx, global);
//}

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
        JS_SetConstructor(ctx, js_ctor, proto);
        JS_SetPropertyStr(ctx, js_ctor, "prototype", proto);
        //JS_FreeValue(ctx, proto);
        JSValue global= JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, clazz.className.c_str(), js_ctor);
        JS_FreeValue(ctx, global);
    }
    
}

} // namespace qjs::detail