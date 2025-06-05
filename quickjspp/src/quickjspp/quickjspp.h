#pragma once
#include "quickjs.h"
#include <cassert>
#include <optional>
#include <string>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>
#include <quickjs-libc.h>
#include "detail/bind.h"
#include <functional>
#include "detail/closure.h"
#include "detail/wrap_closure.h"
#include "detail/class_wrapper.h"
#include "quickjspp/detail/traits.h"
#include "detail/function_call.h"
#ifdef CONFIG_DEBUGGER
#ifndef QUICKJSPP_ENABLE_DEBUGGER
#define QUICKJSPP_ENABLE_DEBUGGER
#endif
#endif
namespace qjs
{


struct Exception
{
    std::string message;
};
namespace detail
{
template <>
struct ConvertToJsType<Exception>
{
    static JSValue Convert(JSContext* ctx, const Exception& e)
    {
        // throw
        JSValue message = JS_NewString(ctx, e.message.c_str());
        auto err = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, err, "message", message); // message 会被接管，不需要释放
        JS_Throw(ctx, err);
        return JS_EXCEPTION;
    }
};
} // namespace detail
struct DebuggerServerHandle
{
    void* handle = nullptr; // qjs::detail::DebuggerServer*
    ~DebuggerServerHandle();
    void Destroy();
};
using detail::Closure;


template <typename T>
class ClassRegistry
{
public:
    
    ClassRegistry& Begin(std::string_view className)
    {
        m_Class=detail::Class();
        m_Class.className= className;
        m_Class.constructor = []() -> void* {
            return new T();
        };
        m_Class.destructor = [](void* t) {
            delete static_cast<T*>(t);
        };
        return *this;
    }
    void End()
    {
        detail::RegisterClass(std::move(m_Class));
    }
    /**
     * usage:
     * struct Triangle
     * {
     *      Vec2f a,b,c;
     *      float CalculateArea();
     *      bool Contain(const Vec2f& p);
     * };
     * auto builder=ClassBuilder<Triangle>();
     * builder.Method("CalculateArea",[](Triangle& self){return self.CalculateArea();});
     * builder.Method("Contain",[](Triangle& self,const Vec2f& p){return self.Contain(p);});
    */
    
    template<typename F,typename U ,size_t... I>
    static inline auto apply_method_args_impl(F& f,T& self,U& args,std::index_sequence<I...>)
    {
        return f(self, detail::DereferenceIfRegistered<std::tuple_element_t<I, U>>{}(std::get<I>(args))...);
    }
    template<typename F,typename U>
    static inline auto apply_method_args(F& f,T& self,U& args)
    {
        return apply_method_args_impl(f,self,args,
            std::make_index_sequence<std::tuple_size_v<U>>{});
    }

