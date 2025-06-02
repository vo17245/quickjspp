#pragma once
#include "closure.h"
#include "bind.h"
#include "traits.h"
namespace qjs::detail
{


template <typename T>
struct ConvertArgs2;
template <typename... Ts>
struct ConvertArgs2<std::tuple<Ts...>>
{
    bool operator()(std::tuple<Ts...>& args, JSValueConst* argv, JSContext* ctx)
    {
        return ConvertArgs<Ts...>{}(args, argv, ctx);
    }
};
template <typename F>
inline Closure WrapClosure(const F& func)
{
    using namespace detail;
    using FuncType = std::decay_t<F>;
    using Traits = LambdaTraits<decltype(&std::remove_reference_t<F>::operator())>;
    using Return = typename Traits::ReturnType;
    using Args = typename Traits::ArgsTuple;
    return [func](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
        if (argc != std::tuple_size<Args>::value)
        {
            JS_ThrowTypeError(ctx, "Expected %d arguments, got %d", std::tuple_size<Args>::value, argc);
            return JS_EXCEPTION;
        }
        Args args;
        if (!ConvertArgs2<Args>{}(args, argv, ctx))
        {
            JS_ThrowTypeError(ctx, "Argument conversion failed");
            return JS_EXCEPTION; // 转换失败
        }
        if constexpr (std::is_same_v<Return, void>)
        {
            std::apply(func, args);
            return JS_UNDEFINED;
        }
        else
        {
            auto res = std::apply(func, args);
            return ConvertToJsType<Return>::Convert(ctx, res);
        }
    };
}
}