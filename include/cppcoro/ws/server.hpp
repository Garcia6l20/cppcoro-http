/** @file cppcoro/ws/server.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/concepts.hpp>
#include <cppcoro/http/http_server.hpp>

#include <cppcoro/ws/connection.hpp>

#include <cppcoro/crypto/sha1.hpp>
#include <cppcoro/crypto/base64.hpp>

namespace cppcoro::ws
{
    template<http::is_config ConfigT>
    class server : public http::server<ConfigT>
    {
        using base = http::server<ConfigT>;

    public:
        using base::base;

        using connection_type = ws::connection<typename ConfigT::socket_provider>;

        using http_connection_type = typename base::connection_type;
        task<connection_type> listen()
        {
            http_connection_type conn = co_await base::listen();
            // start handshake
            http::string_request hs_request;
            auto init = [&](auto& parser) {
              hs_request = parser;
              return std::ref(hs_request);
            };
            auto req = co_await conn.next(init);
            for (auto& [k, v] : req->headers)
            {
                spdlog::debug("- {}: {}", k, v);
            }

            auto con_header = req->header("Connection");
            auto upgrade_header = req->header("Upgrade");
            bool upgrade = con_header and con_header->get() == "Upgrade";
            bool websocket = upgrade_header and upgrade_header->get() == "websocket";
            if (not upgrade or not websocket)
            {
                spdlog::warn("got a non-websocket connection");
                http::string_response resp{ http::status::HTTP_STATUS_BAD_REQUEST,
                                            "Expecting websocket connection" };
                co_await conn.send(resp);
                throw std::system_error{ std::make_error_code(std::errc::connection_reset) };
            }
            if (auto version_header = req->header("Sec-WebSocket-Version"); version_header)
            {
				auto &version = version_header->get();
                std::from_chars(
                    version.data(), version.data() + version.size(), ws_version_);
            }
            std::string accept = req->header("Sec-WebSocket-Key")->get();
            accept = crypto::base64::encode(
                crypto::sha1::hash(accept, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
            spdlog::info("accept-hash: {}", accept);
            http::string_response resp{ http::status::HTTP_STATUS_SWITCHING_PROTOCOLS,
                                        http::headers{
                                            { "Upgrade", "websocket" },
                                            { "Connection", "Upgrade" },
                                            { "Sec-WebSocket-Accept", std::move(accept) },
                                        } };
            co_await conn.send(resp);
            co_return conn.template upgrade<connection_type>();
        }

    private:
        int32_t ws_version_;
    };
}  // namespace cppcoro::http::ws