    template<typename F>
    ClassRegistry& Method(std::string_view name,F&& func)
    {
        using Traits=detail::LambdaTraits<decltype(&std::remove_reference_t<F>::operator())>;
        using ReturnType=Traits::ReturnType;
        using AllArgsTuple=typename Traits::ArgsTuple;
        using Self=typename std::tuple_element_t<0, AllArgsTuple>;
        using Args=typename detail::TupleDecay<typename detail::SliceType<AllArgsTuple, 1, std::tuple_size_v<AllArgsTuple>>::Type>::type;
       
        static_assert(std::is_same_v<std::decay_t<Self>, T>, "Method must be a member function of the class T");
        detail::ClassMethod method=[func=std::move(func)](JSContext* ctx,void* c_this/*cpp this*/ ,int argc /*argc*/,JSValue* argv /*argv*/)->JSValue{
            Args args;
            if(!detail::ConvertArgs2<Args>{}(args,argv,ctx))
            {
                JS_ThrowTypeError(ctx, "Argument conversion failed");
                return JS_EXCEPTION; // 转换失败
            }
            if constexpr (std::is_same_v<ReturnType, void>)
            {
                apply_method_args(func, *(T*)c_this,args);
                return JS_UNDEFINED;
            }
            else
            {
                auto res = apply_method_args(func, *(T*)c_this,args);
                return detail::ConvertToJsType<ReturnType>::Convert(ctx, res);
            }
        };
        m_Class.methods.emplace_back(std::string(name), std::move(method));
        return *this;
    }
    /* usage:
    *   struct Entity{std::string tag};
    *   auto builder=ClassBuilder<Entity>();
    *   builder.Property("tag",
    *       [](Entity& self){return self.tag;},
    *       [](Entity& self,std::string& v){self.tag=v;}
    *   );
    */
    template<typename Getter,typename Setter>
    ClassRegistry& Property(std::string_view name, Getter&& getter,Setter&& setter)
    {
        using Traits=detail::LambdaTraits<decltype(&std::remove_reference_t<Getter>::operator())>;
        using PropertyType=Traits::ReturnType;
        m_Class.properties.push_back(std::string(name));
        //wrap getter
        auto _getter=[getter=std::move(getter)](JSContext* ctx,void* t){
            return detail::ConvertToJsType<PropertyType>::Convert(ctx,getter(*(T*)t));
        };
        m_Class.getter.push_back(std::move(_getter));
        // wrap setter
        auto _setter=[setter=std::move(setter)](JSContext* ctx,void*  t,JSValue v)
        {
            auto vOpt=detail::ConvertToCppType<PropertyType>::Convert(ctx,v);
            if(!vOpt)
            {
                assert(false && "Failed to convert JS value to C++ type");
                return;
            }
            PropertyType _v=*vOpt;
            setter(*(T*)t,_v);
        };
        m_Class.setter.push_back(std::move(_setter));
        return *this;
    }
private:
    detail::Class m_Class;
};
class Context;
class Value
{
public:
    Value(JSValue value, JSContext* context) :
        m_Value(value), m_Context(context)
    {
        assert(context != nullptr);
    }
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&& other) noexcept :
        m_Value(other.m_Value), m_Context(other.m_Context)
    {
        other.m_Value = JS_UNDEFINED;
        other.m_Context = nullptr;
    }
    Value& operator=(Value&& other) noexcept
    {
        if (this != &other)
        {
            if (m_Context && m_Value != JS_UNDEFINED)
                JS_FreeValue(m_Context, m_Value);
            m_Value = other.m_Value;
            m_Context = other.m_Context;
            other.m_Value = JS_UNDEFINED;
            other.m_Context = nullptr;
        }
        return *this;
    }
    ~Value()
    {
        if (m_Context && m_Value != JS_UNDEFINED)
            JS_FreeValue(m_Context, m_Value);
    }
    template <typename T>
    T Convert() const
    {
        static_assert(sizeof(T) == 0, "Convert function not implemented for this type");
    }
    JSValue GetRaw() const
    {
        return m_Value;
    }
    //@note value的所有权会被JS_SetPropertyStr接管
    void SetPropertyStr(const char* name, Value& value)
    {
        JS_SetPropertyStr(m_Context, m_Value, name, value.Unwrap());
    }
    JSValue Unwrap()
    {
        auto res = m_Value;
        m_Value = JS_UNDEFINED; // Prevent double free
        return res;
    }

private:
    JSValue m_Value = JS_UNDEFINED;
    JSContext* m_Context = nullptr;
    std::vector<std::function<void(void*)>> m_Destructors; // 用于存储析构函数
    std::vector<std::function<void*()>> m_Constructors;    // 用于存储构造函数
    std::vector<std::string> m_ClassName;
};
class Runtime
{
public:
    static std::optional<Runtime> Create()
    {
        Runtime rt;
        rt.m_Runtime = JS_NewRuntime();
        if (!rt.m_Runtime)
            return std::nullopt;
        js_std_init_handlers(rt.GetRaw());
        detail::InitJsClosureClass(rt.GetRaw());
        detail::InitJsClassClass(rt.GetRaw());

        return rt;
    }
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&& other) noexcept :
        m_Runtime(other.m_Runtime)
    {
        other.m_Runtime = nullptr;
    }
    Runtime& operator=(Runtime&& other) noexcept
    {
        if (this != &other)
        {
            if (m_Runtime)
                JS_FreeRuntime(m_Runtime);
            m_Runtime = other.m_Runtime;
            other.m_Runtime = nullptr;
        }
        return *this;
    }
    ~Runtime()
    {
        if (m_Runtime)
        {
            js_std_free_handlers(m_Runtime);
            JS_FreeRuntime(m_Runtime);
        }
    }
    JSRuntime* GetRaw() const
    {
        return m_Runtime;
    }

