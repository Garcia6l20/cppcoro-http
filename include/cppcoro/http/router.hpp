#pragma once

#include <cppcoro/router/router.hpp>

namespace cppcoro::http
{
	namespace detail
	{
		template<auto pattern, typename HandlerT>
		struct handler : cppcoro::router::detail::handler<pattern, HandlerT>
		{
			using base = cppcoro::router::detail::handler<pattern, HandlerT>;

			handler(http::method method, HandlerT&& handler) noexcept
				: base{ std::forward<HandlerT>(handler) }
				, method_{ method }
			{
			}
			using base::matches;
			template<typename ContextT, typename ArgsT>
			std::optional<typename base::result_t>
			operator()(ContextT& context, std::string_view path, ArgsT&& args)
			{
				if (std::get<http::method>(args) == method_)
				{
					return base::operator()(context, path, std::forward<ArgsT>(args));
				}
				else
				{
					return {};
				}
			}

		private:
			http::method method_;
		};
	}  // namespace detail

	namespace route
	{
		template<ctll::fixed_string pattern, http::method method, typename HandlerT>
		constexpr auto on(HandlerT&& handler)
		{
			return cppcoro::http::detail::handler<pattern, HandlerT>{
				method, std::forward<HandlerT>(handler)
			};
		}
		template<ctll::fixed_string pattern, typename HandlerT>
		constexpr auto get(HandlerT&& handler) noexcept
		{
			return cppcoro::http::detail::handler<pattern, HandlerT>{
				http::method::get, std::forward<HandlerT>(handler)
			};
		}
		template<ctll::fixed_string pattern, typename HandlerT>
		constexpr auto post(HandlerT&& handler) noexcept
		{
			return cppcoro::http::detail::handler<pattern, HandlerT>{
				http::method::post, std::forward<HandlerT>(handler)
			};
		}
	}  // namespace route
}  // namespace cppcoro::http
