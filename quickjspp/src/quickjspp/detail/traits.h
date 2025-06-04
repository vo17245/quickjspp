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


template<size_t from,typename T>
struct make_index_sequence_impl;
template<size_t from,size_t... I>
struct make_index_sequence_impl<from,std::index_sequence<I...>>
{
    using type= std::index_sequence<(I+from)...>;
};
template<size_t from,size_t to>
struct make_index_sequence
{
    static_assert(from <= to, "from must be less than or equal to to");
    using type=make_index_sequence_impl<from,std::make_index_sequence<to - from>>::type;
};

template<typename T,typename S>
struct SliceTypeImpl;
template<typename T,size_t... I>
struct SliceTypeImpl<T, std::index_sequence<I...>>
{
    using Type=std::tuple<std::tuple_element<I, T>...>;
};
template<typename T,size_t from,size_t to>
struct SliceType
{
    using Type= typename SliceTypeImpl<T,typename make_index_sequence<from, to>::type>::Type;
};
}