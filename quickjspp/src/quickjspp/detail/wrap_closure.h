#pragma once
#include "closure.h"
#include "bind.h"
#include "traits.h"
#include "function_call.h"
#include <utility>
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
namespace WrapClosureUtils
{
template<typename F,typename T,size_t... I>
auto apply_args_impl(F& f,T& args,std::index_sequence<I...>)
{
    return f(detail::DereferenceIfRegistered<std::tuple_element_t<I, T>>{}(std::get<I>(args))...);
}
template<typename F,typename T>
auto apply_args(F& f,T& args)
{
    return apply_args_impl(f, args,
        std::make_index_sequence<std::tuple_size_v<T>>{});
}

}
template <typename F>
inline Closure WrapClosure(const F& func)
{
    using namespace detail;
    using FuncType = std::decay_t<F>;
    using Traits = LambdaTraits<decltype(&std::remove_reference_t<F>::operator())>;
    using Return = typename Traits::ReturnType;
    using Args = TupleDecay<typename Traits::ArgsTuple>::type;
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
            WrapClosureUtils::apply_args(func, args);
            return JS_UNDEFINED;
        }
        else
        {
            auto res = WrapClosureUtils::apply_args(func, args);
            return ConvertToJsType<Return>::Convert(ctx, res);
        }
    };
}
}