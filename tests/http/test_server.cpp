#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/async_scope.hpp>
#include <cppcoro/fmt/span.hpp>
#include <cppcoro/http/connection.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <ranges>
#include <thread>

namespace rng = std::ranges;
using namespace cppcoro;

struct base_test
{
	using server_con_type = http::server_connection<net::socket>;
	using client_con_type = http::client_connection<net::socket>;
	static constexpr size_t thread_count = 0;
	static constexpr size_t client_count = 2;
	static constexpr size_t message_count = 10;
	static constexpr size_t packet_count = 10;
};

struct multi_threaded_test
{
	using server_con_type = http::server_connection<net::socket>;
	using client_con_type = http::client_connection<net::socket>;
	static constexpr size_t thread_count = 4;
	static constexpr size_t client_count = 1;
	static constexpr size_t message_count = 10;
	static constexpr size_t packet_count = 10;
};

#if CPPCORO_HTTP_HAS_SSL
#include "../ssl/cert.hpp"

struct ssl_test
{
	using server_con_type = http::server_connection<net::ssl::socket>;
	using client_con_type = http::client_connection<net::ssl::socket>;
	static constexpr size_t thread_count = 0;
	static constexpr size_t client_count = 1;
	static constexpr size_t message_count = 10;
	static constexpr size_t packet_count = 10;
};

struct multi_threaded_ssl_test
{
	using server_con_type = http::server_connection<net::ssl::socket>;
	using client_con_type = http::client_connection<net::ssl::socket>;
	static constexpr size_t thread_count = 4;
	static constexpr size_t client_count = 1;
	static constexpr size_t message_count = 10;
	static constexpr size_t packet_count = 10;
};
#endif

