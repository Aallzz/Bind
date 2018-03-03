#ifndef BIND_H
#define BIND_H

#include <utility>
#include <iostream>
#include <tuple>
#include <string>

template <typename F, typename... Ts>
struct bind_t;

template<int N>
struct placeholder {};

constexpr placeholder<1> _1;
constexpr placeholder<2> _2;
constexpr placeholder<3> _3;
constexpr placeholder<4> _4;

template<typename T>
struct binder {
    template<typename TT>
    binder(TT&& new_value)
        : value(std::forward<TT>(new_value)) {}

    template<typename... Us>
    decltype(auto) operator ()(Us...) {
        return static_cast<T>(value);
    }

private:
    std::remove_reference_t<T> value;
};

template<>
struct binder<placeholder<1>> {
    binder(placeholder<1>){}
    template<typename U, typename... Us>
    decltype(auto) operator ()(U&& value, Us&&...) {
        return std::forward<U>(value);
    }
};

template<int N>
struct binder<placeholder<N>> {
    binder(placeholder<N>) {}
    template<typename U, typename... Us>
    decltype(auto) operator ()(U&& , Us&&... rest) {
        return binder<placeholder<N - 1>>(placeholder<N - 1>()).operator ()(std::forward<Us>(rest)...);
    }
};

template<typename F, typename... Ts>
struct binder<bind_t<F, Ts...>> {
    binder(const bind_t<F, Ts...>& new_bnd)
        : bnd(new_bnd) {}

    binder(bind_t<F, Ts...>&& new_bnd)
        :bnd(std::move(new_bnd)) {}

    template<typename... Us>
    decltype(auto) operator ()(Us&&... args) {
        return bnd(std::forward<Us>(args)...);
    }

private:
    bind_t<F, Ts...> bnd;
};

template<size_t Id, typename... Ts>
struct count_placeholders;

template<size_t Id>
struct count_placeholders<Id> {
    constexpr static size_t value = 0;
};

template<size_t Id, typename T, typename... Ts>
struct count_placeholders<Id, T, Ts...> {
    constexpr static size_t value = count_placeholders<Id, Ts...>::value;
};

template<size_t Id, typename... Ts>
struct count_placeholders<Id, placeholder<Id>, Ts...> {
    constexpr static size_t value = count_placeholders<Id, Ts...>::value + 1;
};

template<size_t Id, typename... Ts>
constexpr bool is_unique_placeholder = count_placeholders<Id, Ts...>::value <= 1;

#if __cplusplus <= 201402L

template<typename T>
struct remove_bind_t_ref_c {
    using type = T;
};

template<typename... T>
struct remove_bind_t_ref_c<bind_t<T...> const&> {
    using type = bind_t<T...>;

};

template<typename... T>
struct remove_bind_t_ref_c<bind_t<T...>&> {
    using type = bind_t<T...>;
};

template<typename... T>
struct remove_bind_t_ref_c<bind_t<T...>&&> {
    using type = bind_t<T...>;
};

template<typename T>
struct remove_placeholders_ref_c {
    using type = T;
};

template<int N>
struct remove_placeholders_ref_c<placeholder<N>&> {
    using type = placeholder<N>;
};


template<int N>
struct remove_placeholders_ref_c<placeholder<N> const& > {
    using type = placeholder<N>;
};


template<int N>
struct remove_placeholders_ref_c<placeholder<N>&&> {
    using type = placeholder<N>;
};

template<typename T>
using remove_bind_t_ref_c_t = typename remove_bind_t_ref_c<T>::type;

template<typename T>
using remove_placeholders_ref_c_t = typename remove_placeholders_ref_c<T>::type;

#else

template<typename T>
struct remove_type_ref_const {
    using type = T;
};

template<template<auto...> class C, auto... Ts>
struct remove_type_ref_const<C<Ts...> const&> {
    using type = C<Ts...>;
};

template<template<auto...> class C, auto... Ts>
struct remove_type_ref_const<C<Ts...>&> {
    using type = C<Ts...>;
};


template<template<auto...> class C, auto... Ts>
struct remove_type_ref_const<C<Ts...>&&> {
    using type = C<Ts...>;
};

template<template<typename...> class C, typename... Ts>
struct remove_type_ref_const<C<Ts...> const&> {
    using type = C<Ts...>;
};

template<template<typename...> class C, typename... Ts>
struct remove_type_ref_const<C<Ts...>&> {
    using type = C<Ts...>;
};


template<template<typename...> class C, typename... Ts>
struct remove_type_ref_const<C<Ts...>&&> {
    using type = C<Ts...>;
};


template<typename T>
using remove_bind_t_ref_c_t = typename remove_type_ref_const<T>::type;

template<typename T>
using remove_placeholders_ref_c_t = typename remove_type_ref_const<T>::type;

#endif


template<typename F, typename... Ts>
struct bind_t {

    template<typename FF, typename... TTs>
    bind_t(FF&& bf, TTs&&... bargs)
        : f(std::forward<FF>(bf)), args(std::forward<TTs>(bargs)...) {}

    template<typename... Us>
    decltype(auto) operator() (Us&&... bargs) {
        return call(std::index_sequence_for<Ts...>(), std::index_sequence_for<Us...>(), std::forward<Us>(bargs)...);
    }

private:

    template<size_t... IDTs, size_t... IDUs, typename... Us>
    decltype(auto) call(std::index_sequence<IDTs...>, std::index_sequence<IDUs...>, Us&&... bind_args) {
        return f((std::get<IDTs>(args)(
                      static_cast<
                          std::conditional_t<
                               is_unique_placeholder<
                                 IDUs + 1,
                                 std::tuple_element_t<IDTs, arguments_types>...
                               >,
                               Us&&,
                               Us&
                          >
                      >(bind_args)...)
                  )...
                 );
    }

    using arguments_types = std::tuple<Ts...>;
    using arguments_types_bindered = std::tuple<binder<remove_bind_t_ref_c_t<Ts>>...>;
    F f;
    arguments_types_bindered args;
};

template<typename F, typename... Ts>
decltype(auto) bind(F&& f, Ts&&... args) {
    return bind_t<std::decay_t<F>, remove_placeholders_ref_c_t<std::decay_t<Ts>&>...>(std::forward<F>(f), std::forward<Ts>(args)...);
}

template<typename F, typename... Ts>
decltype(auto) call_once_bind(F&& f, Ts&&... args) {
    return bind_t<std::decay_t<F>, remove_placeholders_ref_c_t<std::decay_t<Ts>&&>...>(std::forward<F>(f), std::forward<Ts>(args)...);
}

#endif // BIND_H
