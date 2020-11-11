#pragma once

#include <cppcoro/http/connection.hpp>
#include <cppcoro/net/make_socket.hpp>

namespace cppcoro::net
{
	struct use_ssl_t
	{
	} use_ssl;

	namespace impl
	{
		template<typename ConnectionT>
		struct connect_t;

		template<>
		struct connect_t<net::socket>
		{
			template<typename... ArgsT>
			task<net::socket>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT&&... args_v)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				auto socket =
					net::make_socket<net::socket_mode::client, net::socket>(service, endpoint);
				if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_token>(args));
				}
				else if constexpr (cppcoro::detail::in_tuple<
									   args_t,
									   std::reference_wrapper<cancellation_source>>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_source&>(args).token());
				}
				else
				{
					co_await socket.connect(endpoint);
				}
				co_return socket;
			}
		};

#ifdef CPPCORO_HTTP_MBEDTLS
		template<>
		struct connect_t<net::ssl::socket>
		{
			template<typename... ArgsT>
			task<net::ssl::socket>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT... args_v)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				auto socket = std::apply(
					net::make_socket<net::socket_mode::client, net::ssl::socket, ArgsT...>,
					std::tuple_cat(std::forward_as_tuple(service, endpoint), std::move(args)));
				if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_token>(args));
					co_await socket.encrypt(std::get<cancellation_token>(args));
				}
				else if constexpr (cppcoro::detail::in_tuple<
									   args_t,
									   std::reference_wrapper<cancellation_source>>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_source&>(args).token());
					co_await socket.encrypt(std::get<cancellation_source&>(args).token());
				}
				else
				{
					co_await socket.connect(endpoint);
					co_await socket.encrypt();
				}
				co_return socket;
			}
		};
#endif  // CPPCORO_HTTP_MBEDTLS

		template<net::is_connection ConnectionT>
		struct connect_t<ConnectionT>
		{
			using http_con_t =
				http::connection<typename ConnectionT::socket_type, ConnectionT::connection_mode>;

			template<typename... ArgsT>
			task<ConnectionT>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT... args_v) requires(
				not net::is_http_upgrade_connection<ConnectionT, http_con_t>)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				static_assert(
					(not cppcoro::detail::in_tuple<args_t, cancellation_token> and
					 not cppcoro::detail::
						 in_tuple<args_t, std::reference_wrapper<cancellation_source>>),
					"cancellation token is required");
				auto ct = [&]() {
					if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
					{
						return cancellation_token{ std::get<cancellation_token>(args) };
					}
					else if constexpr (
						cppcoro::detail::
							in_tuple<args_t, std::reference_wrapper<cancellation_source>> or
						cppcoro::detail::in_tuple<args_t, cancellation_source&>)
					{
						return std::get<cancellation_source&>(args).token();
					}
					else
					{
						static_assert(
							always_false_v<args_t>,
							"missing cancellation token or cancellation source ref");
					}
				};
				co_return ConnectionT{
					co_await std::apply(
						connect_t<typename ConnectionT::socket_type>{},
						std::tuple_cat(std::forward_as_tuple(service, endpoint), std::move(args))),
					ct()
				};
			}

			template<typename... ArgsT>
			task<ConnectionT> operator()(
				io_service& service,
				const ip_endpoint& endpoint,
				ArgsT... args_v) requires net::is_http_upgrade_connection<ConnectionT, http_con_t>
			{
				auto con = co_await std::invoke(
					connect_t<http_con_t>{}, service, endpoint, std::forward<ArgsT>(args_v)...);
				co_return co_await ConnectionT::from_http_connection(std::move(con));
			}
		};
	}  // namespace impl

	template<typename... ArgsT>
	auto connect(io_service& service, const ip_endpoint& endpoint, ArgsT... args)
	{
#ifdef CPPCORO_HTTP_MBEDTLS
		using args_t = std::tuple<ArgsT...>;
		if constexpr (cppcoro::detail::in_tuple<args_t, use_ssl_t>)
		{
#endif
			return impl::connect_t<ssl::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
#ifdef CPPCORO_HTTP_MBEDTLS
		}
		else if constexpr (
			cppcoro::detail::in_tuple<args_t, ssl::certificate> and
			cppcoro::detail::in_tuple<args_t, ssl::private_key>)
		{
			return impl::connect_t<ssl::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
		else
		{
			return impl::connect_t<net::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
#endif
	}

	template<net::is_connection ConnectionT, typename... ArgsT>
	auto connect(io_service& service, const net::ip_endpoint& endpoint, ArgsT... args)
	{
		using args_t = std::tuple<ArgsT...>;
		if constexpr (
			cppcoro::detail::in_tuple<args_t, use_ssl_t> or
			(cppcoro::detail::in_tuple<args_t, ssl::certificate> and
			 cppcoro::detail::in_tuple<args_t, ssl::private_key>))
		{
			return impl::connect_t<ConnectionT>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
		else
		{
			return impl::connect_t<ConnectionT>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
	}

}  // namespace cppcoro::net