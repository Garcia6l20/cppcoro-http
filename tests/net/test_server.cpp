#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/fmt/span.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

#ifdef CPPCORO_HTTP_MBEDTLS
#include "../ssl/cert.hpp"
using server_socket_provider = ipv4_ssl_server_provider;
using client_socket_provider = ipv4_ssl_client_provider;
#else
using server_socket_provider = tcp::ipv4_socket_provider;
using client_socket_provider = tcp::ipv4_socket_provider;
#endif

#include <cppcoro/net/make_socket.hpp>
#include <cppcoro/detail/function_traits.hpp>

namespace rng = std::ranges;

TEMPLATE_TEST_CASE(
	"echo tcp server",
	"[cppcoro-http][server][echo]",
	net::socket
#ifdef CPPCORO_HTTP_MBEDTLS
	,
	net::ssl::socket
#endif
)
{
#ifdef CPPCORO_HTTP_MBEDTLS
	namespace ssl_args = net::ssl_args;
#endif
	spdlog::set_level(spdlog::level::debug);

	io_service ioSvc{ 512 };
	constexpr size_t client_count = 25;
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4243");

	cancellation_source source{};
	auto echoClient = [&]() -> task<> {
		auto _ = on_scope_exit([&] { source.request_cancellation(); });
		auto client = tcp::client<tcp::ipv4_socket_provider>(ioSvc);
		auto con = co_await net::connect<tcp::connection<TestType>>(
			ioSvc,
			endpoint,
			std::ref(source)
#ifdef CPPCORO_HTTP_MBEDTLS
				,
			ssl_args::verify_flags{ ssl_args::verify_flags::allow_untrusted },
			ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::required }
#endif
		);

		auto receive = [&]() -> task<> {
			std::uint8_t buffer[100];
			std::uint64_t total_bytes_received = 0;
			std::size_t bytes_received;
			auto message = net::make_rx_message(con, std::span{ buffer });
			while ((bytes_received = co_await message.receive()) != 0)
			{
				spdlog::debug("client received: {}", std::span{ buffer, bytes_received });
				for (std::size_t i = 0; i < bytes_received; ++i)
				{
					std::uint64_t byte_index = total_bytes_received + i;
					std::uint8_t expected_byte = 'a' + (byte_index % 26);
					CHECK(buffer[i] == expected_byte);
				}
				total_bytes_received += bytes_received;
			}
			CHECK(total_bytes_received == 1000);
		};
		auto send = [&]() -> task<> {
			std::uint8_t buffer[100]{};
			auto message = net::make_tx_message(con, std::span{ buffer });
			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}
				auto sent_bytes = co_await message.send();
				spdlog::info("client sent {} bytes", sent_bytes);
				spdlog::debug("client sent: {}", std::span{ buffer, sent_bytes });
			}
		};

		co_await when_all(send(), receive());

		co_await con.disconnect();
	};

	sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { ioSvc.stop(); });
			co_await net::serve(
				ioSvc,
				endpoint,
				[&](tcp::connection<TestType> connection) -> task<> {
					try
					{
						size_t bytes_received;
						char buffer[64]{};
						auto rx = net::make_rx_message(connection, std::span{ buffer });
						auto tx = net::make_tx_message(connection, std::span{ buffer });
						while ((bytes_received = co_await rx.receive()) != 0)
						{
							spdlog::info("server received {} bytes", bytes_received);
							spdlog::debug(
								"server received: {}", std::span{ buffer, bytes_received });
							auto bytes_sent = co_await tx.send(bytes_received);
							REQUIRE(bytes_sent == bytes_received);
						}
					}
					catch (operation_cancelled&)
					{
					}
					catch (std::system_error& error)
					{
						if (error.code() != std::errc::connection_reset)
						{
							throw error;
						}
					}
					co_await connection.disconnect();
				},
				std::ref(source)
#ifdef CPPCORO_HTTP_MBEDTLS
					,
				net::ssl::certificate{ cert },
				net::ssl::private_key{ key },
				ssl_args::host_name{ "localhost" },
				ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::optional }
#endif
			);
		}(),
		echoClient(),
		[&]() -> task<> {
			ioSvc.process_events();
			co_return;
		}()));
}
