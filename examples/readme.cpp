#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/server.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/http/chunk_provider.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <fmt/format.h>

using namespace cppcoro;

template<typename ConfigT>
struct session : http::session<ConfigT>
{
	explicit session(http::server<ConfigT>& server)
		: http::session<ConfigT>{ server }
	{
	}
	int id = std::rand();
};

using config = http::config<session, tcp::ipv4_socket_provider>;

template<typename ConfigT>
struct hello_controller;

template<typename ConfigT>
using hello_controller_def = http::route_controller<
	R"(/hello/(\w+))",  // route definition
	ConfigT,
	http::string_request,
	hello_controller>;

template<typename ConfigT>
struct hello_controller : hello_controller_def<ConfigT>
{
	using hello_controller_def<ConfigT>::hello_controller_def;
	// method handlers
	auto on_post(std::string_view who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format(
											 "post at {}: hello {}", this->session().id, who) };
	}
	auto on_get(std::string_view who) -> task<http::string_response>
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format(
											 "get at {}: hello {}", this->session().id, who) };
	}
};

struct hello_chunk_provider : http::abstract_chunk_base
{
	std::string_view who;
	using http::abstract_chunk_base::abstract_chunk_base;
	hello_chunk_provider(io_service& service, std::string_view who)
		: http::abstract_chunk_base{ service }
		, who{ who }
	{
	}
	async_generator<std::string_view> read(size_t)
	{
		co_yield "hello\n";
		co_yield fmt::format("{}\n", who);
	}
};
using hello_chunked_response = http::abstract_response<hello_chunk_provider>;

template<typename ConfigT>
struct hello_chunk_controller;

template<typename ConfigT>
using hello_chunk_controller_def = http::route_controller<
	R"(/chunk/(\w+))",  // route definition
	ConfigT,
	http::string_request,
	hello_chunk_controller>;

template<typename ConfigT>
struct hello_chunk_controller : hello_chunk_controller_def<ConfigT>
{
	using hello_chunk_controller_def<ConfigT>::hello_chunk_controller_def;
	// method handlers
	task<hello_chunked_response> on_get(std::string_view who)
	{
		co_return hello_chunked_response{ http::status::HTTP_STATUS_OK,
										  hello_chunk_provider{ this->service(), who } };
	}
};

int main()
{
	io_service service;

	auto do_serve = [&]() -> task<> {
		auto _ = on_scope_exit([&] { service.stop(); });
		http::controller_server<config, hello_controller, hello_chunk_controller> server{
			service, *net::ip_endpoint::from_string("127.0.0.1:4242")
		};
		co_await server.serve();
	};
	(void)sync_wait(when_all(do_serve(), [&]() -> task<> {
		service.process_events();
		co_return;
	}()));
	return 0;
}
