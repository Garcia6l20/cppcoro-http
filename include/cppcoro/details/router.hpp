#pragma once

#include <string>
#include <type_traits>
#include <typeinfo>
#include <typeindex>
#include <tuple>
#include <regex>
#include <variant>

#include <tuple>

#include <cppcoro/http/route_parameter.hpp>

#include <cppcoro/details/function_traits.hpp>
#include <cppcoro/details/type_index.hpp>

namespace cppcoro::http::details {

    using namespace cppcoro::details;

    template<typename ReturnT, typename T>
    struct view_handler_traits
        : public view_handler_traits<ReturnT, decltype(&T::operator())>
    {
    };

    template<typename ReturnT, typename ClassType, typename ReturnType, typename... Args>
    struct view_handler_traits<ReturnT, ReturnType(ClassType::*)(Args...) const>
    {
        using handler_traits = function_traits<ReturnType(ClassType::*)(Args...)>;
        using return_type = typename handler_traits::return_type;
        using function_type = typename handler_traits::function_type;
        static const constexpr size_t arity = handler_traits::arity;

        template<size_t idx>
        using arg = typename handler_traits::template arg<idx>;

        using parameters_tuple_type = typename handler_traits::parameters_tuple;
        using data_type = typename parameters_tuple_type::tuple_type;

        static constexpr auto make_tuple = parameters_tuple_type::make;

        template<int idx, int match_idx, typename...ArgsT>
        struct _load_data;

        using _get_param_func = std::function<std::string_view(int)>;

        template<int type_idx, int match_idx, typename FirstT, typename...ArgsT>
        struct _load_data<type_idx, match_idx, FirstT, ArgsT...>
        {

            template <typename MatcherT>
            void operator()(data_type &data, const MatcherT &match) {
                using ParamT = std::remove_cvref_t<FirstT>;
                std::get<type_idx>(data) = route_parameter<ParamT>::load(match.template get<match_idx>());
                _load_data<type_idx + 1, match_idx + route_parameter<ParamT>::group_count() + 1, ArgsT...>{}(data,
                                                                                                             match);
            }
        };

        template<int type_idx, int match_idx, typename LastT>
        struct _load_data<type_idx, match_idx, LastT>
        {
            template <typename MatcherT>
            void operator()(data_type &data, const MatcherT &match) {
                using ParamT = std::remove_cvref_t<LastT>;
                std::get<type_idx>(data) = route_parameter<ParamT>::load(match.template get<match_idx>());
            }
        };

        template <typename MatcherT>
        static bool load_data(const MatcherT &match, data_type &data) {
            if constexpr (sizeof...(Args))
                _load_data<0, 1, Args...>{}(data, match);
            return true;
        }

        template<int idx, typename Ret, typename Arg0, typename... ArgsT>
        static std::function<Ret(ArgsT...)>
        _make_recursive_lambda(std::function<Ret(Arg0, ArgsT...)> func, const data_type &data) {
            if constexpr (sizeof...(ArgsT) == 0) {
                return [&]() -> Ret {
                    return func(std::get<idx>(data));
                };
            } else {
                return [&](ArgsT... args) -> Ret {
                    return func(std::get<idx>(data), args...);
                };
            }
        }


        template<typename Ret, typename Cls, typename IsMutable, typename IsLambda, typename... ArgsT>
        struct _invoker : function_detail::types<Ret, Cls, IsMutable, IsLambda, Args...>
        {
        private:
            template<int idx>
            static Ret _invoke(std::function<Ret()> func, const data_type & /*data*/) {
                return func();
            }

            template<int idx, typename Arg0, typename...RestArgsT>
            static Ret _invoke(std::function<Ret(Arg0, RestArgsT...)> func, const data_type &data) {
                return _invoke<idx + 1>(_make_recursive_lambda<idx>(func, data), data);
            }

        public:
            template<typename Fcn>
            Ret operator()(Fcn func, const data_type &data) {
                function_type fcn = func;
                return _invoke<0>(fcn, data);
            }
        };

