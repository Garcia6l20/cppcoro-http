#include <catch2/catch.hpp>

#include <cppcoro/ws/server.hpp>
#include <cppcoro/ws/client.hpp>

#include <cppcoro/http/session.hpp>

#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/single_consumer_event.hpp>

using namespace cppcoro;

TEST_CASE("WebSocket server should work", "[cppcoro-http][ws]")
{
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);

	auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4222");

	io_service svc{ 128 };
	ws::server<http::default_session_config<>> ws_server{
		svc, endpoint
	};
	single_consumer_event client_terminated;
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto stop_io_service = on_scope_exit([&svc] {
				svc.stop();
			});
			try
			{
				auto con = co_await ws_server.listen();
				auto msg = co_await con.recv();
				spdlog::info("received {}", std::string_view{ msg.begin(), msg.end() });
				auto sent = co_await con.send(std::span{ msg });
				spdlog::info("sent {} bytes", sent);
			}
			catch (std::exception& error)
			{
				FAIL(fmt::format("error: {}", error.what()));
			}
            co_await client_terminated;
			co_return;
		}(),
		[&]() -> task<> {
            auto notify_stopped = on_scope_exit([&] {
                client_terminated.set();
            });
			ws::client<tcp::ipv4_socket_provider> client{svc};
			auto con = co_await client.connect(endpoint);
			std::string_view hello{"Hello world !"};
			co_await con.send(std::span{ hello });
			auto msg = co_await con.recv();
			REQUIRE(std::string_view{ msg.data(), msg.data() + msg.size() } == hello);
		}(),
		[&svc]() -> task<> {
			svc.process_events();
			co_return;
		}()));
}