private:
    Runtime() = default;
    JSRuntime* m_Runtime = nullptr;
};

class Context
{
public:
    static std::optional<Context> Create(Runtime& runtime)
    {
        Context ctx;
        ctx.m_Context = JS_NewContext(runtime.GetRaw());
        if (!ctx.m_Context)
            return std::nullopt;
        js_init_module_std(ctx.m_Context, "std");
        js_init_module_os(ctx.m_Context, "os");
        js_std_add_helpers(ctx.m_Context, 0, nullptr);
        detail::EnableCreator(ctx.m_Context);
        return ctx;
    }
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&& other) noexcept :
        m_Context(other.m_Context)
    {
        other.m_Context = nullptr;
    }
    Context& operator=(Context&& other) noexcept
    {
        if (this != &other)
        {
            if (m_Context)
                JS_FreeContext(m_Context);
            m_Context = other.m_Context;
            other.m_Context = nullptr;
        }
        return *this;
    }
    ~Context()
    {
        if (m_Context)
            JS_FreeContext(m_Context);
    }
    JSContext* GetRaw() const
    {
        return m_Context;
    }
    Value Eval(const char* code, size_t length, const char* filename, int eval_flags = JS_EVAL_TYPE_GLOBAL)
    {
        JSValue result = JS_Eval(m_Context, code, length, filename, eval_flags);
        return Value(result, m_Context);
    }
    Value Eval(std::string_view code)
    {
        JSValue result = JS_Eval(m_Context, code.data(), code.length(), "<input>", JS_EVAL_TYPE_GLOBAL);
        return Value(result, m_Context);
    }
    Value GetGlobalObject()
    {
        JSValue global = JS_GetGlobalObject(m_Context);
        return Value(global, m_Context);
    }
    Value CreateFunction(JSCFunction* func, const char* name, int length)
    {
        JSValue function = JS_NewCFunction(m_Context, func, name, length);
        return Value(function, m_Context);
    }
    Value CreateClosureByRaw(Closure* closure)
    {
        JSValue jsClosure = detail::CreateClosure(m_Context, closure);
        return Value(jsClosure, m_Context);
    }
    template <typename T>
    Value CreateClosureNoWrap(T&& closure)
    {
        auto* p = new Closure(std::forward<T>(closure));
        JSValue jsClosure = detail::CreateClosure(m_Context, p);
        return Value(jsClosure, m_Context);
    }
    template <typename T>
    Value CreateClosure(const T& closure)
    {
        auto c = detail::WrapClosure(closure);
        return CreateClosureNoWrap(std::move(c));
    }

#ifdef QUICKJSPP_ENABLE_DEBUGGER
    struct BreakPoint
    {
        std::string filename;
        uint32_t line;
    };
    void SetDebuggerMode(int mode);
    DebuggerServerHandle& GetDebuggerServer()
    {
        return m_Server;
    }
    std::vector<BreakPoint>& GetBreakPoints()
    {
        return m_BreakPoints;
    }
#endif
private:
    Context() = default;
    JSContext* m_Context;
#ifdef QUICKJSPP_ENABLE_DEBUGGER
    DebuggerServerHandle m_Server{nullptr};
    std::unique_ptr<std::thread> m_ServerThread;

    std::vector<BreakPoint> m_BreakPoints; // 存储断点信息
#endif
};
template <>
inline int32_t Value::Convert<int32_t>() const
{
    // if (JS_IsException(value))
    //     return 0; // or throw an exception
    return JS_VALUE_GET_INT(m_Value);
}
template <>
inline double Value::Convert<double>() const
{
    // if (JS_IsException(value))
    //     return 0.0; // or throw an exception
    double result;
    if (JS_ToFloat64(m_Context, &result, m_Value) < 0)
        return 0.0; // or throw an exception
    return result;
}
template <>
inline std::string Value::Convert<std::string>() const
{
    // if (JS_IsException(value))
    //     return ""; // or throw an exception
    const char* str = JS_ToCString(m_Context, m_Value);
    if (!str)
        return ""; // or throw an exception
    std::string result(str);
    JS_FreeCString(m_Context, str);
    return result;
}

} // namespace qjs