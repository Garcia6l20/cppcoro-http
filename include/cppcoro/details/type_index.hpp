#pragma once

#include <cstdlib>

#include <tuple>
#include <type_traits>

namespace cppcoro::details {

    /**
     * Useful template for resolving index of variadic template arguments
     */
    template<typename T, typename ... Ts>
    struct type_index;

    template<typename T, typename ... Ts>
    struct type_index<T, T, Ts ...>
        : std::integral_constant<std::size_t, 0>
    {
    };

    template<typename T, typename U, typename ... Ts>
    struct type_index<T, U, Ts ...>
        : std::integral_constant<std::size_t, 1 + type_index<T, Ts...>::value>
    {
    };

    template<typename T, typename...Ts>
    static constexpr std::size_t type_index_v = type_index<T, Ts...>::value;

    template<size_t idx, typename...Ts>
    struct type_at
    {
        using type = std::tuple_element_t<idx, std::tuple<Ts...>>;
    };

    template<size_t idx, typename...Ts>
    using type_at_t = typename type_at<idx, Ts...>::type;
}
