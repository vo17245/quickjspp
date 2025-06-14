#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include <quickjs.h>
#include <optional>
#include <tuple>
#include "class_wrapper.h"
#include <print>//for debug
namespace qjs::detail
{
template <typename T>
struct ConvertToCppType
{
};
template <typename T, typename = void>
struct HasToCppTypeConvert
{
    static constexpr bool value = false;
};
template <typename T>
struct HasToCppTypeConvert<T, std::void_t<decltype(static_cast<std::optional<T>(*)(JSContext*, JSValue)>(&ConvertToCppType<T>::Convert))>>
{
    static constexpr bool value = true;
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
    static std::optional<uint32_t> Convert(JSContext* ctx, JSValue value)
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
template <typename T>
struct ConvertToJsType
{
};

template <>
struct ConvertToJsType<int32_t>
{
    static JSValue Convert(JSContext* ctx, int32_t value)
    {
        return JS_NewInt32(ctx, value);
    }
};
template <>
struct ConvertToJsType<double>
{
    static JSValue Convert(JSContext* ctx, double value)
    {
        return JS_NewFloat64(ctx, value);
    }
};
template <>
struct ConvertToJsType<std::string>
{
    static JSValue Convert(JSContext* ctx, const std::string& value)
    {
        return JS_NewString(ctx, value.c_str());
    }
};
template <>
struct ConvertToJsType<float>
{
    static JSValue Convert(JSContext* ctx, float value)
    {
        return JS_NewFloat64(ctx, static_cast<double>(value));
    }
};
template <>
struct ConvertToJsType<bool>
{
    static JSValue Convert(JSContext* ctx, bool value)
    {
        return JS_NewBool(ctx, value);
    }
};

template <>
struct ConvertToJsType<uint32_t>
{
    static JSValue Convert(JSContext* ctx, uint32_t value)
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
        using Type= std::decay_t<decltype(dst)>;
        if constexpr (HasToCppTypeConvert<Type>::value)
        {
            auto res = ConvertToCppType<std::decay_t<decltype(dst)>>::Convert(ctx, argv[n]);
            if (!res.has_value())
            {
                JS_ThrowTypeError(ctx, "Argument %zu conversion failed", n);
                return false; // 转换失败
            }
            dst = std::move(res.value());
            return true; // 转换成功
        }
        else
        {
            static_assert(std::is_pointer<Type>::value, "Type must be a pointer to a registered type or a base type");
            // 认为是注册过的类型
            JSValue val = argv[n];
            void* handle=GetOpaque(ctx, val);
            if(!handle)
            {
                assert(false&& "failed to get opaque, may be not a registered type");
                return false;
            }
            Type ptr= reinterpret_cast<Type>(handle);
            //static_assert(std::is_pointer<Type>::value,"Type must be a pointer to a registered type");
            
            if (!ptr)
            {
                JS_ThrowTypeError(ctx, "Argument %zu is not a registered type or base type", n);
                return false; // 转换失败
            }
            dst = ptr; // 将指针赋值给目标类型
            return true; // 转换成功
        }
    }
};
template <size_t n, typename... Ts>
struct ConvertArgsImpl
{
    bool operator()(std::tuple<Ts...>& args, JSValueConst* argv, JSContext* ctx)
    {
        bool res = ConvertArg<n, Ts...>{}(args, argv, ctx);
        if (!res) return false;
        if constexpr (n != 0)
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


} // namespace qjs::detail