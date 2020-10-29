/** @file cppcoro/ws/server.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/concepts.hpp>
#include <cppcoro/ws/header.hpp>
#include <cppcoro/task.hpp>

namespace cppcoro::http::ws
{
	template<is_config ConfigT>
	class connection
	{
	public:
		using socket_type = typename ConfigT::connection_socket_type;

		explicit connection(socket_type sock) noexcept
			: sock_{ std::move(sock) }
		{
		}

		task<size_t> send(std::string_view data)
		{
			std::array<std::byte, 1024> buffer{};
			size_t data_offset = 0;
			while (data_offset < data.size())
			{
				header h{
					.opcode = op_code::text_frame,
				};
				size_t payload_size = std::min(buffer.size(), data.size() - data_offset);
				auto [send_size, payload_len] = h.calc_payload_size(payload_size, buffer.size());
				if (data_offset + payload_len >= data.size())
				{
					h.fin = true;
				}
				h.serialize(std::span{ buffer });
				std::memcpy(&buffer[h.payload_offset], data.data() + data_offset, h.payload_len);
				data_offset += h.payload_len;
				co_await sock_.send(buffer.data(), send_size);
			}
			sock_.close_send();
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
				co_await sock_.recv(buffer.data(), buffer.size());
				h = header::parse(buffer);
				output.resize(h.payload_len);
				if (h.mask)
				{
					std::byte masking_key[4]{};
					std::memcpy(masking_key, &h.masking_key, 4);
					for (size_t ii = 0; ii < h.payload_len; ++ii)
					{
						output[output_offset++] =
							CharT(buffer[h.payload_offset + ii] xor masking_key[ii % 4]);
					}
				}
				else
				{
					std::memcpy(output.data(), &buffer[h.payload_offset], h.payload_len);
					output_offset += h.payload_len;
				}
			} while (not h.fin);
			sock_.close_recv();
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
			co_await sock_.send(buffer.data(), to_send);
			sock_.close_send();
		}

	private:
		socket_type sock_;
	};
}  // namespace cppcoro::http::ws
