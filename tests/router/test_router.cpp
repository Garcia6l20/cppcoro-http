#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/client.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/router/router.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

using config = http::default_session_config<>;

#include <cppcoro/ssl/socket.hpp>

namespace cppcoro::net::hooks::socket
{
	task<> post_accept(net::ssl::socket& sock) { return sock.encrypt(); }
}  // namespace cppcoro::net::hooks::socket

namespace cppcoro::net
{
	template<typename DerivedT>
	struct crtp
	{
		constexpr decltype(auto) invoke(auto&& fn, auto&&... args)
		{
			return std::invoke(
				std::forward<decltype(fn)>(fn),
				static_cast<DerivedT*>(this),
				std::forward<decltype(args)>(args)...);
		}
	};

	namespace detail
	{
		template<typename T>
		struct always_false
		{
			bool value = false;
		};
		template<typename T>
		constexpr auto always_false_v = always_false<T>::value;
	}  // namespace detail

	template<typename DerivedT>
	struct transport : protected crtp<DerivedT>
	{
	};

	template<>
	struct transport<ssl::socket>
	{
	};

	template<is_socket SocketT, template<class...> typename ConnectionT>
	struct server
	{
		server() noexcept = default;
		task<> serve(auto connection_handler, SocketT&& sock)
		{
			sock.listen();
			if constexpr (requires {
							  {
								  hooks::socket::post_accept(sock)
							  }
							  ->awaitable;
						  })
			{
				co_await hooks::socket::post_accept(sock);
			}
			while (true)
			{
				make_socket(sock);
				co_await sock.accept();
			}
		}
	};
}  // namespace cppcoro::net
#include <cppcoro/net/any_socket.hpp>

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
	struct context_t
	{
		int id = 42;
	} context;
	auto router = cppcoro::router::router{
		std::make_tuple(std::ref(context)),
		router::on<R"(/hello/(\w+))">(
			[](const std::string& who) { return fmt::format("Hello {} !", who); }),
		router::on<R"(/)">([](cppcoro::router::context<context_t> context,
							  cppcoro::router::context<int> id) { return *id + context->id; })
	};
	REQUIRE(std::get<int>(router("/", 0)) == 42);
	REQUIRE(std::get<int>(router("/", 42)) == 84);
	REQUIRE(std::get<std::string>(router("/hello/world", 0)) == "Hello world !");
}
