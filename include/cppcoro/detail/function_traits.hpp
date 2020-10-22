#pragma once

#include <functional>

namespace cppcoro::detail {

    namespace function_detail {

        struct parameters_tuple_all_enabled
        {
            template<typename T>
            static constexpr bool enabled = true;
        };

        template<typename...ArgsT>
        struct parameters_tuple_disable
        {
            template<typename T>
            static constexpr bool enabled = !std::disjunction_v<std::is_same<std::remove_cvref_t<T>, std::remove_cvref_t<ArgsT>>...>;
        };

        template<typename Ret, typename Cls, typename IsMutable, typename IsLambda, typename... Args>
        struct types
        {
            using is_mutable = IsMutable;
            using is_lambda = IsLambda;

            static constexpr auto is_function() { return !is_lambda(); }

            enum
            {
                arity = sizeof...(Args)
            };

            using return_type = Ret;

            using tuple_type = std::tuple<Args...>;

            template<size_t i>
            struct arg
            {
                using type = typename std::tuple_element<i, tuple_type>::type;
                using clean_type = std::remove_cvref_t<type>;
            };

            using function_type = std::function<Ret(Args...)>;

            template<typename Predicate = parameters_tuple_all_enabled>
            struct parameters_tuple
            {

                template<typename FirstT, typename...RestT>
                static constexpr auto __make_tuple() {
                    if constexpr (!sizeof...(RestT)) {
                        if constexpr (Predicate::template enabled<FirstT>)
                            return std::make_tuple<FirstT>({});
                        else return std::tuple();
                    } else {
                        if constexpr (Predicate::template enabled<FirstT>)
                            return std::tuple_cat(std::make_tuple<FirstT>({}), __make_tuple<RestT...>());
                        else return __make_tuple<RestT...>();
                    }
                }

                struct _make
                {
                    constexpr auto operator()() {
                        if constexpr (sizeof...(Args) == 0)
                            return std::make_tuple();
                        else return __make_tuple<Args...>();
                    }
                };

                static constexpr auto make() {
                    return _make{}();
                }

                using tuple_type = std::invoke_result_t<_make>;
            };
        };
    }

    template<class T>
    struct function_traits
        : function_traits<decltype(&std::decay_t<T>::operator())>
    {
    };

// mutable lambda
    template<class Ret, class Cls, class... Args>
    struct function_traits<Ret(Cls::*)(Args...)>
        : function_detail::types<Ret, Cls, std::true_type, std::true_type, Args...>
    {
    };

// immutable lambda
    template<class Ret, class Cls, class... Args>
    struct function_traits<Ret(Cls::*)(Args...) const>
        : function_detail::types<Ret, Cls, std::false_type, std::true_type, Args...>
    {
    };

// std::function
    template<class Ret, class... Args>
    struct function_traits<std::function<Ret(Args...)>>
        : function_detail::types<Ret, std::nullptr_t, std::true_type, std::false_type, Args...>
    {
    };

// c-function
    template<class Ret, class... Args>
    struct function_traits<std::function<Ret(*)(Args...)>>
        : function_detail::types<Ret, std::nullptr_t, std::true_type, std::false_type, Args...>
    {
    };
}