        template<class T>
        struct invoker
            : invoker<decltype(&std::remove_cvref_t<T>::operator())>
        {
        };

        // mutable lambda
        template<class Ret, class Cls, class... ArgsT>
        struct invoker<Ret(Cls::*)(ArgsT...)>
            : _invoker<Ret, Cls, std::true_type, std::true_type, ArgsT...>
        {
        };

        // immutable lambda
        template<class Ret, class Cls, class... ArgsT>
        struct invoker<Ret(Cls::*)(ArgsT...) const>
            : _invoker<Ret, Cls, std::false_type, std::true_type, ArgsT...>
        {
        };

        // function
        template<class Ret, class... ArgsT>
        struct invoker<std::function<Ret(ArgsT...)>>
            : _invoker<Ret, std::nullptr_t, std::true_type, std::false_type, Args...>
        {
        };

        template<int idx>
        static void _make_match_pattern(std::string &pattern) {
            static const std::regex route_args_re(R"(<([^<>]+)>)");
            std::smatch match;
            if (!std::regex_search(pattern, match, route_args_re)) {
                throw std::exception();
            }
            pattern.replace(static_cast<size_t>(match.position()),
                            static_cast<size_t>(match.length()),
                            "(" + route_parameter<typename arg<idx>::clean_type>::pattern() + ")");
        }

        using invoker_type = invoker<typename handler_traits::function_type>;

        template<int idx, typename...ArgsT>
        struct pattern_maker;

        template<int idx, typename FirstT>
        struct pattern_maker<idx, FirstT>
        {
            void operator()(std::string &pattern) {
                _make_match_pattern<idx>(pattern);
            }
        };

        template<int idx, typename FirstT, typename...RestT>
        struct pattern_maker<idx, FirstT, RestT...>
        {
            void operator()(std::string &pattern) {
                _make_match_pattern<idx>(pattern);
                pattern_maker<idx, RestT...>{}(pattern);
            }
        };

        static std::regex make_regex(const std::string &path) {
            std::string pattern = path;
            if constexpr (sizeof...(Args))
                pattern_maker<0, Args...>{}(pattern);
            return std::regex(pattern);
        }

    };  // class view_handler_traits

    using test_type = function_traits<std::function<void(std::string)>>;
    static_assert(std::is_same_v<test_type::parameters_tuple::tuple_type, std::tuple<std::string>>);
    static_assert(test_type::arity == 1);

//    template<typename...BodyTypes>
//    struct body_traits
//    {
//        template<typename BodyType>
//        using request = boost::beast::http::request<BodyType>;
//        template<typename BodyType>
//        using response = boost::beast::http::response<BodyType>;
//        template<typename BodyType>
//        using request_parser = boost::beast::http::request_parser<BodyType>;
//
//        using body_variant = std::variant<BodyTypes...>;
//        using body_value_variant = std::variant<typename BodyTypes::value_type...>;
//        using response_variant = std::variant<response<BodyTypes>...>;
//        using parser_variant = std::variant<request_parser<BodyTypes>...>;
//        using request_variant = std::variant<request<BodyTypes>...>;
//
//        template<typename T>
//        static constexpr std::size_t body_index = type_index_v<T, BodyTypes...>;
//
//        template<typename T>
//        static constexpr auto body_value_index_v = type_index_v<std::remove_cvref_t<T>, typename BodyTypes::value_type...>;
//
//        template<typename T>
//        static constexpr auto body_index_v = type_index_v<std::remove_cvref_t<T>, BodyTypes...>;
//
//        template<typename BodyValueType>
//        static auto body_of() {
//            return body_variant{std::in_place_index<body_value_index_v<BodyValueType>>};
//        }
//
//        template<typename BodyValueType>
//        using body_of_value_t = type_at_t<type_index_v<std::remove_cvref_t<BodyValueType>, typename BodyTypes::value_type...>, BodyTypes...>;
//    }

// helper type for the visitor #4
    template<class... Ts>
    struct overloaded : Ts ...
    {
        using Ts::operator()...;
    };
// explicit deduction guide (not needed as of C++20)
    template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace xdev::net::details
