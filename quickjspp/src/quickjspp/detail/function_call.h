#pragma once
#include <tuple>
#include "bind.h"
namespace qjs::detail
{
template<typename T,typename =void>
struct Decay
{
    using type = std::decay_t<T>*;
};
template<typename T>
struct Decay<T,std::enable_if<HasToCppTypeConvert<T>::value>>
{
    using type=std::decay_t<T>;
};
template <typename T,typename U>
struct TupleDecayImpl;
template <typename T,size_t... I>
struct TupleDecayImpl<T, std::index_sequence<I...>>
{
    using type = std::tuple<typename Decay<std::tuple_element_t<I, T>>::type...>;
};

template <typename T>
struct TupleDecay
{
    using type = typename TupleDecayImpl<T, std::make_index_sequence<std::tuple_size_v<T>>>::type;
};
// 
/**
 * @note t must be a registered type, or a non-registered type pointer.
 * @example:
 * struct Vec3f;// assume Vec3f is registered
 * float x;
 * float& x_=DecayIfRegistered<float>{}(x);
 * Vec3f* v = new Vec3f();
 * Vec3f& v_=DecayIfRegistered<Vec3f*>{}(v);
*/
template<typename T,typename = void>
struct DereferenceIfRegistered
{
    
    std::remove_pointer_t<T>& operator()(T t)
    {
        #ifdef _MSC_VER
        static_assert(std::is_pointer<T>::value, "T must be a pointer to a registered type,funcsig: " __FUNCSIG__);
        #else 
        static_assert(std::is_pointer<T>::value, "T must be a pointer to a registered type");
        #endif
        
        return *t; // 如果是注册的类型，解引用返回
    }
};
template<typename T>
struct DereferenceIfRegistered<T, std::enable_if<HasToCppTypeConvert<T>::value>>
{
    // 如果不是注册的类型，直接返回引用
    T& operator()(T& t)
    {
        return t; 
    }
};

} // namespace qjs::detail