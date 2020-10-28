#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_client.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/net/ssl/socket.hpp>

using namespace cppcoro;

struct session
{
	int id = std::rand();
};

using hello_controller_def = http::
	route_controller<R"(/hello/(\w+))", session, http::string_request, struct hello_controller>;

struct hello_controller : hello_controller_def
{
	using hello_controller_def::hello_controller_def;

	auto on_post(std::string_view who) -> task<http::string_response>
	{
		fmt::print("post on {}\n", session().id);
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format("post: {}", who) };
	}
	auto on_get(const std::string& who) -> task<http::string_response>
	{
		fmt::print("get on {}\n", session().id);
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format("get: {}", who) };
	}
};

struct ipv4_ssl_client_provider
{
    auto create_listening_sock(io_service& ios) const { return net::socket::create_tcpv4(ios); }
    net::ssl::socket create_connection_sock(io_service& ios) const {
        auto sock = net::ssl::socket::create_client(ios);
        sock.set_peer_verify_mode(net::ssl::peer_verify_mode::required);
        sock.set_verify_flags(net::ssl::verify_flags::allow_untrusted);
        return sock;
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
              co_await server.serve();
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
		http::controller_server<tcp::ipv4_socket_provider, session, hello_controller>
			server{ ios, *test_endpoint };
		auto client = http::client<>{ ios };
		test(std::move(client), std::move(server));
	}
}
