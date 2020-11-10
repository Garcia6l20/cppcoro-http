#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/generator.hpp>
#include <cppcoro/http/connection.hpp>
#include <cppcoro/net/connect.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]")
{
	spdlog::set_level(spdlog::level::debug);
	spdlog::flush_on(spdlog::level::debug);

	io_service ios{ 128 };
	cancellation_source cancel{};
	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

	GIVEN("A chunked echo server/client")
	{
		auto server = [&]() -> task<> {
			auto _ = on_scope_exit([&] { ios.stop(); });
			co_await net::serve(
				ios,
				endpoint,
				[](http::server_connection<net::socket> con) -> task<> {
					net::byte_buffer<256> buffer{};
					net::byte_buffer<256> tx_buffer{};
					auto rx = co_await net::make_rx_message(con, std::span{ buffer });
					REQUIRE(rx.chunked);
					http::headers hdrs{ { "Transfer-Encoding", "chunked" } };
					auto tx = co_await net::make_tx_message(
						con, std::span{ tx_buffer }, http::status::HTTP_STATUS_OK, std::move(hdrs));
					net::readable_bytes body;
					while ((body = co_await rx.receive()).size() != 0)
					{
						spdlog::debug(
							"server received {} bytes: {}",
							body.size(),
							std::string_view{ reinterpret_cast<const char*>(body.data()),
											  body.size() });
						co_await tx.send(body);
					}
					con.close_send();
					co_await con.disconnect();
				},
				std::ref(cancel));
		};

		auto client = [&]() -> task<> {
			auto _ = on_scope_exit([&] { cancel.request_cancellation(); });
			auto con = co_await net::connect<http::client_connection<net::socket>>(
				ios, endpoint, std::ref(cancel));

			size_t total_bytes_sent = 0;
			net::byte_buffer<256> buffer{};
			http::headers hdrs{ { "Transfer-Encoding", "chunked" } };
			auto tx = co_await net::make_tx_message(
				con, std::span{ buffer }, http::method::post, "/", std::move(hdrs));

			for (std::uint64_t i = 0; i < buffer.size(); ++i)
			{
				buffer[i] = std::byte('a' + ((total_bytes_sent + i) % 26));
			}
			total_bytes_sent += co_await tx.send();
			con.close_send();

			auto rx = co_await net::make_rx_message(con, std::span{ buffer });
			REQUIRE(rx.chunked);
			net::readable_bytes body;
			size_t total_bytes_received = 0;
			while ((body = co_await rx.receive()).size() != 0)
			{
				for (std::uint64_t i = 0; i < body.size_bytes(); ++i)
				{
					REQUIRE(body[i] == std::byte('a' + ((total_bytes_received + i) % 26)));
				}
				total_bytes_received += body.size_bytes();
			}

			co_await con.disconnect();
		};

		WHEN("...")
		{
			sync_wait(when_all(server(), client(), [&]() -> task<> {
				ios.process_events();
				co_return;
			}()));
		}
	}
}
