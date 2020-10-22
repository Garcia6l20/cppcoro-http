#pragma once

#include <concepts>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <typeindex>
#include <tuple>
#include <regex>
#include <variant>

#include <tuple>

#include <cppcoro/http/route_parameter.hpp>

#include <cppcoro/detail/function_traits.hpp>
#include <cppcoro/detail/type_index.hpp>

namespace cppcoro::http::detail {

    using namespace cppcoro::detail;

    template<typename ReturnT, typename PredicateType, typename FunctionTraitsT>
    struct view_handler_traits_impl : FunctionTraitsT
    {
        using parameters_tuple_type = typename FunctionTraitsT::template parameters_tuple<PredicateType>;
        using data_type = typename parameters_tuple_type::tuple_type;
        // TODO handle non-awaitables
        using await_result_type = std::conditional_t<std::is_void_v<typename FunctionTraitsT::return_type>, void, typename FunctionTraitsT::return_type::value_type>;
        static const constexpr size_t data_arity = std::tuple_size_v<data_type>;

        static constexpr auto make_tuple = parameters_tuple_type::make;

        template<int type_idx, int match_idx, typename MatcherT>
        static void _load_data(data_type &data, const MatcherT &match) {
            if constexpr (type_idx < data_arity) {
                using ParamT = typename FunctionTraitsT::template arg<type_idx>::clean_type;
                std::get<type_idx>(data) = route_parameter<ParamT>::load(match.template get<match_idx>());
                _load_data<type_idx + 1, match_idx + route_parameter<ParamT>::group_count() + 1>(data, match);
            }
        }

        template<typename MatcherT>
        static bool load_data(const MatcherT &match, data_type &data) {
            _load_data<0, 1>(data, match);
            return true;
        }
    };

    template<int type_idx, int match_idx, int max, typename TupleT, typename MatcherT>
    static constexpr void _load_data(TupleT &data, const MatcherT &match) {
        if constexpr (type_idx < max) {
            using ParamT = std::decay_t<decltype(std::get<type_idx>(data))>;
            std::get<type_idx>(data) = route_parameter<ParamT>::load(match.template get<match_idx>());
            _load_data<type_idx + 1, match_idx + route_parameter<ParamT>::group_count() + 1, max>(data, match);
        }
    }

    template<typename TupleT, typename MatcherT>
    static constexpr bool load_data(const MatcherT &match, TupleT &data) {
        _load_data<0, 1, std::tuple_size_v<TupleT>>(data, match);
        return true;
    }

    template<int max_size, typename TupleT, typename MatcherT>
    static constexpr bool load_data(const MatcherT &match, TupleT &data) {
        _load_data<0, 1, max_size>(data, match);
        return true;
    }

    template<typename ReturnT, typename PredicateT, class T>
    struct view_handler_traits
        : view_handler_traits<ReturnT, PredicateT, decltype(&std::decay_t<T>::operator())>
    {
    };

    // mutable lambda
    template<typename ReturnT, typename PredicateT, class Ret, class Cls, class... Args>
    struct view_handler_traits<ReturnT, PredicateT, Ret(Cls::*)(Args...)>
        : view_handler_traits_impl<ReturnT, PredicateT, function_detail::types<Ret, Cls, std::true_type, std::true_type, Args...>>
    {
    };

    // immutable lambda
    template<typename ReturnT, typename PredicateT, class Ret, class Cls, class... Args>
    struct view_handler_traits<ReturnT, PredicateT, Ret(Cls::*)(Args...) const>
        : view_handler_traits_impl<ReturnT, PredicateT, function_detail::types<Ret, Cls, std::true_type, std::true_type, Args...>>
    {
    };

    // std::function
    template<typename ReturnT, typename PredicateT, class Ret, class... Args>
    struct view_handler_traits<ReturnT, PredicateT, std::function<Ret(Args...)>>
        : view_handler_traits_impl<ReturnT, PredicateT, function_detail::types<Ret, std::nullptr_t, std::true_type, std::true_type, Args...>>
    {
    };

    // c-function
    template<typename ReturnT, typename PredicateT, class Ret, class... Args>
    struct view_handler_traits<ReturnT, PredicateT, Ret(*)(Args...)>
        : view_handler_traits_impl<ReturnT, PredicateT, function_detail::types<Ret, std::nullptr_t, std::true_type, std::false_type, Args...>>
    {
    };

// helper type for the visitor #4
    template<class... Ts>
    struct overloaded : Ts ...
    {
        using Ts::operator()...;
    };
// explicit deduction guide (not needed as of C++20)
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace xdev::net::detail
