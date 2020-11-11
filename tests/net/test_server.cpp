#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/fmt/span.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

struct base_test
{
	using server_con_type = tcp::server_connection<net::socket>;
	using client_con_type = tcp::client_connection<net::socket>;
};

#if CPPCORO_HTTP_HAS_SSL
#include "../ssl/cert.hpp"

struct ssl_test
{
	using server_con_type = tcp::server_connection<net::ssl::socket>;
	using client_con_type = tcp::client_connection<net::ssl::socket>;
};
#endif

TEMPLATE_TEST_CASE(
	"echo tcp server",
	"[cppcoro-http][tcp-server][echo]",
	base_test
#if CPPCORO_HTTP_HAS_SSL
	,
	ssl_test
#endif
)
{
#if CPPCORO_HTTP_HAS_SSL
	namespace ssl_args = net::ssl_args;
#endif

	using server_connection = typename TestType::server_con_type;
	using client_connection = typename TestType::client_con_type;

	spdlog::set_level(spdlog::level::debug);

	io_service ioSvc{ 512 };
	constexpr size_t client_count = 25;
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

	static_assert(net::ssl::is_socket<net::ssl::socket>);

	cancellation_source source{};
	auto echoClient = [&]() -> task<> {
		auto _ = on_scope_exit([&] { source.request_cancellation(); });
		auto con = co_await net::connect<client_connection>(
			ioSvc,
			endpoint,
			std::ref(source)
#if CPPCORO_HTTP_HAS_SSL
				,
			ssl_args::verify_flags{ ssl_args::verify_flags::allow_untrusted },
			ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::required }
#endif
		);

		auto receive = [&]() -> task<> {
			std::uint8_t buffer[100];
			std::uint64_t total_bytes_received = 0;
			std::size_t bytes_received;
			auto message = co_await net::make_rx_message(con, std::span{ buffer });
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
			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				auto message = co_await net::make_tx_message(con);
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}
				auto sent_bytes = co_await message.send(std::as_bytes(std::span{ buffer }));
				spdlog::info("client sent {} bytes", sent_bytes);
				spdlog::debug("client sent: {}", std::span{ buffer, sent_bytes });
			}
			con.close_send();
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
				[&](server_connection connection) -> task<> {
					size_t bytes_received;
					char buffer[64]{};
					auto rx = co_await net::make_rx_message(connection, std::span{ buffer });
					auto tx = co_await net::make_tx_message(connection);
					while ((bytes_received = co_await rx.receive()) != 0)
					{
						spdlog::info("server received {} bytes", bytes_received);
						spdlog::debug("server received: {}", std::span{ buffer, bytes_received });
						auto bytes_sent = co_await tx.send(std::span{ buffer, bytes_received });
						REQUIRE(bytes_sent == bytes_received);
					}
					spdlog::info("closing server connection");
					co_await connection.disconnect();
				},
				std::ref(source)
#if CPPCORO_HTTP_HAS_SSL
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
