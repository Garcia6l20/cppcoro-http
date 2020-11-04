#pragma once

#include <cppcoro/net/concepts.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/ssl/socket.hpp>
#include <variant>

namespace cppcoro::detail
{
	template<typename T>
	concept is_tuple = specialization_of<T, std::tuple>;

	template<typename T, typename Tuple>
	struct tuple_contains;

	template<typename T, typename... Us>
	struct tuple_contains<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...>
	{
	};

	template<typename TupleT, typename T>
	concept in_tuple = is_tuple<TupleT>and tuple_contains<T, TupleT>::value;

	template<typename TupleT>
	struct tuple_to_variant
	{
	};

	template<typename... TypesT>
	struct tuple_to_variant<std::tuple<TypesT...>>
	{
		using type = std::variant<TypesT...>;
	};

	namespace impl
	{
		template<size_t begin_, size_t... indices_, typename Tuple>
		auto tuple_slice_impl(Tuple&& tuple, std::index_sequence<indices_...>)
		{
			return std::make_tuple(std::get<begin_ + indices_>(std::forward<Tuple>(tuple))...);
		}
	}  // namespace impl

	template<size_t begin_, size_t count_, typename Tuple>
	auto tuple_slice(Tuple&& tuple)
	{
		static_assert(count_ > 0, "slicing tuple to 0-length is weird...");
		return impl::tuple_slice_impl<begin_>(
			std::forward<Tuple>(tuple), std::make_index_sequence<count_>());
	}

	constexpr decltype(auto) tuple_push_front(is_tuple auto&& tuple, auto&&... elem)
	{
		return std::tuple_cat(
			std::make_tuple(std::forward<decltype(elem)>(elem))...,
			std::forward<decltype(tuple)>(tuple));
	}

	namespace impl {
		template <typename TupleT>
		struct tuple_pop_front;

        template <typename FirstT, typename ...RestT>
        struct tuple_pop_front<std::tuple<FirstT, RestT...>> {
			using type = std::tuple<RestT...>;
		};
	}

	template <is_tuple TupleT>
	using tuple_pop_front_t = typename impl::tuple_pop_front<TupleT>::type;

	constexpr decltype(auto) tuple_push_back(is_tuple auto&& tuple, auto&&... elem)
	{
		return std::tuple_cat(
			std::forward<decltype(tuple)>(tuple),
			std::make_tuple(std::forward<decltype(elem)>(elem))...);
	}

	template<template<class...> class ToT, typename TupleT>
	struct tuple_convert;

	template<template<class...> class ToT, typename... TypesT>
	struct tuple_convert<ToT, std::tuple<TypesT...>>
	{
		using type = ToT<TypesT...>;
	};

	struct break_t
	{
		bool break_ = false;
		constexpr explicit operator bool() const { return break_; }
	};

	constexpr break_t break_{ .break_ = true };
	constexpr break_t continue_{ .break_ = false };

	namespace impl
	{
		template<typename T, typename... Args>
		concept is_index_invocable = requires(T v, Args... args)
		{
			{ v.template operator()<std::size_t(42)>(args...) };
		};
		template<typename T, typename... Args>
		concept is_index_invocable_with_params = requires(T v, Args... args)
		{
			{ v.template operator()<std::size_t(42), Args...>() };
		};

		template<size_t index = 0, typename TupleT, typename LambdaT>
		constexpr auto for_each(TupleT& tuple, LambdaT&& lambda)
		{
			if constexpr (std::tuple_size_v<TupleT> == 0)
			{
				return;
			}
			else
			{
				using ValueT = std::decay_t<decltype(std::get<index>(tuple))>;
				auto invoker = [&]() mutable {
					if constexpr (is_index_invocable<decltype(lambda), ValueT>)
					{
						return lambda.template operator()<index>(std::get<index>(tuple));
					}
					else
					{
						return lambda(std::get<index>(tuple));
					}
				};
				using ReturnT = decltype(invoker());
				if constexpr (std::is_void_v<ReturnT>)
				{
					invoker();
					if constexpr (index + 1 < std::tuple_size_v<TupleT>)
					{
						return for_each<index + 1>(tuple, std::forward<LambdaT>(lambda));
					}
				}
				else if constexpr (std::same_as<ReturnT, break_t>)
				{
					// non-constexpr break
					if constexpr (index >= std::tuple_size_v<TupleT>)
					{
						return;  // did not break
					}
					else
					{
						if (invoker())
						{
							return;
						}
						else
						{
							if constexpr (index + 1 < std::tuple_size_v<TupleT>)
							{
								for_each<index + 1>(tuple, std::forward<LambdaT>(lambda));
							}
						}
					}
				}
				else
				{
					return invoker();
				}
			}
		}

		template<
			typename TupleT,
			size_t index = 0,
			typename LambdaT,
			typename CurrentT = std::tuple<>>
		constexpr decltype(auto) transform(LambdaT&& lambda, CurrentT current = {})
		{
			using ValueT = std::decay_t<std::tuple_element_t<index, TupleT>>;
			auto invoker = [&] {
				if constexpr (is_index_invocable<decltype(lambda), ValueT>)
				{
					return lambda.template operator()<index>(ValueT{});
				}
				else if constexpr (std::invocable<decltype(lambda), ValueT>)
				{
					return lambda(ValueT{});
				}
				else
				{
					return lambda.template operator()<ValueT>();
				}
			};
			using ReturnT = decltype(invoker());
			auto then = [&](auto&& next) {
				return transform<TupleT, index + 1>(
					std::forward<decltype(lambda)>(lambda), std::forward<decltype(next)>(next));
			};
			if constexpr (std::is_void_v<ReturnT>)
			{
				invoker();
				if constexpr (index + 1 < std::tuple_size_v<TupleT>)
				{
					return then(std::move(current));
				}
				else
				{
					return current;
				}
			}
			else
			{
				auto next = tuple_push_back(std::move(current), invoker());
				if constexpr (index + 1 < std::tuple_size_v<TupleT>)
				{
					return then(std::move(next));
				}
				else
				{
					return next;
				}
			}
		}

		template<size_t index = 0, typename LambdaT, typename CurrentT = std::tuple<>>
		constexpr decltype(auto) generate(LambdaT&& lambda, CurrentT current = {})
		{
			auto invoker = [&]() mutable { return lambda.template operator()<index>(); };
			using ReturnT = decltype(invoker());
			if constexpr (std::is_void_v<ReturnT>)
			{
				return current;
			}
			else
			{
				auto next = tuple_push_back(std::move(current), invoker());
				return generate<index + 1>(std::forward<decltype(lambda)>(lambda), std::move(next));
			}
		}

	}  // namespace impl

	constexpr decltype(auto) for_each(is_tuple auto& tuple, auto&& functor)
	{
		return impl::for_each(tuple, std::forward<decltype(functor)>(functor));
	}

	template<is_tuple TupleT>
	constexpr decltype(auto) tuple_transform(auto&& generator)
	{
		return impl::transform<TupleT>(std::forward<decltype(generator)>(generator));
	}

	template<is_tuple TupleT, auto lambda>
	using tuple_transform_t = std::decay_t<decltype(
		tuple_transform<TupleT>(std::declval<std::decay_t<decltype(lambda)>>()))>;

    template <impl::is_index_invocable LambdaT>
    constexpr auto tuple_generate(LambdaT &&lambda) {
        return detail::impl::generate(std::forward<decltype(lambda)>(lambda));
    }

}  // namespace cppcoro::detail