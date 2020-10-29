#include <catch2/catch.hpp>

#include <cppcoro/ws/server.hpp>

#include <cppcoro/http/session.hpp>

#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>

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
			auto stop_io_service = on_scope_exit([&svc] { svc.stop(); });
			try
			{
				auto con = co_await ws_server.listen();
				auto msg = co_await con.recv();
				spdlog::info("received {}", std::string_view{ msg.begin(), msg.end() });
				auto sent = co_await con.send("Hello !!!");
				spdlog::info("sent {} bytes", sent);
			}
			catch (std::exception& error)
			{
				FAIL(fmt::format("error: {}", error.what()));
			}
			co_return;
		}(),
		[&svc]() -> task<> {
			svc.process_events();
			co_return;
		}()));
}
