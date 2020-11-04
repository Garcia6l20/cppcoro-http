#pragma once

#include <cppcoro/net/concepts.hpp>
#include <cppcoro/task.hpp>

#include <span>

namespace cppcoro::net
{
	using readable_bytes = std::span<const std::byte, std::dynamic_extent>;
	using writeable_bytes = std::span<std::byte, std::dynamic_extent>;

	enum class message_direction
	{
		incoming,
		outgoing
	};
	template<is_socket SocketT, message_direction direction, typename HeaderT = void>
	class message
	{
		using bytes_type = std::conditional_t<
			direction == message_direction::incoming,
			writeable_bytes,
			readable_bytes>;

	public:
		message(SocketT& socket, bytes_type bytes, cancellation_token ct) noexcept
			: socket_{ socket }
			, ct_{ std::move(ct) }
		    , bytes_{bytes}
		{
		}

		~message()
		{
			try
			{
				if constexpr (direction == message_direction::outgoing)
				{
					socket_.close_send();
				}
				else
				{
					// socket_.close_recv();
				}
			}
			catch (std::system_error&)
			{
				// ignore close errors
			}
		}

		task<size_t> send(size_t size = std::numeric_limits<size_t>::max()) requires(direction == message_direction::outgoing)
		{
			if (size == std::numeric_limits<size_t>::max()) {
				size = bytes_.size();
			}
            assert(size <= bytes_.size());

			std::size_t bytesSent = 0;
			do
			{
				bytesSent += co_await socket_.send(
					bytes_.data() + bytesSent, size - bytesSent, ct_);
			} while (bytesSent < size);
			co_return bytesSent;
		}

		task<size_t> receive(size_t size = std::numeric_limits<size_t>::max()) requires(direction == message_direction::incoming)
		{
            if (size == std::numeric_limits<size_t>::max()) {
                size = bytes_.size();
            }
			assert(size <= bytes_.size());

			std::size_t totalBytesReceived = 0;
			std::size_t bytesReceived = 0;
			do
			{
				bytesReceived = co_await socket_.recv(
					bytes_.data() + totalBytesReceived, size - totalBytesReceived, ct_);
				totalBytesReceived += bytesReceived;
			} while (bytesReceived > 0 && totalBytesReceived < size);
			co_return totalBytesReceived;
		}

	private:
		SocketT& socket_;
		cancellation_token ct_;
		bytes_type bytes_;
	};

	template<is_connection ConnectionTypeT, typename... ArgsT>
	auto make_rx_message(
		ConnectionTypeT& connection, auto buffer, ArgsT&&... args)
	{
		return net::message<typename ConnectionTypeT::socket_type, message_direction::incoming>{
			connection.socket(), std::as_writable_bytes(buffer), connection.token()
		};
	}
	template<is_connection ConnectionTypeT, typename... ArgsT>
	auto make_tx_message(
		ConnectionTypeT& connection, auto buffer, ArgsT&&... args)
	{
		return net::message<typename ConnectionTypeT::socket_type, message_direction::outgoing>{
			connection.socket(), std::as_bytes(buffer), connection.token()
		};
	}

}  // namespace cppcoro::net
