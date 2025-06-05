#pragma once
#include <tuple>
#include <type_traits>
#include <print>
namespace qjs::detail
{
template <typename T>
struct LambdaTraits
{
    static_assert(sizeof(T) == 0, "LambdaTraits is not specialized for this type. Please provide a specialization for your lambda type.");
};

// 针对非泛型 lambda
template <typename ClassType, typename Ret, typename... Args>
struct LambdaTraits<Ret (ClassType::*)(Args...) const>
{
    using ReturnType = Ret;
    using ArgsTuple = std::tuple<Args...>;
};

template <size_t from, typename T>
struct make_index_sequence_impl;
template <size_t from, size_t... I>
struct make_index_sequence_impl<from, std::index_sequence<I...>>
{
    using type = std::index_sequence<(I + from)...>;
};
template <size_t from, size_t to>
struct make_index_sequence
{
    static_assert(from <= to, "from must be less than or equal to to");
    using type = make_index_sequence_impl<from, std::make_index_sequence<to - from>>::type;
};

template <typename T, typename S>
struct SliceTypeImpl;
template <typename T, size_t... I>
struct SliceTypeImpl<T, std::index_sequence<I...>>
{
    using Type = std::tuple<std::tuple_element_t<I, T>...>;
};
template <typename T, size_t from, size_t to>
struct SliceType
{
    using Type = typename SliceTypeImpl<T, typename make_index_sequence<from, to>::type>::Type;
};
#ifdef _MSC_VER
template <typename U>
void print_type()
{
    static_assert(sizeof(U) == 0, __FUNCSIG__); // 会在编译时报错并打印类型名
}
#else 
template <typename U>
void print_type()
{
    static_assert(sizeof(U) == 0, __PRETTY_FUNCTION__); // 会在编译时报错并打印类型名
}
#endif
} // namespace qjs::detail