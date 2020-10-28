/** @file cppcoro/concepts.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cstddef>

#include <cppcoro/awaitable_traits.hpp>

namespace cppcoro
{
	namespace detail
	{
		struct any_return_type
		{
		};
	}  // namespace detail

	template<typename T, typename ReturnT = detail::any_return_type>
	concept awaitable = requires(T)
	{
		typename awaitable_traits<T>::awaiter_t;
		typename awaitable_traits<T>::await_result_t;
		requires(
			std::same_as<ReturnT, detail::any_return_type> or
			std::same_as<ReturnT, typename awaitable_traits<T>::await_result_t>);
	};

}  // namespace cppcoro
