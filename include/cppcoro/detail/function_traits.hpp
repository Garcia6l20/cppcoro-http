#pragma once

#include <functional>

namespace cppcoro::detail
{
	struct parameters_tuple_all_enabled
	{
		template<typename T>
		static constexpr bool value = true;
	};

	template<typename... ArgsT>
	struct parameters_tuple_disable
	{
		template<typename T>
		static constexpr bool value = !std::disjunction_v<
			std::is_same<std::remove_cvref_t<T>, std::remove_cvref_t<ArgsT>>...>;
	};
	template<typename... ArgsT>
	struct parameters_tuple_disable<std::tuple<ArgsT...>> : parameters_tuple_disable<ArgsT...>
	{
	};

	namespace function_detail
	{
		template<
			typename Ret,
			typename Cls,
			bool IsMutable,
			bool IsLambda,
			typename... Args>
		struct types : std::true_type
		{
			static constexpr bool is_mutable = IsMutable;
            static constexpr bool is_lambda = IsLambda;

			static constexpr bool is_function = not is_lambda;

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
			using args_tuple = std::tuple<Args...>;

			template<typename Predicate = parameters_tuple_all_enabled>
			struct parameters_tuple
			{
				template<typename FirstT, typename... RestT>
				static constexpr auto __make_tuple()
				{
					if constexpr (!sizeof...(RestT))
					{
						if constexpr (Predicate::template value<FirstT>)
						{
							return std::make_tuple<FirstT>({});
						}
						else
						{
							return std::tuple();
						}
					}
					else
					{
						if constexpr (Predicate::template value<FirstT>)
						{
							return std::tuple_cat(
								std::make_tuple<FirstT>({}), __make_tuple<RestT...>());
						}
						else
						{
							return __make_tuple<RestT...>();
						}
					}
				}

				struct _make
				{
					constexpr auto operator()()
					{
						if constexpr (sizeof...(Args) == 0)
							return std::make_tuple();
						else
							return __make_tuple<Args...>();
					}
				};

				static constexpr auto make() { return _make{}(); }

				using tuple_type = std::invoke_result_t<_make>;
			};
		};
	}  // namespace function_detail

	// primary template: not a function
	template<class T, typename = void>
	struct function_traits : std::false_type
	{
	};

	// default resolution with operator()
	// forwards to corresponding
    template<class T>
    struct function_traits<T, std::void_t<decltype(&T::operator())>>
        : function_traits<decltype(&T::operator())>
    {
    };

	// callable
	template<class Ret, class Cls, class... Args>
	struct function_traits<Ret (Cls::*)(Args...)>
		: function_detail::types<Ret, Cls, true, true, Args...>
	{
	};

	// noexcept callable
	template<class Ret, class Cls, class... Args>
	struct function_traits<Ret (Cls::*)(Args...) noexcept>
		: function_detail::types<Ret, Cls, true, true, Args...>
	{
	};

	// const callable
	template<class Ret, class Cls, class... Args>
	struct function_traits<Ret (Cls::*)(Args...) const>
		: function_detail::types<Ret, Cls, false, true, Args...>
	{
	};

	// const noexcept callable
	template<class Ret, class Cls, class... Args>
	struct function_traits<Ret (Cls::*)(Args...) const noexcept>
		: function_detail::types<Ret, Cls, false, true, Args...>
	{
	};

	// function
	template<class Ret, class... Args>
	struct function_traits<std::function<Ret(Args...)>>
		: function_detail::types<Ret, std::nullptr_t, true, true, Args...>
	{
	};

	// c-style function
	template<class Ret, class... Args>
	struct function_traits<Ret (*)(Args...)>
		: function_detail::types<Ret, std::nullptr_t, true, false, Args...>
	{
	};

	template<typename T>
	constexpr bool is_callable_v = function_traits<T>::value;

}  // namespace cppcoro::detail
