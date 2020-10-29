#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_client.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/ssl/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

using config = http::default_session_config<>;

template<typename>
struct hello_controller;

template<typename ConfigT = config>
using hello_controller_def =
	http::route_controller<R"(/hello/(\w+))", ConfigT, http::string_request, hello_controller>;

template<typename ConfigT = config>
struct hello_controller : hello_controller_def<ConfigT>
{
	using hello_controller_def<ConfigT>::hello_controller_def;

	auto on_post(std::string_view who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format("post: {}", who) };
	}
	auto on_get(const std::string& who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format("get: {}", who) };
	}
};

SCENARIO("route controller are easy to use", "[cppcoro-http][router]")
{
	cppcoro::io_service ios;
	static const auto test_endpoint = net::ip_endpoint::from_string("127.0.0.1:4242");

	auto test = [&](auto client, auto server) {
		(void)sync_wait(when_all(
			[&]() -> task<> {
				auto _ = on_scope_exit([&] { ios.stop(); });
				try
				{
					co_await server.serve();
				}
				catch (operation_cancelled&)
				{
				}
			}(),
			[&]() -> task<> {
				auto _ = on_scope_exit([&] { server.stop(); });
				using namespace std::chrono_literals;
				auto conn = co_await client.connect(*test_endpoint);
				auto resp = co_await conn.post("/hello/world");
				REQUIRE(co_await resp->read_body() == "post: world");
				resp = co_await conn.get("/hello/world");
				REQUIRE(co_await resp->read_body() == "get: world");
			}(),
			[&]() -> task<> {
				ios.process_events();
				co_return;
			}()));
	};

	GIVEN("A simple route controller")
	{
		http::controller_server<config, hello_controller> server{ ios, *test_endpoint };
		auto client = http::client<>{ ios };
		test(std::move(client), std::move(server));
	}
}
