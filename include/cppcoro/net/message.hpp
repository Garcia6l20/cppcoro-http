#pragma once

#include <cppcoro/net/concepts.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/cancellation_token.hpp>

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
	template<is_socket SocketT, message_direction direction>
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
            return _send(std::span{bytes_.data(), size});
		}

		task<size_t> receive(size_t size = std::numeric_limits<size_t>::max()) requires(direction == message_direction::incoming)
		{
            if (size == std::numeric_limits<size_t>::max()) {
                size = bytes_.size();
            }
			assert(size <= bytes_.size());
			return _receive(std::span{bytes_.data(), size});
		}

	protected:
		task<size_t> _send(readable_bytes bytes) {
            std::size_t bytesSent = 0;
            do
            {
                bytesSent += co_await socket_.send(
                    bytes.data() + bytesSent, bytes.size_bytes() - bytesSent, ct_);
            } while (bytesSent < bytes.size_bytes());
            co_return bytesSent;
		}

        task<size_t> _receive(writeable_bytes bytes) {
            std::size_t totalBytesReceived = 0;
            std::size_t bytesReceived;
            do
            {
                bytesReceived = co_await socket_.recv(
                    bytes.data() + totalBytesReceived, bytes.size_bytes() - totalBytesReceived, ct_);
                totalBytesReceived += bytesReceived;
                spdlog::debug("client bytesReceived: {}", bytesReceived);
            } while (bytesReceived > 0 && totalBytesReceived < bytes.size_bytes());
            co_return totalBytesReceived;
		}

		SocketT& socket_;
		cancellation_token ct_;
		bytes_type bytes_;
	};

	template<is_connection ConnectionTypeT, typename... ArgsT>
	auto make_rx_message(
		ConnectionTypeT& connection, auto buffer, ArgsT&&... args)
	{
		return typename ConnectionTypeT::template message_type<message_direction::incoming>{
			connection.socket(), std::as_writable_bytes(buffer), connection.token()
		};
	}
	template<is_connection ConnectionTypeT, typename... ArgsT>
	auto make_tx_message(
		ConnectionTypeT& connection, auto buffer, ArgsT&&... args)
	{
		return typename ConnectionTypeT::template message_type<message_direction::outgoing>{
			connection.socket(), std::as_bytes(buffer), connection.token()
		};
	}

}  // namespace cppcoro::net
