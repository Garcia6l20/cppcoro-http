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

		static header make_header(op_code op = op_code::text_frame) noexcept
		{
			return header{
				.opcode = op,
			};
		}

		task<size_t> send(header&& _header)
		{
			_header.update_payload_offset();
			_header.serialize(header_bytes_);
			return base::send(header_bytes_);
		}

		task<> begin_message(size_t size) {
            header_ = make_header(op_code::binary_frame);
			co_return;
		}

		using base::send;

	private:
		std::array<std::byte, header::max_header_size> header_storage_{};
		net::writeable_bytes header_bytes_{ header_storage_ };
		header header_{make_header()};
	};

	// rx specialization
	template<net::is_socket SocketT, net::connection_mode mode>
	struct message<SocketT, net::message_direction::incoming, mode>
		: protected net::message<SocketT, net::message_direction::incoming>
	{
		using base = net::message<SocketT, net::message_direction::incoming>;
		using base::base;

		task<header> receive_header()
		{
			body_ = {};
			size_t bytes_received = 0;
			while ((bytes_received = co_await base::receive()) == 0)
			{
			}
			header_ = header::parse(std::span{ this->bytes_.data(), bytes_received });
			if (header_.payload_offset > bytes_received)
			{
				body_ = { &this->bytes_[header_.payload_offset],
						  std::min(bytes_received, header_.payload_length) };
			}
			co_return header_;
		}

		task<> begin_message() { header_ = co_await receive_header(); }

		task<net::readable_bytes> receive()
		{
			if (body_.size())
			{
				co_return std::exchange(body_, net::readable_bytes{});
			}
			else
			{
				auto bytes_received = co_await base::receive();
				co_return std::exchange(body_, net::readable_bytes{});
			}
		}

	private:
		header header_{};
		net::readable_bytes body_{};
		size_t bytes_to_read = 0;
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
			http::headers headers{
				{ "Connection", "Upgrade" },
				{ "Upgrade", "websocket" },
				{ "Sec-WebSocket-Key", hash },
				{ "Sec-WebSocket-Version", std::to_string(ws_version_) },
			};
			net::byte_buffer<128> buffer{};

			auto tx = co_await net::make_tx_message(con, std::span{ buffer }, http::method::get, "/");
//			auto h = tx.make_header(http::method::post, std::move(headers));
//			h.path = "/";
//			co_await tx.send(std::move(h));
			auto rx = co_await net::make_rx_message(con, std::span{ buffer });
			auto rx_header = co_await rx.receive_header();
			auto accept = rx_header["Sec-WebSocket-Accept"];

			co_return con.template upgrade<ws::connection<SocketT, connection_mode>>();
		}
		static task<ws::connection<SocketT, connection_mode>>
		from_http_connection(http::connection<SocketT, connection_mode>&& con) requires(
			connection_mode == net::connection_mode::server)
		{
			net::byte_buffer<128> buffer{};
			{
				auto rx = co_await net::make_rx_message(con, std::span{ buffer });
				auto rx_header = co_await rx.receive_header();

				auto const& con_header = rx_header["Connection"];
				auto const& upgrade_header = rx_header["Upgrade"];
				auto const& key_header = rx_header["Sec-WebSocket-Key"];
				auto const& version_header = rx_header["Sec-WebSocket-Version"];
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
					auto tx = co_await net::make_tx_message(con, std::span{ buffer });
					std::string accept = rx_header["Sec-WebSocket-Key"];
					accept = crypto::base64::encode(
						crypto::sha1::hash(accept, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
					spdlog::info("accept-hash: {}", accept);
					auto tx_header = tx.make_header(
						http::status::HTTP_STATUS_SWITCHING_PROTOCOLS,
						http::headers{
							{ "Upgrade", "websocket" },
							{ "Connection", "Upgrade" },
							{ "Sec-WebSocket-Accept", std::move(accept) },
						});
					co_await tx.send(std::move(tx_header));
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
