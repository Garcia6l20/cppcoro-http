#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/connection.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

#include <cppcoro/http/router.hpp>

template<typename T>
struct route_result_visitor
{
	T value_;
	void operator()(T&& value) { value_ = std::forward<T>(value); }
};

SCENARIO("http::router is easy to use", "[cppcoro-http][router]")
{
	io_service ios{ 128 };
	cancellation_source cancel{};
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

	router::router router{
		std::make_tuple(),
		http::route::get<R"(/hello/(\w+))">(
			[](const std::string& who,
			   router::context<http::server_connection<net::socket>> con) -> task<> {
				co_await net::make_tx_message(
					*con, http::status::HTTP_STATUS_OK, fmt::format("Hello {} !", who));
			}),
		http::route::get<R"(/add/(\d+)/(\d+))">(
			[](int lhs,
			   int rhs,
			   router::context<http::server_connection<net::socket>> con) -> task<> {
				co_await net::make_tx_message(
					*con, http::status::HTTP_STATUS_OK, fmt::format("{}", lhs + rhs));
			}),
		router::on<R"(.*)">(
			[](router::context<http::server_connection<net::socket>> con) -> task<> {
				co_await net::make_tx_message(
					*con, http::status::HTTP_STATUS_NOT_FOUND, "route not found");
			}),
	};

	auto server = [&]() -> task<> {
		auto _ = on_scope_exit([&] { ios.stop(); });
		co_await net::serve(
			ios,
			endpoint,
			[&](http::server_connection<net::socket> con) -> task<> {
				net::byte_buffer<256> rx_buffer{};
				auto rx = co_await net::make_rx_message(con, std::span{ rx_buffer });
				co_await std::get<task<>>(
					router(rx.path, std::ref(rx), std::ref(con), http::method::get));
				spdlog::debug("done");
			},
			std::ref(cancel));
	};
	auto hello_client = [&](std::string_view who = "test") -> task<> {
		auto con = co_await net::connect<http::client_connection<net::socket>>(
			ios, endpoint, std::ref(cancel));
		net::byte_buffer<256> data{};
		co_await net::make_tx_message(con, http::method::get, fmt::format("/hello/{}", who));
		auto rx = co_await net::make_rx_message(con, std::span{ data });
		std::span body = co_await rx.receive();
		spdlog::info("hello_client received {} ({} bytes)", int(rx.status()), body.size_bytes());
		std::string_view sv_body{ reinterpret_cast<const char*>(body.data()), body.size_bytes() };
		spdlog::info("hello_client received: {}", sv_body);
		REQUIRE(sv_body == fmt::format("Hello {} !", who));
	};
    auto add_client = [&](int lhs = 40, int rhs = 2) -> task<> {
      auto con = co_await net::connect<http::client_connection<net::socket>>(
          ios, endpoint, std::ref(cancel));
      net::byte_buffer<256> data{};
      co_await net::make_tx_message(con, http::method::get, fmt::format("/add/{}/{}", lhs, rhs));
      auto rx = co_await net::make_rx_message(con, std::span{ data });
      std::span body = co_await rx.receive();
      spdlog::info("add_client received {} ({} bytes)", int(rx.status()), body.size_bytes());
	  int result = -1;
      std::string_view sv_body{ reinterpret_cast<const char*>(body.data()), body.size_bytes() };
	  std::from_chars(sv_body.data(), sv_body.data() + body.size_bytes(), result);
      spdlog::info("add_client received: {}", result);
      REQUIRE(result == lhs + rhs);
    };
	auto not_found_client = [&]() -> task<> {
      auto con = co_await net::connect<http::client_connection<net::socket>>(
          ios, endpoint, std::ref(cancel));
      net::byte_buffer<256> data{};
      co_await net::make_tx_message(con, http::method::get, fmt::format("/oops"));
      auto rx = co_await net::make_rx_message(con, std::span{ data });
      std::span body = co_await rx.receive();
      spdlog::info("not_found_client received {} ({} bytes)", int(rx.status()), body.size_bytes());
      std::string_view sv_body{ reinterpret_cast<const char*>(body.data()), body.size_bytes() };
      spdlog::info("not_found_client received: {}", sv_body);
	  REQUIRE(int(rx.status()) == 404);
	};
	auto run_clients = [&]() -> task<> {
		auto _ = on_scope_exit([&] { cancel.request_cancellation(); });
		co_await when_all(hello_client(), add_client(), not_found_client());
	};
	sync_wait(when_all(server(), run_clients(), [&]() -> task<> {
		ios.process_events();
		co_return;
	}()));

	//	REQUIRE(
	//		std::get<std::string>(router("/hello/world", http::method::get)) ==
	//		"Hello world from 42 !");
	//	REQUIRE(std::get<std::string>(router("/hello/world", http::method::post)) == "");
}
