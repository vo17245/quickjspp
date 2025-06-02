#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include <quickjs.h>
#include <optional>
#include <tuple>
namespace qjs::detail
{
template <typename T>
struct ConvertToCppType
{
    static std::optional<T> Convert(JSContext* ctx, JSValue value)
    {
        static_assert(sizeof(T) == 0, "Unsupported type for conversion");
        return std::nullopt; // 默认返回JSValue
    }
};
template <>
struct ConvertToCppType<int32_t>
{
    static std::optional<int32_t> Convert(JSContext* ctx, JSValue value)
    {
        int32_t result;
        if (JS_ToInt32(ctx, &result, value) < 0)
            return std::nullopt; // 转换失败
        return result;
    }
};
template <>
struct ConvertToCppType<uint32_t>
{
    static std::optional<int32_t> Convert(JSContext* ctx, JSValue value)
    {
        uint32_t result;
        if (JS_ToUint32(ctx, &result, value) < 0)
            return std::nullopt; // 转换失败
        return result;
    }
};
template <>
struct ConvertToCppType<double>
{
    static std::optional<double> Convert(JSContext* ctx, JSValue value)
    {
        double result;
        if (JS_ToFloat64(ctx, &result, value) < 0)
            return std::nullopt; // 转换失败
        return result;
    }
};
template <>
struct ConvertToCppType<std::string>
{
    static std::optional<std::string> Convert(JSContext* ctx, JSValue value)
    {
        const char* str = JS_ToCString(ctx, value);
        if (!str)
            return std::nullopt; // 转换失败
        std::string result(str);
        JS_FreeCString(ctx, str);
        return result;
    }
};
template <>
struct ConvertToCppType<float>
{
    static std::optional<float> Convert(JSContext* ctx, JSValue value)
    {
        double result;
        if (JS_ToFloat64(ctx, &result, value) < 0)
            return std::nullopt; // 转换失败
        return static_cast<float>(result);
    }
};

template <>
struct ConvertToCppType<bool>
{
    static std::optional<bool> Convert(JSContext* ctx, JSValue value)
    {
        if (JS_IsBool(value))
        {
            return JS_ToBool(ctx, value) != 0;
        }
        return std::nullopt; // 转换失败
    }
};
template<typename T>
struct ConvertToJsType
{
    static_assert(sizeof(T)==0,"unsupport type" );
};
template<>
struct ConvertToJsType<int32_t>
{
    static JSValue Convert(JSContext* ctx, int32_t value)
    {
        return JS_NewInt32(ctx, value);
    }
};
template<>
struct ConvertToJsType<double>
{
    static JSValue Convert(JSContext* ctx, double value)
    {
        return JS_NewFloat64(ctx, value);
    }
};
template<>
struct ConvertToJsType<std::string>
{
    static JSValue Convert(JSContext* ctx, const std::string& value)
    {
        return JS_NewString(ctx, value.c_str());
    }
};
template<>
struct ConvertToJsType<float>
{
    static JSValue Convert(JSContext* ctx, float value)
    {
        return JS_NewFloat64(ctx, static_cast<double>(value));
    }
};
template<>
struct ConvertToJsType<bool>
{
    static JSValue Convert(JSContext* ctx, bool value)
    {
        return JS_NewBool(ctx, value);
    }
};

template<>
struct ConvertToJsType<uint32_t>
{
    static JSValue Convert(JSContext* ctx, bool value)
    {
        return JS_NewUint32(ctx, value);
    }
};


template <size_t n, typename... Ts>
struct ConvertArg
{
    bool operator()(std::tuple<Ts...>& args, JSValueConst* argv, JSContext* ctx)
    {
        auto& dst = std::get<n>(args);
        auto res = ConvertToCppType<std::decay_t<decltype(dst)>>::Convert(ctx, argv[n]);
        if (!res.has_value())
        {
            JS_ThrowTypeError(ctx, "Argument %zu conversion failed", n);
            return false; // 转换失败
        }
        dst = std::move(res.value());
        return true; // 转换成功
    }
};
template<size_t n,typename... Ts>
struct ConvertArgsImpl
{
    bool operator()(std::tuple<Ts...>& args, JSValueConst* argv, JSContext* ctx)
    {
        bool res=ConvertArg<n, Ts...>{}(args,argv,ctx);
        if(!res)return false;
        if constexpr(n!=0)
        {
            return ConvertArgsImpl<n - 1, Ts...>{}(args, argv, ctx); // 递归处理前面的参数
        }
        return true;
    }
};
template <typename... Ts>
struct ConvertArgs
{
    bool operator()(std::tuple<Ts...>& args, JSValueConst* argv, JSContext* ctx)
    {
        if constexpr (sizeof...(Ts) == 0)
        {
            return true; // 没有参数，直接返回成功
        }
        else
        {
            return ConvertArgsImpl<sizeof...(Ts) - 1, Ts...>{}(args, argv, ctx); // 从最后一个参数开始转换
        }
    }
};
template <typename F, typename T, typename... Ts>
struct FunctionCall
{
    JSValue operator()(JSContext* ctx, JSValueConst this_val, int argc,
                       JSValueConst* argv,F f)
    {
        if (argc != sizeof...(Ts))
        {
            JS_ThrowTypeError(ctx, "Expected %d arguments, got %d", sizeof...(Ts), argc);
            return JS_EXCEPTION;
        }
        std::tuple<Ts...> args;
        if constexpr (sizeof...(Ts) > 0)
        {
            if (!ConvertArgs(ctx, args, argv))
            {
                JS_ThrowTypeError(ctx, "Argument conversion failed");
                return JS_EXCEPTION; // 转换失败
            }
        }
        auto res=std::apply(f, args);
        return ConvertToJsType<T>{}(res);
    }
};

} // namespace qjs::detail