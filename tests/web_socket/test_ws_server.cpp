#include <catch2/catch.hpp>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/crypto/base64.hpp>
#include <cppcoro/crypto/sha1.hpp>

namespace cppcoro::http::ws
{
	template<is_config ConfigT>
	class server : public http::server<ConfigT>
	{
		using base = http::server<ConfigT>;

	public:
		using base::base;

		using connection_type = typename base::connection_type;
		task<connection_type> listen()
		{
			connection_type conn = co_await base::listen();
			// start handshake
            http::string_request hs_request;
			auto init = [&](auto &parser) {
              hs_request = parser;
              return std::ref(hs_request);
			};
			auto req = co_await conn.next(init);
            for (auto &[k, v] : req->headers) {
                spdlog::debug("- {}: {}", k, v);
            }

			auto con_header = req->header("Connection");
            bool upgrade = con_header and con_header->get() == "Upgrade";
            auto upgrade_header = req->header("Upgrade");
            bool websocket = upgrade_header and upgrade_header->get() == "websocket";
			if (not upgrade or not websocket) {
				spdlog::warn("got a non-websocket connection");
				http::string_response resp{
					http::status::HTTP_STATUS_BAD_REQUEST,
					"Expecting websocket connection"
				};
				co_await conn.send(resp);
				throw std::system_error{std::make_error_code(std::errc::connection_reset)};
			}
			std::string accept = req->header("Sec-WebSocket-Key")->get();
			accept = crypto::base64::encode(crypto::sha1::hash(accept, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
			spdlog::info("accept-hash: {}", accept);
            http::string_response resp{
                http::status::HTTP_STATUS_SWITCHING_PROTOCOLS, http::headers {
                    {"Upgrade", "websocket"},
                    {"Connection", "Upgrade"},
                    {"Sec-WebSocket-Accept", std::move(accept)},
               }
            };
            co_await conn.send(resp);
			co_return std::move(conn);
		}

	private:
		int32_t ws_version_;
	};
}  // namespace cppcoro::http::ws

using namespace cppcoro;

TEST_CASE("WebSocket server should work", "[cppcoro-http][ws]")
{
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);

	io_service svc{ 128 };
	http::ws::server<http::default_session_config<>> ws_server{
		svc, *net::ip_endpoint::from_string("127.0.0.1:4242")
	};
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto stop_io_service = on_scope_exit([&svc] {
				svc.stop();
			});
			try
			{
				auto con = co_await ws_server.listen();
//				http::string_response response{ http::status::HTTP_STATUS_OK, "Hello !!" };
//				co_await con.send(response);
			}
			catch (std::exception &error)
			{
				spdlog::error("error: {}", error.what());
			}
			co_return;
		}(),
		[&svc]() -> task<> {
			svc.process_events();
			co_return;
		}()));
}
