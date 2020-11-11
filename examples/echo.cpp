#include <cppcoro/async_scope.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <cppcoro/http/connection.hpp>
#include <cppcoro/net/serve.hpp>

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
	fmt::print("listening at '{}'\n", server_endpoint->to_string());

	std::signal(SIGTERM, at_exit);
	std::signal(SIGINT, at_exit);

	io_service service;

//	static const constinit int thread_count = 5;
//	std::array<std::thread, thread_count> thread_pool;
//	rng::generate(thread_pool, [&] { return std::thread([&] { service.process_events(); }); });

	auto do_serve = [&]() -> task<> {
		auto _ = on_scope_exit([&] { service.stop(); });
		co_await net::serve(
			service,
			*server_endpoint,
			[&](http::server_connection<net::socket> con) -> task<> {
				spdlog::info("New connection from {}", con.peer_address().to_string());
				net::byte_buffer<128> buffer{};
				while (true)
				{
					auto rx = co_await net::make_rx_message(
						con, std::as_writable_bytes(std::span{ buffer }));
					if (not rx.content_length)
					{
						spdlog::info("empty content length");
						co_await net::make_tx_message(
							con,
							http::status::HTTP_STATUS_OK);
					}
					else
					{
						spdlog::info("receiving {} bytes", *rx.content_length);
						auto tx = co_await net::make_tx_message(
							con,
							http::status::HTTP_STATUS_OK,
							*rx.content_length);
						net::readable_bytes body{};
						while ((body = co_await rx.receive()).size() != 0)
						{
							co_await tx.send(body);
						}
					}
				}
			},
			std::ref(g_cancellation_source));
	};

	(void)sync_wait(when_all(do_serve(), [&]() -> task<> {
		service.process_events();
		co_return;
	}()));

//	rng::for_each(thread_pool, [](auto&& th) { th.join(); });

	std::cout << "Bye !\n";
	return 0;
}
