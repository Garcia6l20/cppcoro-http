#include <catch2/catch.hpp>

//#define CPPCORO_SSL_DEBUG

#include <cppcoro/async_scope.hpp>
#include <cppcoro/net/ssl/socket.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <spdlog/spdlog.h>

using namespace cppcoro;

#include "cert.hpp"

SCENARIO("one ssl client", "[cppcoro-http][ssl]")
{
#ifdef CPPCORO_SSL_DEBUG
	spdlog::set_level(spdlog::level::debug);
#endif

	io_service io_service;
	auto endpoint = *net::ipv4_endpoint::from_string("127.0.0.1:4242");
	bool failure = false;
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { io_service.stop(); });
			try
			{
				auto server = net::socket::create_tcpv4(io_service);
				server.bind(endpoint);
				auto sock = net::ssl::socket::create_server(
					io_service, net::ssl::certificate{ cert }, net::ssl::private_key{ key });
				server.listen();
				co_await server.accept(sock);
				sock.host_name("localhost");
				spdlog::debug("connection accepted");
				co_await sock.encrypt();
				spdlog::debug("connection encrypted");
				uint8_t buffer[64] = {};
				auto bytes_received = co_await sock.recv(buffer, sizeof(buffer));
				REQUIRE(bytes_received == 12);
				std::string_view data{ reinterpret_cast<char*>(buffer), bytes_received };
				spdlog::debug("received {} bytes: {}", bytes_received, data);
				using namespace std::literals;
				REQUIRE(data == "hello ssl !!"sv);
			}
			catch (std::exception& error)
			{
				spdlog::error("server error: {}", error.what());
				failure = true;
			}
			co_return;
		}(),
		[&]() -> task<> {
			try
			{
				std::string_view data{ "hello ssl !!" };
				auto client = net::ssl::socket::create_client(io_service);
				co_await client.connect(endpoint);
				spdlog::debug("connected");
				client.set_peer_verify_mode(net::ssl::peer_verify_mode::required);
				client.set_verify_flags(net::ssl::verify_flags::allow_untrusted);
				co_await client.encrypt();
				spdlog::debug("encrypted");
				auto sent_bytes = co_await client.send(data.data(), data.size());
				REQUIRE(sent_bytes == data.size());
			}
			catch (std::exception& error)
			{
				spdlog::error("client error: {}", error.what());
				failure = true;
			}
		}(),
		[&]() -> task<> {
			io_service.process_events();
			co_return;
		}()));
	REQUIRE(!failure);
}

#include <future>
#include <ranges>

namespace rng = std::ranges;

template<size_t tread_count = 0>
void multi_clients_test()
{
	constexpr size_t client_count = 128;

	io_service io_service{ 256 };
	std::array<std::future<void>, tread_count> futures;

	rng::generate(futures, [&io_service] {
		return std::async([&io_service] { io_service.process_events(); });
	});

	auto endpoint = *net::ipv4_endpoint::from_string("127.0.0.1:4343");
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { io_service.stop(); });
			try
			{
				auto server = net::socket::create_tcpv4(io_service);
				server.bind(endpoint);
				server.listen();
				async_scope scope;
				for (size_t client_num = 0; client_num < client_count; ++client_num)
				{
					auto sock = net::ssl::socket::create_server(
						io_service, net::ssl::certificate{ cert }, net::ssl::private_key{ key });
					sock.host_name("localhost");
					co_await server.accept(sock);
					spdlog::debug("connection {} accepted", client_num + 1);
					scope.spawn([](net::ssl::socket sock, size_t client_num) -> task<> {
						co_await sock.encrypt();
						spdlog::debug("connection {} encrypted", client_num + 1);
						uint8_t buffer[64] = {};
						auto bytes_received = co_await sock.recv(buffer, sizeof(buffer));
						std::string_view data{ reinterpret_cast<char*>(buffer), bytes_received };
						spdlog::debug(
							"received {} bytes from client {}: {}",
							bytes_received,
							client_num + 1,
							data);
						using namespace std::literals;
						REQUIRE(data == fmt::format("hello ssl {} !!", client_num + 1));
					}(std::move(sock), client_num));
				}
				co_await scope.join();
			}
			catch (std::exception& error)
			{
				spdlog::error("server error: {}", error.what());
			}
			co_return;
		}(),
		[&]() -> task<> {
			async_scope scope;
			for (size_t client_num = 0; client_num < client_count; ++client_num)
			{
				auto client = net::ssl::socket::create_client(io_service);
				scope.spawn(
					[](net::ssl::socket client,
					   net::ipv4_endpoint& endpoint,
					   size_t client_num) -> task<> {
						std::string data = fmt::format("hello ssl {} !!", client_num + 1);
						co_await client.connect(endpoint);
						spdlog::debug("connected {}", client_num + 1);
						client.set_peer_verify_mode(net::ssl::peer_verify_mode::required);
						client.set_verify_flags(net::ssl::verify_flags::allow_untrusted);
						co_await client.encrypt();
						spdlog::debug("encrypted {}", client_num + 1);
						auto sent_bytes = co_await client.send(data.data(), data.size());
						REQUIRE(sent_bytes == data.size());
					}(std::move(client), endpoint, client_num));
			}
			co_await scope.join();
		}(),
		[&]() -> task<> {
			io_service.process_events();
			co_return;
		}()));

	rng::for_each(futures, [](auto&& f) { f.get(); });
}

SCENARIO("multiple ssl clients", "[cppcoro-http][ssl]")
{
#ifdef CPPCORO_SSL_DEBUG
	spdlog::set_level(spdlog::level::debug);
#endif
	multi_clients_test();
}

SCENARIO("multiple ssl clients multi-threaded", "[cppcoro-http][ssl]")
{
#ifdef CPPCORO_SSL_DEBUG
	spdlog::set_level(spdlog::level::debug);
#endif
	multi_clients_test<10>();
}
