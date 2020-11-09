/**
 * @file cppcoro/tcp.hpp
 */
#pragma once

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/net/connection.hpp>
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
		template<net::is_cancelable_socket SocketT, net::connection_mode _mode>
		class connection
		{
		public:
			using socket_type = SocketT;

			static constexpr auto connection_mode = _mode;

            template<net::message_direction dir = net::message_direction::incoming>
            using message_type = net::message<SocketT, dir>;

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

			template<typename T, size_t extent = std::dynamic_extent>
			task<size_t> receive_all(std::span<T, extent> data)
			{
				std::size_t totalBytesReceived = 0;
				std::size_t bytesReceived = 0;
				auto bytes = std::as_writable_bytes(data);
				do
				{
					bytesReceived = co_await sock_.recv(
						bytes.data() + totalBytesReceived, bytes.size() - totalBytesReceived, ct_);
					totalBytesReceived += bytesReceived;
				} while (bytesReceived > 0 && totalBytesReceived < bytes.size());
				co_return totalBytesReceived;
			}

            template<typename T, size_t extent = std::dynamic_extent>
            task<size_t> receive(std::span<T, extent> data)
            {
                auto bytes = std::as_writable_bytes(data);
                co_return co_await sock_.recv(
                        bytes.data(), bytes.size_bytes(), ct_);
            }

			template<typename T, size_t extent = std::dynamic_extent>
			task<size_t> send(std::span<T, extent> data)
			{
				std::size_t bytesSent = 0;
				auto bytes = std::as_bytes(data);
				do
				{
					bytesSent += co_await sock_.send(
						bytes.data() + bytesSent, bytes.size() - bytesSent, ct_);
				} while (bytesSent < bytes.size());
				co_return bytesSent;
			}

			decltype(auto) disconnect() {

                try
                {
					sock_.close_send();
                }
                catch (std::system_error&)
                {
                    // ignore close errors
                }
				return sock_.disconnect();
			}
			decltype(auto) close_send() { return sock_.close_send(); }

			[[nodiscard]] auto& socket() noexcept { return sock_; }
			[[nodiscard]] auto token() const noexcept { return ct_; }

		protected:
			SocketT sock_;
			cancellation_token ct_;
		};

        template <net::is_socket SocketT>
        using server_connection = connection<SocketT, net::connection_mode::server>;

		template <net::is_socket SocketT>
		using client_connection = connection<SocketT, net::connection_mode::client>;

	}  // namespace tcp

}  // namespace cppcoro