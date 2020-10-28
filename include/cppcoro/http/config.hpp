/** @file cppcoro/http/concepts.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/concepts.hpp>
#include <cppcoro/net/concepts.hpp>
#include <cppcoro/net/tcp.hpp>
#include <type_traits>

namespace cppcoro::http
{
	namespace detail
	{
		template<cppcoro::net::is_socket_provider SocketProvider = tcp::ipv4_socket_provider>
		struct base_config
		{
			using socket_provider = SocketProvider;
			using connection_socket_type = decltype(
				std::declval<SocketProvider>().create_connection_sock(std::declval<io_service&>()));
			using session_type = void;  // force concept pass
		};
	}  // namespace detail

	template<
		template<typename> typename SessionT,
		cppcoro::net::is_socket_provider SocketProviderT = tcp::ipv4_socket_provider>
	struct config : detail::base_config<SocketProviderT>
	{
		using session_type = SessionT<config>;
	};

}  // namespace cppcoro::http