TEMPLATE_TEST_CASE(
	"echo http server",
	"[cppcoro-http][http-server][echo]",
	base_test,
	multi_threaded_test
#if CPPCORO_HTTP_HAS_SSL
	,
	ssl_test,
	multi_threaded_ssl_test
#endif
)
{
#if CPPCORO_HTTP_HAS_SSL
	namespace ssl_args = net::ssl_args;
#endif

	using server_connection = typename TestType::server_con_type;
	using client_connection = typename TestType::client_con_type;
	static constexpr auto thread_count = TestType::thread_count;
	static constexpr auto client_count = TestType::client_count;
	static constexpr auto message_count = TestType::message_count;
	static constexpr auto packet_count = TestType::packet_count;

	http::logging::log_level = spdlog::level::debug;
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::info);

	io_service ioSvc{ 512 };
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

	cancellation_source canceller{};
	auto echo_client = [](size_t id,
						  io_service& service,
						  const net::ip_endpoint& endpoint,
						  cancellation_source& cancellation_source) -> task<> {
		auto con = co_await net::connect<client_connection>(
			service,
			endpoint,
			std::ref(cancellation_source)
#ifdef CPPCORO_HTTP_MBEDTLS
				,
			ssl_args::verify_flags{ ssl_args::verify_flags::allow_untrusted },
			ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::required }
#endif
		);

		auto receive = [&]() -> task<> {
			std::uint8_t buffer[100];
			for (int ii = 0; ii < message_count; ++ii)
			{
				std::uint64_t total_bytes_received = 0;
				auto rx = co_await net::make_rx_message(con, std::span{ buffer });

				spdlog::info(
					"client {} rx message: content-length: {} bytes",
					id,
					rx.content_length.value());

				net::readable_bytes body{};

				while ((body = co_await rx.receive()).size() != 0)
				{
					spdlog::debug("client {} received: {} bytes", id, body.size_bytes());
					spdlog::debug(
						"client {} received: {}",
						id,
						std::string_view{ reinterpret_cast<const char*>(body.data()),
										  body.size() });
					for (std::size_t i = 0; i < body.size_bytes(); ++i)
					{
						std::uint64_t byte_index = total_bytes_received + i;
						std::byte expected_byte = std::byte('a' + (byte_index % 26));
						if (std::byte{ buffer[i] } != expected_byte)
						{
							throw std::runtime_error(fmt::format(
								"client {}: invalid byte detected: {} != {}",
								id,
								char(buffer[i]),
								char(expected_byte)));
						}
					}
					total_bytes_received += body.size_bytes();
				}
				spdlog::debug("client {} message {}/{} received", id, ii + 1, message_count);
				if (*rx.content_length != 100 * packet_count)
				{
					throw std::runtime_error(fmt::format(
						"client {}: invalid content length : {} != {}",
						id,
						*rx.content_length,
						100 * packet_count));
				}
				if (total_bytes_received != 100 * packet_count)
				{
					throw std::runtime_error(fmt::format(
						"client {} invalid byte count received : {} != {}",
						id,
						total_bytes_received,
						100 * packet_count));
				}
			}
		};
		auto send = [&]() -> task<> {
			std::uint8_t buffer[100]{};
			for (int ii = 0; ii < message_count; ++ii)
			{
				auto tx = co_await net::make_tx_message(
					con,
					http::method::post,
					std::string_view{ "/" },
					size_t{ packet_count * sizeof(buffer) });

				for (std::uint64_t i = 0; i < sizeof(buffer) * packet_count; i += sizeof(buffer))
				{
					for (std::size_t j = 0; j < sizeof(buffer); ++j)
					{
						buffer[j] = 'a' + ((i + j) % 26);
					}
					auto sent_bytes = co_await tx.send(std::span{ buffer });
					spdlog::info("client {} sent {} bytes", id, sent_bytes);
					spdlog::debug(
						"client {} sent: {}",
						id,
						std::string_view{ reinterpret_cast<const char*>(&buffer[0]), sent_bytes });
				}
				spdlog::debug("client {} message {}/{} sent", id, ii + 1, message_count);
			}
			con.close_send();
		};

		co_await when_all(send(), receive());
		//		co_await when_all(send());
		//
		//		using namespace std::chrono_literals;
		//		co_await ioSvc.schedule_after(2s);

		co_await con.disconnect();
		cancellation_source.request_cancellation();
	};

	auto run_all_clients = [&]() -> task<> {
		async_scope scope{};
		std::exception_ptr exception_ptr = nullptr;
		try
		{
			for (size_t client = 0; client < client_count; ++client)
			{
				scope.spawn(echo_client(client, ioSvc, endpoint, canceller));
			}
		}
		catch (operation_cancelled&)
		{
		}
		catch (...)
		{
			exception_ptr = std::current_exception();
		}
		co_await scope.join();
		spdlog::debug("all clients terminated, stopping server");
		canceller.request_cancellation();
		if (exception_ptr)
		{
			std::rethrow_exception(exception_ptr);
		}
	};

	auto serve = [&]() -> task<> {
		auto _ = on_scope_exit([&] { ioSvc.stop(); });
		co_await net::serve(
			ioSvc,
			endpoint,
			[&](server_connection connection) -> task<> {
				char buffer[100]{};
				size_t message_num = 0;
				while (true)
				{
					auto rx = co_await net::make_rx_message(connection, std::span{ buffer });
					spdlog::info(
						"server received header: content-length: {} bytes",
						rx.content_length.value());

					++message_num;

					auto tx = co_await net::make_tx_message(
						connection, http::status::HTTP_STATUS_OK, *rx.content_length);

					net::readable_bytes body{};

					while ((body = co_await rx.receive()).size() != 0)
					{
						spdlog::debug(
							"server received {} bytes: {}",
							body.size(),
							std::string_view{ reinterpret_cast<const char*>(body.data()),
											  body.size() });
						auto bytes_sent = co_await tx.send(body);
						REQUIRE(bytes_sent == body.size_bytes());
					}
					spdlog::info("server message {} done", message_num);
				}
			},
			std::ref(canceller)
#ifdef CPPCORO_HTTP_MBEDTLS
				,
			net::ssl::certificate{ cert },
			net::ssl::private_key{ key },
			ssl_args::host_name{ "localhost" },
			ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::optional }
#endif
		);
	};

	std::vector<std::thread> tp{ thread_count };

	rng::generate(
		tp, [&ioSvc] { return std::thread{ [&ioSvc]() mutable { ioSvc.process_events(); } }; });

	sync_wait(when_all(
		serve(),
		echo_client(1, ioSvc, endpoint, canceller),
		//					   run_all_clients(),
		[&]() -> task<> {
			if constexpr (not thread_count)
				ioSvc.process_events();
			co_return;
		}()));

	rng::for_each(tp, [](auto&& thread) { thread.join(); });
}
