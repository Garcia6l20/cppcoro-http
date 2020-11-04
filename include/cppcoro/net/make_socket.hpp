#pragma once

#include <cppcoro/kw_args.hpp>
#include <cppcoro/detail/is_specialization.hpp>
#include <cppcoro/detail/tuple_utils.hpp>

namespace cppcoro::net
{
	enum class address_familly
	{
		ipv4,
		ipv6
	};

	enum class socket_proto
	{
		udp,
		tcp
	};

	enum class socket_mode
	{
		client,
		server,
		server_connection
	};
	//	template<typename DerivedT>
	//	struct crtp
	//	{
	//		constexpr decltype(auto) invoke(auto&& fn, auto&&... args)
	//		{
	//			return std::invoke(
	//				std::forward<decltype(fn)>(fn),
	//				static_cast<DerivedT*>(this),
	//				std::forward<decltype(args)>(args)...);
	//		}
	//	};

	namespace ssl_args
	{
		using certificate = net::ssl::certificate;
		using private_key = net::ssl::private_key;
		struct host_name : std::string_view
		{
		};
		using verify_flags = net::ssl::verify_flags;
		using peer_verify_mode = net::ssl::peer_verify_mode;
	}  // namespace ssl_args

	namespace detail
	{
		template<typename T>
		struct always_false
		{
			bool value = false;
		};
		template<typename T>
		constexpr auto always_false_v = always_false<T>::value;
	}  // namespace detail

	template<socket_mode mode, typename SocketT = void>
	struct make_socket_t
	{
		socket_proto proto;
		address_familly af;
	};

	template<socket_mode mode>
	struct make_socket_t<mode, net::socket> : make_socket_t<mode>
	{
		template<typename... Args>
		decltype(auto) operator()(io_service& service, Args&&...)
		{
			if (this->proto == socket_proto::tcp and this->af == address_familly::ipv4)
			{
				return net::socket::create_tcpv4(service);
			}
			else if (this->proto == socket_proto::tcp and this->af == address_familly::ipv6)
			{
				return net::socket::create_tcpv6(service);
			}
			else if (this->proto == socket_proto::udp and this->af == address_familly::ipv4)
			{
				return net::socket::create_udpv4(service);
			}
			else if (this->proto == socket_proto::udp and this->af == address_familly::ipv6)
			{
				return net::socket::create_udpv6(service);
			}
			else
			{
				throw std::runtime_error("the impossible append");
			}
		}
	};
	template<socket_mode mode>
	struct make_socket_t<mode, net::ssl::socket> : make_socket_t<mode>
	{
		decltype(auto) apply_args(is_socket auto&& socket, auto args) noexcept
		{
			using args_t = decltype(args);
			if constexpr (cppcoro::detail::in_tuple<args_t, ssl_args::host_name>)
			{
				socket.host_name(std::get<ssl_args::host_name>(args));
			}
			if constexpr (cppcoro::detail::in_tuple<args_t, ssl::verify_flags>)
			{
				socket.set_verify_flags(std::get<ssl::verify_flags>(args));
			}
			if constexpr (cppcoro::detail::in_tuple<args_t, ssl::peer_verify_mode>)
			{
				socket.set_peer_verify_mode(std::get<ssl::peer_verify_mode>(args));
			}
			return std::forward<decltype(socket)>(socket);
		}

		template<typename... Args>
		decltype(auto) operator()(io_service& service, Args&&... args_v)
		{
			auto args = std::make_tuple(std::forward<Args>(args_v)...);
			using args_t = decltype(args);
			if constexpr (mode == socket_mode::server)
			{
				return make_socket_t<mode, net::socket>{}(service);
			}
			else
			{
				if constexpr (mode == socket_mode::server_connection)
				{
					static_assert(
						cppcoro::detail::in_tuple<args_t, ssl::certificate>, "missing certificate");
					static_assert(
						cppcoro::detail::in_tuple<args_t, ssl::private_key>, "missing private key");
					if (this->proto == socket_proto::tcp and this->af == address_familly::ipv6)
					{
						return apply_args(
							net::ssl::socket::create_server_v6(
								service,
								std::move(std::get<ssl::certificate>(args)),
								std::move(std::get<ssl::private_key>(args))),
							std::move(args));
					}
					else if (this->proto == socket_proto::tcp and this->af == address_familly::ipv4)
					{
						auto sock = net::ssl::socket::create_server(
							service,
							std::move(std::get<ssl::certificate>(args)),
							std::move(std::get<ssl::private_key>(args)));
						return apply_args(std::move(sock), std::move(args));
					}
				}
				else if constexpr (mode == socket_mode::client)
				{
					if (this->proto == socket_proto::tcp and this->af == address_familly::ipv6)
					{
						if constexpr (
							cppcoro::detail::in_tuple<args_t, ssl::certificate> and
							cppcoro::detail::in_tuple<args_t, ssl::private_key>)
						{
							return apply_args(
								net::ssl::socket::create_client_v6(
									service,
									std::get<ssl::certificate>(args),
									std::get<ssl::private_key>(args)),
								std::move(args));
						}
						else
						{
							return apply_args(
								net::ssl::socket::create_client_v6(service), std::move(args));
						}
					}
					else if (this->proto == socket_proto::tcp and this->af == address_familly::ipv4)
					{
						if constexpr (
							cppcoro::detail::in_tuple<args_t, ssl::certificate> and
							cppcoro::detail::in_tuple<args_t, ssl::private_key>)
						{
							return apply_args(net::ssl::socket::create_client(
								service,
								std::get<ssl::certificate>(args),
								std::get<ssl::private_key>(args)));
						}
						else
						{
							return apply_args(
								net::ssl::socket::create_client(service), std::move(args));
						}
					}
				}
				std::abort();
			}
		}
	};

	template<typename T, typename ToT>
	concept decays_to = std::same_as<std::decay_t<T>, ToT>;

	template<
		socket_mode mode = socket_mode::client,
		is_socket SocketT = net::socket,
		typename... Args>
	static SocketT make_socket(io_service& executor, ip_endpoint const& endpoint, Args&&... args_v)
	{
		socket_proto proto = socket_proto::tcp;
		auto args = std::make_tuple(std::forward<Args>(args_v)...);
		if constexpr (cppcoro::detail::in_tuple<decltype(args), socket_proto>)
		{
			proto = std::get<socket_proto>(args);
		}

		if (endpoint.is_ipv6())
		{
			return std::apply(
				make_socket_t<mode, SocketT>{ proto, address_familly::ipv6 },
				std::tuple_cat(std::forward_as_tuple(executor), std::move(args)));
		}
		else
		{
			return std::apply(
				make_socket_t<mode, SocketT>{ proto, address_familly::ipv4 },
				std::tuple_cat(std::forward_as_tuple(executor), std::move(args)));
		}
	}
}  // namespace cppcoro::net
