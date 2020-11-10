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

SCENARIO("http::router is easy to use", "[cppcoro-http][router]")
{
	io_service ios{ 128 };
	cancellation_source cancel{};
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

	struct session_t
	{
		int id = 42;
	} session;
	router::router router{ std::make_tuple(std::ref(session)),
						   http::route::get<R"(/hello/(\w+))">(
							   [](const std::string& who,
								  router::context<session_t> session,
								  router::context<http::server_connection<net::socket>> con) {

								   return fmt::format("Hello {} from {} !", who, session->id);
							   }),
						   http::route::get<R"(/add/(\d+)/(\d+))">(
							   [](int lhs, int rhs) { return lhs + rhs; }) };

	auto server = [&]() -> task<> {
		co_await net::serve(ios, endpoint, [&](http::server_connection<net::socket> con) -> task<> {
			net::byte_buffer<256> rx_buffer{};
			net::byte_buffer<256> tx_buffer{};
			while (true)
			{
				auto rx = co_await net::make_rx_message(con, std::span{ rx_buffer });
//				auto method = rx.method;
				router(rx.path, std::move(rx), std::ref(con), http::method::get);
			}
		}, std::ref(cancel));
	};

//	REQUIRE(
//		std::get<std::string>(router("/hello/world", http::method::get)) ==
//		"Hello world from 42 !");
//	REQUIRE(std::get<std::string>(router("/hello/world", http::method::post)) == "");
}
