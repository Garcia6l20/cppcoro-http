#include <catch2/catch.hpp>

//#include <cppcoro/ws/server.hpp>
//#include <cppcoro/ws/client.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/ws/connection.hpp>

//#include <cppcoro/http/session.hpp>

#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>

using namespace cppcoro;

struct base_test
{
	using server_con_type = ws::server_connection<net::socket>;
	using client_con_type = ws::client_connection<net::socket>;
};

#if CPPCORO_HTTP_HAS_SSL
#include <cppcoro/ssl/socket.hpp>
#include "../ssl/cert.hpp"

struct ssl_test
{
	using server_con_type = ws::server_connection<net::ssl::socket>;
	using client_con_type = ws::client_connection<net::ssl::socket>;
};
#endif

TEMPLATE_TEST_CASE(
	"echo web socket server",
	"[cppcoro-http][http-server][echo]",
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

	//    http::logging::log_level = spdlog::level::debug;
	spdlog::set_level(spdlog::level::debug);

	io_service ioSvc{ 512 };
	constexpr size_t client_count = 25;
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4343");

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
			auto rx = co_await net::make_rx_message(con, std::span{ buffer });

			spdlog::info("client received header: payload-length: {} bytes", *rx.payload_length);

			net::readable_bytes body{};

			while ((body = co_await rx.receive()).size() != 0)
			{
				spdlog::debug("client received: {} bytes", body.size_bytes());
				spdlog::debug(
					"client received: {}",
					std::string_view{ reinterpret_cast<const char*>(body.data()), body.size() });
				for (std::size_t i = 0; i < body.size_bytes(); ++i)
				{
					std::uint64_t byte_index = total_bytes_received + i;
					std::byte expected_byte = std::byte('a' + (byte_index % 26));
					CHECK(body[i] == expected_byte);
				}
				total_bytes_received += body.size_bytes();
			}
			CHECK(total_bytes_received == 1000);
		};
		auto send = [&]() -> task<> {
			std::uint64_t total_bytes_sent = 0;
			std::uint8_t buffer[100]{};
			auto tx = co_await net::make_tx_message(con, 1000);

			for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
			{
				for (std::size_t j = 0; j < sizeof(buffer); ++j)
				{
					buffer[j] = 'a' + ((i + j) % 26);
				}
				auto sent_bytes = co_await tx.send(std::span{buffer});
				total_bytes_sent += sent_bytes;
				spdlog::info("client sent {} bytes", sent_bytes);
				spdlog::debug(
					"client sent: {}",
					std::string_view{ reinterpret_cast<const char*>(&buffer[0]), sent_bytes });
			}
			CHECK(total_bytes_sent == 1000);
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
					std::uint64_t total_bytes_received = 0;
					char buffer[100]{};
					char tx_buffer[100]{};
					auto rx = co_await net::make_rx_message(connection, std::span{ buffer });

					spdlog::info(
						"server received header: payload-length: {} bytes", *rx.payload_length);

					auto tx = co_await net::make_tx_message(
						connection, *rx.payload_length);

					net::readable_bytes body{};

					while ((body = co_await rx.receive()).size() != 0)
					{
						spdlog::info(
							"server received {} bytes: {}",
							body.size(),
							std::string_view{ reinterpret_cast<const char*>(body.data()),
											  body.size() });
						total_bytes_received += body.size_bytes();
						auto bytes_sent = co_await tx.send(body);
						REQUIRE(bytes_sent == body.size_bytes());
					}
					REQUIRE(total_bytes_received == 1000);
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
