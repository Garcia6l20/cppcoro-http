#include <cppcoro/async_scope.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/http/connection.hpp>
#include <cppcoro/net/connect.hpp>

#include <csignal>
#include <iostream>
#include <thread>

using namespace cppcoro;
namespace rng = std::ranges;

namespace
{
	cancellation_source g_cancellation_source{};
}

void at_exit(int)
{
	fmt::print("exit requested\n");
	g_cancellation_source.request_cancellation();
}

int main(const int argc, const char** argv)
{
	http::logging::log_level = spdlog::level::debug;

	using namespace cppcoro;

	std::vector<std::string_view> args{ argv + 1, argv + argc };
	auto server_endpoint =
		net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
	fmt::print("requesting at '{}'\n", server_endpoint->to_string());

	std::signal(SIGTERM, at_exit);
	std::signal(SIGINT, at_exit);

	io_service service;

	//	static const constinit int thread_count = 5;
	//	std::array<std::thread, thread_count> thread_pool;
	//	rng::generate(thread_pool, [&] { return std::thread([&] { service.process_events(); }); });

	auto post = [](auto& con, std::string data, std::string path = "/") -> task<std::string> {
		auto tx = co_await net::make_tx_message(
			con, http::method::post, std::string_view{ path }, data.size());
		co_await tx.send(std::as_bytes(std::span{ data }));
		std::array<char, 256> buffer{};
		auto rx = co_await net::make_rx_message(con, std::span{ buffer });
		net::readable_bytes body{};
		std::string response_data{};
		while ((body = co_await rx.receive()).size() != 0)
		{
            response_data.append(
				std::string_view{ reinterpret_cast<const char*>(body.data()), body.size() });
		}
        spdlog::info("response: {}", response_data);
		co_return response_data;
	};

	auto do_request = [&]() -> task<> {
		auto _ = on_scope_exit([&] { service.stop(); });
		auto con = co_await net::connect<http::client_connection<net::socket>>(
			service, *server_endpoint, std::ref(g_cancellation_source));

		assert((co_await post(con, "Hello, world")) == "Hello, world");
        assert((co_await post(con, "42")) == "42");
	};

	(void)sync_wait(when_all(do_request(), [&]() -> task<> {
		service.process_events();
		co_return;
	}()));

	//	rng::for_each(thread_pool, [](auto&& th) { th.join(); });

	std::cout << "Bye !\n";
	return 0;
}
