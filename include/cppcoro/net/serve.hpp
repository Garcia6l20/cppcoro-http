#pragma once

#include <cppcoro/async_scope.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/concepts.hpp>
#include <cppcoro/net/ip_endpoint.hpp>
#include <cppcoro/net/make_socket.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/detail/function_traits.hpp>

#ifdef CPPCORO_HTTP_MBEDTLS
#include <cppcoro/ssl/concepts.hpp>
#endif

namespace cppcoro::net
{
	template<typename HandlerT, typename... ArgsT>
	task<> serve(
		io_service& io_service,
		const ip_endpoint& endpoint,
		HandlerT connection_handler,
		ArgsT&&... args_v)
	{
		auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
		using args_t = decltype(args);
		using handler_traits = typename cppcoro::detail::function_traits<HandlerT>;
		using connection_type = typename handler_traits::template arg<0>::clean_type;
		using socket_type = std::conditional_t<
			is_connection<connection_type>,
			typename connection_type::socket_type,
			connection_type>;
		using handler_args_type =
			cppcoro::detail::tuple_pop_front_t<typename handler_traits::args_tuple>;

		std::exception_ptr exception_ptr{};
		is_socket auto server = net::make_socket<socket_mode::server>(io_service, endpoint);
		async_scope scope{};
		try
		{
			server.bind(endpoint);
			server.listen();
			auto make_socket = [&]() {
				return std::apply(
					net::make_socket<socket_mode::server_connection, socket_type, ArgsT...>,
					std::tuple_cat(std::forward_as_tuple(io_service, endpoint), args));
			};
			auto accept = [&](auto& sock) -> task<> {
				if constexpr (cppcoro::detail::in_tuple<std::tuple<ArgsT...>, cancellation_token>)
				{
					co_await server.accept(sock, std::get<cancellation_token>(args));
				}
				else if constexpr (cppcoro::detail::in_tuple<
									   std::tuple<ArgsT...>,
									   std::reference_wrapper<cancellation_source>>)
				{
					co_await server.accept(sock, std::get<cancellation_source&>(args).token());
				}
				else
				{
					co_await server.accept(sock);
				}
			};
			auto is_done = [&] {
				if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
				{
					if constexpr (cppcoro::detail::in_tuple<
									  std::tuple<ArgsT...>,
									  std::reference_wrapper<cancellation_source>>)
					{
						return std::get<cancellation_source&>(args).is_cancellation_requested();
					}
					else
					{
						return std::get<cancellation_token>(args).is_cancellation_requested();
					}
				}
				else
				{
					return false;  // endless
				}
			};
			auto ct = [](auto&& args) {
				if constexpr (cppcoro::detail::in_tuple<
								  std::tuple<ArgsT...>,
								  std::reference_wrapper<cancellation_source>>)
				{
					return std::get<cppcoro::cancellation_source&>(args).token();
				}
				else
				{
					return std::get<cancellation_token>(args);
				}
			};
			auto make_connection = [ct](socket_type&& socket, auto& args) -> task<connection_type> {
				if constexpr (is_connection<connection_type>)
				{
					using http_con_t = http::connection<socket_type, connection_mode::server>;
					if constexpr (is_http_upgrade_connection<connection_type, http_con_t>)
					{
						co_return co_await connection_type::from_http_connection(
							http_con_t{ std::forward<socket_type>(socket), ct(args) });
					}
					else
					{
						co_return connection_type{ std::forward<socket_type>(socket), ct(args) };
					}
				}
				else
				{
					co_return std::forward<socket_type>(socket);
				}
			};
			auto spawner = [](auto sock,
                              auto make_connection,
							  auto handler,
							  std::reference_wrapper<args_t> args) -> task<> {

				if constexpr (CPPCORO_HTTP_HAS_SSL and ssl::is_socket<socket_type>)
				{
					co_await sock.encrypt();
				}
				try
				{
					co_await std::apply(
						handler,
						std::tuple_cat(
							std::make_tuple(co_await make_connection(std::move(sock), args.get())),
							cppcoro::detail::tuple_generate([&]<size_t index>() {
								if constexpr (std::tuple_size_v<handler_args_type> > index)
								{
									using element_t =
										std::tuple_element_t<index, handler_args_type>;
									if constexpr (std::is_rvalue_reference_v<element_t>)
									{
										return std::get<std::remove_reference_t<element_t>>(
											args.get());
									}
									else
									{
										return std::get<element_t>(args);
									}
								}
							})));
				}
				catch (operation_cancelled&)
				{
				}
			};
			while (not is_done())
			{
				auto conn_socket = make_socket();
				co_await accept(conn_socket);
				scope.spawn(spawner(std::move(conn_socket), make_connection, connection_handler, std::ref(args)));
			}
		}
		catch (operation_cancelled&)
		{
		}
		catch (...)
		{
			exception_ptr = std::current_exception();
		}
		co_await scope.join();
		if (exception_ptr)
		{
			std::rethrow_exception(exception_ptr);
		}
	}

}  // namespace cppcoro::net