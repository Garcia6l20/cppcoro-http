#pragma once

#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/net/concepts.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/detail/always_false.hpp>

#include <span>

namespace cppcoro::net
{
	template<size_t size>
	using byte_buffer = std::array<std::byte, size>;

	using readable_bytes = std::span<const std::byte, std::dynamic_extent>;
	using writeable_bytes = std::span<std::byte, std::dynamic_extent>;

	enum class message_direction
	{
		incoming,
		outgoing
	};
	template<is_socket SocketT, message_direction direction>
	struct message
	{
		using bytes_type = std::conditional_t<
			direction == message_direction::incoming,
			writeable_bytes,
			readable_bytes>;

		message(SocketT& socket, bytes_type bytes, cancellation_token ct) noexcept
			: socket_{ socket }
			, ct_{ std::move(ct) }
			, bytes_{ bytes }
		{
		}

		message(message&&) = default;
		message(const message&) = delete;

		task<size_t> send(size_t size = std::numeric_limits<size_t>::max()) requires(
			direction == message_direction::outgoing)
		{
			if (size == std::numeric_limits<size_t>::max())
			{
				size = bytes_.size();
			}
			assert(size <= bytes_.size());
			return send(std::span{ bytes_.data(), size });
		}

		task<size_t> receive(size_t size = std::numeric_limits<size_t>::max()) requires(
			direction == message_direction::incoming)
		{
			if (size == std::numeric_limits<size_t>::max())
			{
				size = bytes_.size();
			}
			assert(size <= bytes_.size());
			return receive(std::span{ bytes_.data(), size });
		}

		task<size_t> send(readable_bytes bytes)
		{
			std::size_t bytesSent = 0;
			do
			{
				bytesSent += co_await socket_.send(
					bytes.data() + bytesSent, bytes.size_bytes() - bytesSent, ct_);
			} while (bytesSent < bytes.size_bytes());
			co_return bytesSent;
		}

		task<size_t> receive_all(writeable_bytes bytes)
		{
			std::size_t totalBytesReceived = 0;
			std::size_t bytesReceived;
			do
			{
				bytesReceived = co_await socket_.recv(
					bytes.data() + totalBytesReceived,
					bytes.size_bytes() - totalBytesReceived,
					ct_);
				totalBytesReceived += bytesReceived;
			} while (bytesReceived > 0 && totalBytesReceived < bytes.size_bytes());
			co_return totalBytesReceived;
		}

		task<size_t> receive(writeable_bytes bytes)
		{
			co_return co_await socket_.recv(bytes.data(), bytes.size_bytes(), ct_);
		}

		SocketT& socket_;
		cancellation_token ct_;
		bytes_type bytes_;
	};

	template<typename ConnectionTypeT>
	using connection_tx_message =
		typename ConnectionTypeT::template message_type<message_direction::outgoing>;

	template<typename ConnectionTypeT>
	using connection_rx_message =
		typename ConnectionTypeT::template message_type<message_direction::incoming>;

	// clang-format off
	template <typename T, typename...Args>
	concept has_begin_message = requires (T obj, Args&&...args){
//        requires std::invocable<T, Args...>;
        { obj.begin_message(std::forward<Args>(args)...)}->awaitable;
    };
	// clang-format on

	template<
		is_connection ConnectionTypeT,
		typename T,
		size_t extent = std::dynamic_extent,
		typename MessageT =
			typename ConnectionTypeT::template message_type<message_direction::incoming>,
		typename... ArgsT>
	task<MessageT>
	make_rx_message(ConnectionTypeT& connection, std::span<T, extent> buffer, ArgsT&&... args)
	{
		MessageT msg{ connection.socket(), std::as_writable_bytes(buffer), connection.token() };
		if constexpr (has_begin_message<MessageT, ArgsT...>)
		{
			co_await msg.begin_message(std::forward<ArgsT>(args)...);
		}
		else if constexpr (sizeof...(ArgsT))
		{
			static_assert(cppcoro::always_false_v<MessageT>, "Cannot bind to requested message initializer");
		}
		co_return msg;
	}
	template<
		is_connection ConnectionTypeT,
		typename T,
		size_t extent = std::dynamic_extent,
		typename MessageT =
			typename ConnectionTypeT::template message_type<message_direction::outgoing>,
		typename... ArgsT>
	task<MessageT>
	make_tx_message(ConnectionTypeT& connection, std::span<T, extent> buffer, ArgsT&&... args)
	{
		auto msg =
			MessageT{ connection.socket(), std::as_writable_bytes(buffer), connection.token() };
		if constexpr (has_begin_message<MessageT, ArgsT...>)
		{
			co_await msg.begin_message(std::forward<ArgsT>(args)...);
		}
		else if constexpr (sizeof...(ArgsT))
		{
			static_assert(always_false_v<MessageT>, "Cannot bind to requested message initializer");
		}
		co_return msg;
	}

}  // namespace cppcoro::net
