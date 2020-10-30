/**
 * @file cppcoro/tcp.hpp
 */
#pragma once

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/ssl/concepts.hpp>

#include <span>
#include <utility>

namespace cppcoro
{
	namespace net
	{
		template<bool bind = true>
		auto create_tcp_socket(io_service& ios, const ip_endpoint& endpoint)
		{
			auto sock = socket{ endpoint.is_ipv4() ? socket::create_tcpv4(ios)
												   : socket::create_tcpv6(ios) };
			if constexpr (bind)
			{
				sock.bind(endpoint);
			}
			return sock;
		}
	}  // namespace net
	namespace tcp
	{
		template<net::is_cancelable_socket SocketT>
		class connection
		{
		public:
			connection(connection&& other) noexcept
				: sock_{ std::move(other.sock_) }
				, ct_{ std::move(other.ct_) }
			{
			}

			connection(const connection&) = delete;

			connection(SocketT socket, cancellation_token ct)
				: sock_{ std::move(socket) }
				, ct_{ std::move(ct) }
			{
			}

			[[nodiscard]] const net::ip_endpoint& peer_address() const
			{
				return sock_.remote_endpoint();
			}

			decltype(auto) receive(auto data)
			{
				return sock_.recv(reinterpret_cast<void*>(data.data()), data.size_bytes(), ct_);
			}

			decltype(auto) receive(void* buffer, size_t size)
			{
				return sock_.recv(buffer, size, { ct_ });
			}

			decltype(auto) send(auto data)
			{
				return sock_.send(
					reinterpret_cast<const void*>(data.data()), data.size_bytes(), ct_);
			}

			decltype(auto) send(const void* buffer, size_t size)
			{
				return sock_.send(buffer, size, ct_);
			}

			decltype(auto) disconnect() { return sock_.disconnect(); }
			decltype(auto) close_send() { return sock_.close_send(); }

			[[nodiscard]] auto& socket() { return sock_; }

		protected:
			SocketT sock_;
			cancellation_token ct_;
		};

		struct ipv4_socket_provider
		{
			using listening_socket_type = net::socket;
            using connection_socket_type = net::socket;
            static listening_socket_type create_listening_sock(io_service& ios)
			{
				return net::socket::create_tcpv4(ios);
			}
            static connection_socket_type create_connection_sock(io_service& ios)
			{
				return create_listening_sock(ios);
			}
		};

		template<net::is_socket_provider SocketProviderT = ipv4_socket_provider>
		class server
		{
		public:
			using connection_socket_type =
				typename SocketProviderT::connection_socket_type;
			using connection_type = connection<connection_socket_type>;

			server(server&& other) noexcept
				: ios_{ other.ios_ }
				, endpoint_{ std::move(other.endpoint_) }
				, socket_{ std::move(other.socket_) }
				, cs_{ other.cs_ }
			{
			}

			server(const server&) = delete;

			~server() noexcept = default;

			server(io_service& ios, const net::ip_endpoint& endpoint)
				: ios_{ ios }
				, endpoint_{ endpoint }
				, socket_{ SocketProviderT::create_listening_sock(ios) }
			{
				socket_.bind(endpoint_);
				socket_.listen();
			}

			/** @brief Accept next incoming connection.
			 *
			 * @return The connection.
			 */
			task<connection_type> accept()
			{
				auto sock = SocketProviderT::create_connection_sock(ios_);
				co_await socket_.accept(sock, cs_.token());
				if constexpr (net::ssl::is_socket<connection_socket_type>)
				{
					co_await sock.encrypt(cs_.token());
				}
				co_return connection{ std::move(sock), cs_.token() };
			}

			decltype(auto) disconnect() { return socket_.disconnect(cs_.token()); }

			/** @brief Stop the server.
			 *
			 */
			void stop() { cs_.request_cancellation(); }

			/** @brief Obtain cancellation token from server.
			 *
			 * @return A cancellation token.
			 */
			auto token() noexcept { return cs_.token(); }

			auto& service() noexcept { return ios_; }

			auto const& local_endpoint() const noexcept { return socket_.local_endpoint(); }

		protected:
			io_service& ios_;
			net::ip_endpoint endpoint_;
			net::socket socket_;
			cancellation_source cs_;
		};

		template<net::is_connection_socket_provider SocketProviderT = ipv4_socket_provider>
		class client
		{
		public:
			using connection_type =
				connection<typename SocketProviderT::connection_socket_type>;
			client(client&& other) noexcept
				: ios_{ other.ios_ }
				, cs_{ other.cs_ }
			{
			}

			client(const client&) = delete;

			client(io_service& ios)
				: ios_{ ios }
			{
			}

			task<connection_type> connect(net::ip_endpoint const& endpoint)
			{
				auto sock = SocketProviderT::create_connection_sock(ios_);
				co_await sock.connect(endpoint, cs_.token());
				if constexpr (requires { sock.encrypt(cs_.token()); })
				{
					co_await sock.encrypt(cs_.token());
				}
				co_return connection{ std::move(sock), cs_.token() };
			}

			void stop() { cs_.request_cancellation(); }

			auto& service() noexcept { return ios_; }

		protected:
			io_service& ios_;
			cancellation_source cs_;
		};
	}  // namespace tcp
}  // namespace cppcoro