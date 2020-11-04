#pragma once

#include <cppcoro/net/concepts.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/ssl/socket.hpp>
#include <cppcoro/detail/tuple_utils.hpp>
#include <variant>

namespace cppcoro::net
{
	namespace config
	{
		using socket_types = std::tuple<net::socket, net::ssl::socket>;
	}

	namespace detail
	{
		using socket_variant = typename cppcoro::detail::tuple_convert<
			std::variant,
			cppcoro::detail::tuple_push_front<config::socket_types, std::monostate>::type>::type;
	}  // namespace detail

	// clang-format off
struct tcp_socket_t {} tcp_socket;
struct udp_socket_t {} udp_socket;
struct ssl_server_socket_t {} ssl_server_socket;
struct ssl_client_socket_t {} ssl_client_socket;
	// clang-format on

	template<typename... SockTs>
	using any_socket = std::variant<SockTs...>;

	inline net::socket
	make_socket(net::tcp_socket_t, io_service& io_service, const ip_endpoint& endpoint)
	{
		std::optional<net::socket> sock{};
		if (endpoint.is_ipv6())
		{
			sock.emplace(net::socket::create_tcpv6(io_service));
		}
		else
		{
			sock.emplace(net::socket::create_tcpv4(io_service));
		}
		sock->bind(endpoint);
		return std::move(sock).value();
	}

	inline net::socket
	make_socket(net::udp_socket_t, io_service& io_service, net::ip_endpoint const& endpoint)
	{
		std::optional<net::socket> sock{};
		if (endpoint.is_ipv6())
		{
			sock.emplace<net::socket>(net::socket::create_udpv6(io_service));
		}
		else
		{
			sock.emplace<net::socket>(net::socket::create_udpv4(io_service));
		}
		sock->bind(endpoint);
		return std::move(sock).value();
	}

	inline net::ssl::socket
	make_socket(net::ssl_client_socket_t, io_service& io_service, net::ip_endpoint const& endpoint)
	{
		std::optional<net::ssl::socket> sock{};
		if (endpoint.is_ipv6())
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_client_v6(io_service));
		}
		else
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_client(io_service));
		}
		sock->bind(endpoint);
		return std::move(sock).value();
	}

	inline net::ssl::socket make_socket(
		net::ssl_client_socket_t,
		io_service& io_service,
		net::ip_endpoint const& endpoint,
		ssl::certificate&& certificate,
		ssl::private_key&& pk)
	{
		std::optional<net::ssl::socket> sock{};
		if (endpoint.is_ipv6())
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_client_v6(
				io_service,
				std::forward<ssl::certificate>(certificate),
				std::forward<ssl::private_key>(pk)));
		}
		else
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_client(
				io_service,
				std::forward<ssl::certificate>(certificate),
				std::forward<ssl::private_key>(pk)));
		}
		sock->bind(endpoint);
		return std::move(sock).value();
	}

	inline net::ssl::socket make_socket(
		net::ssl_server_socket_t,
		io_service& io_service,
		net::ip_endpoint const& endpoint,
		ssl::certificate&& certificate,
		ssl::private_key&& pk)
	{
		std::optional<net::ssl::socket> sock{};
		if (endpoint.is_ipv6())
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_server_v6(
				io_service,
				std::forward<ssl::certificate>(certificate),
				std::forward<ssl::private_key>(pk)));
		}
		else
		{
			sock.emplace<net::ssl::socket>(net::ssl::socket::create_server(
				io_service,
				std::forward<ssl::certificate>(certificate),
				std::forward<ssl::private_key>(pk)));
		}
		sock->bind(endpoint);
		return std::move(sock).value();
	}

}  // namespace cppcoro::net
