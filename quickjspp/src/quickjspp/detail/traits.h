#pragma once
#include <tuple>
#include <type_traits>
namespace qjs::detail
{
template <typename T>
struct LambdaTraits;

// 针对非泛型 lambda
template <typename ClassType, typename Ret, typename... Args>
struct LambdaTraits<Ret (ClassType::*)(Args...) const>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<std::decay_t<Args>...>;
};
}