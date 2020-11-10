#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/router/router.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

SCENARIO("router::router is easy to use", "[cppcoro-http][router]")
{
	cppcoro::io_service ios;
	static const auto test_endpoint = net::ip_endpoint::from_string("127.0.0.1:4242");

	auto router = cppcoro::router::router{
		router::on<R"(/hello/(\w+))">(
			[](const std::string& who) { return fmt::format("Hello {} !", who); }),
		router::on<R"(/add/(\d+)/(\d+))">([](int lhs, int rhs) { return lhs + rhs; }),
	};
	REQUIRE(std::get<std::string>(router("/hello/world")) == "Hello world !");
	REQUIRE(std::get<int>(router("/add/21/21")) == 42);
}

SCENARIO("router::router can capture a context", "[cppcoro-http][router]")
{
	cppcoro::io_service ios;
	static const auto test_endpoint = net::ip_endpoint::from_string("127.0.0.1:4242");
	struct my_context_t
	{
		int id = 42;
	} context;
	auto router = cppcoro::router::router{
		std::make_tuple(std::ref(context)),
		router::on<R"(/hello/(\w+))">(
			[](const std::string& who) { return fmt::format("Hello {} !", who); }),
		router::on<R"(/)">([](cppcoro::router::context<my_context_t> context,
							  cppcoro::router::context<int> id) { return *id + context->id; })
	};
	REQUIRE(std::get<int>(router("/", 0)) == 42);
	REQUIRE(std::get<int>(router("/", 42)) == 84);
	REQUIRE(std::get<std::string>(router("/hello/world", 0)) == "Hello world !");
}
