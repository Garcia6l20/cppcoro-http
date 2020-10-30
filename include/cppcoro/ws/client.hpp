/** @file cppcoro/ws/client.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/client.hpp>

#include <cppcoro/ws/connection.hpp>

#include <cppcoro/crypto/base64.hpp>

namespace cppcoro::ws
{
	template<net::is_connection_socket_provider SocketProviderT>
	class client : private http::client<SocketProviderT>
	{
		using base = http::client<SocketProviderT>;
		static constexpr uint32_t ws_version_ = 13;

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

	public:
		using http_connection_type = typename base::connection_type;

		using base::base;

		task<ws::connection<SocketProviderT>> connect(net::ip_endpoint const& endpoint)
		{
			http_connection_type conn = co_await base::connect(endpoint);
			std::string hash = crypto::base64::encode(random_string(20));

            // GCC 11 bug:
			//  cannot inline headers initialization in co_await statement
            http::headers headers{
                { "Connection", "Upgrade" },
                { "Upgrade", "websocket" },
                { "Sec-WebSocket-Key", hash },
                { "Sec-WebSocket-Version", std::to_string(ws_version_) },
            };
			auto resp = co_await conn.get(
				"/",
				"",
				std::move(headers));

			if (not resp)
			{
				throw std::system_error{ std::make_error_code(std::errc::connection_refused) };
			}
			if (resp->status != http::status::HTTP_STATUS_SWITCHING_PROTOCOLS)
			{
				throw std::runtime_error("server won't switch protocol");
			}
			if (auto header = resp->header("Sec-WebSocket-Accept"); header)
			{
				std::string expected_accept_hash = crypto::base64::encode(
					crypto::sha1::hash(hash, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
				if (header->get() != expected_accept_hash)
				{
					throw std::runtime_error("invalid accept hash");
				}
			}
			else
			{
				throw std::runtime_error("missing accept header");
			}
			co_return conn.template upgrade<ws::connection<SocketProviderT>>();;
		}
	};
}  // namespace cppcoro::ws
