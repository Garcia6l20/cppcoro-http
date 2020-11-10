/** @file cppcoro/ws/connection.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/crypto/base64.hpp>
#include <cppcoro/crypto/sha1.hpp>
#include <cppcoro/net/concepts.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/ws/header.hpp>

#include <cppcoro/http/connection.hpp>
#include <cppcoro/net/message.hpp>
#include <vector>

namespace cppcoro::ws
{
	template<net::is_socket SocketT, net::message_direction direction, net::connection_mode mode>
	struct message
	{
	};

	// tx specialization
	template<net::is_socket SocketT, net::connection_mode mode>
	struct message<SocketT, net::message_direction::outgoing, mode>
		: protected net::message<SocketT, net::message_direction::outgoing>
	{
		using base = net::message<SocketT, net::message_direction::outgoing>;
		using base::base;
		using typename base::bytes_type;

		task<> begin_message(size_t size)
		{
			co_await send(header{
				.fin = true,
				.opcode = op_code::binary_frame,
				.payload_length = size,
			});
			co_return;
		}

		using base::send;

	private:
		task<size_t> send(header&& _header)
		{
			_header.update_payload_offset();
			_header.serialize(header_bytes_);
			return base::send(std::span{ header_bytes_.data(), _header.payload_offset });
		}
		std::array<std::byte, header::max_header_size> header_storage_{};
		net::writeable_bytes header_bytes_{ header_storage_ };
	};

	// rx specialization
	template<net::is_socket SocketT, net::connection_mode mode>
	struct message<SocketT, net::message_direction::incoming, mode>
		: protected net::message<SocketT, net::message_direction::incoming>
	{
		using base = net::message<SocketT, net::message_direction::incoming>;
		using base::base;

		task<> begin_message() { co_await receive_header(); }

		task<net::readable_bytes> receive()
		{
//            spdlog::info("bytes_remaining : {}/{}", bytes_remaining_, header_.payload_length);
			if (body_.size())
			{
				unmask_body();
				co_return std::exchange(body_, net::writeable_bytes{});
			}
			else if (bytes_remaining_ == 0)
			{
				co_return net::readable_bytes{};
			}
			else if (header_.fin)
			{
				auto bytes_received = co_await base::receive();
				body_ = { this->bytes_.data(), bytes_received };
				bytes_remaining_ -= bytes_received;
				unmask_body();
				co_return std::exchange(body_, net::writeable_bytes{});
			}
			else
			{
				throw std::runtime_error("not implemented");
			}
		}

		std::optional<size_t> payload_length{};

	private:
		size_t mask_offset_{ 0 };
		void unmask_body()
		{
			if (header_.mask)
			{
				std::byte masking_key[4]{};
				std::memcpy(masking_key, &header_.masking_key, 4);
				for (size_t ii = 0; ii < body_.size(); ++ii, ++mask_offset_)
				{
					body_[ii] = body_[ii] xor masking_key[mask_offset_ % 4];
				}
			}
		}

		task<> receive_header()
		{
			body_ = {};
			size_t bytes_received = co_await base::receive();
			header_ = header::parse(std::span{ this->bytes_.data(), bytes_received });
			if (header_.payload_offset < bytes_received)
			{
				body_ = { &this->bytes_[header_.payload_offset],
						  std::min(
							  bytes_received - header_.payload_offset, header_.payload_length) };
			}
			bytes_remaining_ = header_.payload_length - (bytes_received - header_.payload_offset);
			payload_length = header_.payload_length;
		}
		size_t bytes_remaining_{ 0 };
		header header_{};
		net::writeable_bytes body_{};
	};

	template<net::is_socket SocketT, net::connection_mode connection_mode>
	class connection : public tcp::connection<SocketT, connection_mode>
	{
	public:
		using socket_type = SocketT;
		using base = tcp::connection<SocketT, connection_mode>;

		template<net::message_direction dir = net::message_direction::incoming>
		using message_type = ws::message<SocketT, dir, connection_mode>;

		static constexpr bool is_http_upgrade = true;

		connection() noexcept = default;

		connection(connection&& other) noexcept = default;

		connection(const connection& other) = delete;

		connection& operator=(connection&& other) = delete;

		connection& operator=(const connection& other) = delete;

		connection(socket_type socket, cancellation_token ct) noexcept
			: base{ std::move(socket), std::move(ct) }
		{
		}

		static std::string random_string(size_t len)
		{
			constexpr char charset[] = "0123456789"
									   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
									   "abcdefghijklmnopqrstuvwxyz";

			static std::mt19937 rg{ std::random_device{}() };
			static std::uniform_int_distribution<std::string::size_type> dist(
				0, sizeof(charset) - 2);

			std::string str{};
			str.resize(len);
			std::generate_n(str.begin(), len, [] { return dist(rg); });
			return str;
		}

		static task<ws::connection<SocketT, connection_mode>>
		from_http_connection(http::connection<SocketT, connection_mode>&& con) requires(
			connection_mode == net::connection_mode::client)
		{
			std::string hash = crypto::base64::encode(random_string(20));

			// GCC 11 bug:
			//  cannot inline headers initialization in co_await statement
			http::headers hdrs{
				{ "Connection", "Upgrade" },
				{ "Upgrade", "websocket" },
				{ "Sec-WebSocket-Key", hash },
				{ "Sec-WebSocket-Version", std::to_string(ws_version_) },
			};
			net::byte_buffer<128> buffer{};

			auto tx = co_await net::make_tx_message(
				con,
				std::span{ buffer },
				http::method::get,
				std::string_view{ "/" },
				std::move(hdrs));
            co_await tx.send(std::as_bytes(std::span{ "\r\n" }));
			//			auto h = tx.make_header(http::method::post, std::move(headers));
			//			h.path = "/";
			//			co_await tx.send(std::move(h));
			auto rx = co_await net::make_rx_message(con, std::span{ buffer });
			auto accept = rx["Sec-WebSocket-Accept"];

			co_return con.template upgrade<ws::connection<SocketT, connection_mode>>();
		}
		static task<ws::connection<SocketT, connection_mode>>
		from_http_connection(http::connection<SocketT, connection_mode>&& con) requires(
			connection_mode == net::connection_mode::server)
		{
			net::byte_buffer<128> buffer{};
			{
				auto rx = co_await net::make_rx_message(con, std::span{ buffer });

				auto const& con_header = rx["Connection"];
				auto const& upgrade_header = rx["Upgrade"];
				auto const& key_header = rx["Sec-WebSocket-Key"];
				auto const& version_header = rx["Sec-WebSocket-Version"];
				if (con_header.empty() or upgrade_header.empty())
				{
					spdlog::warn("got a non-websocket connection");
					//                http::string_response resp{
					//                http::status::HTTP_STATUS_BAD_REQUEST,
					//                                            "Expecting websocket connection"
					//                                            };
					//                co_await conn.send(resp);
					throw std::system_error{ std::make_error_code(std::errc::connection_reset) };
				}
				{
					std::string accept = rx["Sec-WebSocket-Key"];
					accept = crypto::base64::encode(
						crypto::sha1::hash(accept, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
					spdlog::info("accept-hash: {}", accept);
					http::headers hdrs{
						{ "Upgrade", "websocket" },
						{ "Connection", "Upgrade" },
						{ "Sec-WebSocket-Accept", std::move(accept) },
					};
					auto tx = co_await net::make_tx_message(
						con,
						std::span{ buffer },
						http::status::HTTP_STATUS_SWITCHING_PROTOCOLS,
						std::move(hdrs));
                    co_await tx.send(std::as_bytes(std::span{ "\r\n" }));
				}
			}
			co_return con.template upgrade<ws::connection<SocketT, connection_mode>>();
		}
		static constexpr uint32_t ws_version_ = 13;

		template<typename CharT, size_t extent = std::dynamic_extent>
		task<size_t> send(std::span<CharT, extent> data)
		{
			std::array<std::byte, 1024> buffer{};
			size_t data_offset = 0;
			while (data_offset < data.size_bytes())
			{
				header h{
					.opcode = op_code::text_frame,
				};
				size_t payload_size = std::min(buffer.size(), data.size_bytes() - data_offset);
				auto [send_size, payload_len] = h.calc_payload_size(payload_size, buffer.size());
				if (data_offset + payload_len >= data.size_bytes())
				{
					h.fin = true;
				}
				h.serialize(std::span{ buffer });
				std::memcpy(&buffer[h.payload_offset], data.data() + data_offset, h.payload_length);
				data_offset += h.payload_length;
				co_await this->sock_.send(buffer.data(), send_size);
			}
			this->sock_.close_send();
			co_return data_offset;
		}

		template<typename CharT = char>
		task<std::vector<CharT>> recv()
		{
			std::array<std::byte, 1024> buffer{};
			std::vector<CharT> output{};
			size_t output_offset = 0;
			header h{};
			do
			{
				co_await this->sock_.recv(buffer.data(), buffer.size());
				h = header::parse(buffer);
				output.resize(h.payload_length);
				if (h.mask)
				{
					std::byte masking_key[4]{};
					std::memcpy(masking_key, &h.masking_key, 4);
					for (size_t ii = 0; ii < h.payload_length; ++ii)
					{
						output[output_offset++] =
							CharT(buffer[h.payload_offset + ii] xor masking_key[ii % 4]);
					}
				}
				else
				{
					std::memcpy(output.data(), &buffer[h.payload_offset], h.payload_length);
					output_offset += h.payload_length;
				}
			} while (not h.fin);
			co_return output;
		}

		task<> close()
		{
			std::array<std::byte, 16> buffer{};
			header h{
				.fin = true,
				.opcode = op_code::connection_close,
			};
			auto [to_send, _] = h.calc_payload_size(0, buffer.size());
			h.serialize(std::span{ buffer });
			co_await this->sock_.send(buffer.data(), to_send);
			this->sock_.close_send();
		}
	};

	template<net::is_socket SocketT>
	using server_connection = connection<SocketT, net::connection_mode::server>;

	template<net::is_socket SocketT>
	using client_connection = connection<SocketT, net::connection_mode::client>;

	static_assert(net::is_http_upgrade_connection<
				  client_connection<net::socket>,
				  http::client_connection<net::socket>>);

}  // namespace cppcoro::ws
