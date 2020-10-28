#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include "ssl/cert.hpp"

using namespace cppcoro;

namespace rng = std::ranges;


TEST_CASE("echo server should work", "[cppcoro-http][server][echo]")
{
	http::logging::log_level = spdlog::level::debug;
	spdlog::set_level(spdlog::level::debug);

	io_service ioSvc{ 512 };
	constexpr size_t client_count = 25;

	auto server = tcp::server<ipv4_ssl_server_provider>{ ioSvc, net::ipv4_endpoint{ net::ipv4_address::loopback(), 0 } };

	auto handleConnection = [](auto connection) -> task<void> {
		std::uint8_t buffer[64];
		std::size_t bytesReceived;
		do
		{
			bytesReceived = co_await connection.receive(buffer, sizeof(buffer));
			if (bytesReceived > 0)
			{
				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await connection.send(buffer + bytesSent, bytesReceived - bytesSent);
				} while (bytesSent < bytesReceived);
			}
		} while (bytesReceived > 0);

        connection.close_send();

		co_await connection.disconnect();
	};

	auto echoServer = [&]() -> task<> {
		async_scope connectionScope;

		std::exception_ptr ex;
		try
		{
			while (true)
			{
				auto conn = co_await server.accept();

				connectionScope.spawn(handleConnection(std::move(conn)));
			}
		}
		catch (const cppcoro::operation_cancelled&)
		{
		}
		catch (...)
		{
			ex = std::current_exception();
		}

		co_await connectionScope.join();

		if (ex)
		{
			std::rethrow_exception(ex);
		}
	};

	auto echoClient = [&]() -> task<> {
		auto client = tcp::client<ipv4_ssl_client_provider>(ioSvc);
        auto con = co_await client.connect(server.local_endpoint());

		auto receive = [&]() -> task<> {
			std::uint8_t buffer[100];
			std::uint64_t totalBytesReceived = 0;
			std::size_t bytesReceived;
			do
			{
				bytesReceived = co_await con.receive(buffer, sizeof(buffer));
				for (std::size_t i = 0; i < bytesReceived; ++i)
				{
					std::uint64_t byteIndex = totalBytesReceived + i;
					std::uint8_t expectedByte = 'a' + (byteIndex % 26);
					CHECK(buffer[i] == expectedByte);
				}

				totalBytesReceived += bytesReceived;
			} while (bytesReceived > 0);

			CHECK(totalBytesReceived == 1000);
		};

		auto send = [&]() -> task<> {
			std::uint8_t buffer[100];
			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}

				std::size_t bytesSent = 0;
				do
				{
					bytesSent += co_await con.send(
						buffer + bytesSent, sizeof(buffer) - bytesSent);
				} while (bytesSent < sizeof(buffer));
			}

			con.close_send();
		};

		co_await when_all(send(), receive());

		co_await con.disconnect();
	};

	auto manyEchoClients = [&](int count) -> task<void> {
		auto shutdownServerOnExit = on_scope_exit([&] { server.stop(); });

		std::vector<task<>> clientTasks;
		clientTasks.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			clientTasks.emplace_back(echoClient());
		}

		co_await when_all(std::move(clientTasks));
	};

	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto stopOnExit = on_scope_exit([&] { ioSvc.stop(); });
			(void)co_await when_all(manyEchoClients(client_count), echoServer());
		}(),
		[&]() -> task<> {
			ioSvc.process_events();
			co_return;
		}()));
}
