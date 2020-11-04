#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/async_scope.hpp>
#include <cppcoro/fmt/span.hpp>
#include <cppcoro/http/connection.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/serve.hpp>
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

namespace cppcoro::net
{
	struct use_ssl_t
	{
	} use_ssl;

	namespace impl
	{
		template<typename ConnectionT>
		struct connect_t;

		template<>
		struct connect_t<net::socket>
		{
			template<typename... ArgsT>
			task<net::socket>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT&&... args_v)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				auto socket =
					net::make_socket<net::socket_mode::client, net::socket>(service, endpoint);
				if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_token>(args));
				}
				else if constexpr (cppcoro::detail::in_tuple<
									   args_t,
									   std::reference_wrapper<cancellation_source>>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_source&>(args).token());
				}
				else
				{
					co_await socket.connect(endpoint);
				}
				co_return socket;
			}
		};

		template<>
		struct connect_t<net::ssl::socket>
		{
			template<typename... ArgsT>
			task<net::ssl::socket>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT... args_v)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				auto socket = std::apply(
					net::make_socket<net::socket_mode::client, net::ssl::socket, ArgsT...>,
					std::tuple_cat(std::forward_as_tuple(service, endpoint), std::move(args)));
				if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_token>(args));
					co_await socket.encrypt(std::get<cancellation_token>(args));
				}
				else if constexpr (cppcoro::detail::in_tuple<
									   args_t,
									   std::reference_wrapper<cancellation_source>>)
				{
					co_await socket.connect(endpoint, std::get<cancellation_source&>(args).token());
					co_await socket.encrypt(std::get<cancellation_source&>(args).token());
				}
				else
				{
					co_await socket.connect(endpoint);
					co_await socket.encrypt();
				}
				co_return socket;
			}
		};

		template<net::is_connection ConnectionT>
		struct connect_t<ConnectionT>
		{
			template<typename... ArgsT>
			task<ConnectionT>
			operator()(io_service& service, const ip_endpoint& endpoint, ArgsT... args_v)
			{
				auto args = std::make_tuple(std::forward<ArgsT>(args_v)...);
				using args_t = decltype(args);
				static_assert(
					(not cppcoro::detail::in_tuple<args_t, cancellation_token> and
						not cppcoro::detail::in_tuple<args_t, std::reference_wrapper<cancellation_source>>),
					"cancellation token is required");
				auto ct = [&]() {
					if constexpr (cppcoro::detail::in_tuple<args_t, cancellation_token>)
					{
						return cancellation_token{ std::get<cancellation_token>(args) };
					}
					else {
						return std::get<cancellation_source&>(args).token();
					}
				};
				co_return ConnectionT{
					co_await std::apply(
						connect_t<typename ConnectionT::socket_type>{},
						std::tuple_cat(std::forward_as_tuple(service, endpoint), std::move(args))),
					ct()
				};
			}
		};
	}  // namespace impl

	template<typename... ArgsT>
	auto connect(io_service& service, const ip_endpoint& endpoint, ArgsT... args)
	{
		using args_t = std::tuple<ArgsT...>;
		if constexpr (cppcoro::detail::in_tuple<args_t, use_ssl_t>)
		{
			return impl::connect_t<ssl::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
		else if constexpr (
			cppcoro::detail::in_tuple<args_t, ssl::certificate> and
			cppcoro::detail::in_tuple<args_t, ssl::private_key>)
		{
			return impl::connect_t<ssl::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
		else
		{
			return impl::connect_t<net::socket>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
	}

	template<net::is_connection ConnectionT, typename... ArgsT>
	auto connect(io_service& service, const net::ip_endpoint& endpoint, ArgsT... args)
	{
		using args_t = std::tuple<ArgsT...>;
		if constexpr (
			cppcoro::detail::in_tuple<args_t, use_ssl_t> or
			(cppcoro::detail::in_tuple<args_t, ssl::certificate> and
			 cppcoro::detail::in_tuple<args_t, ssl::private_key>))
		{
			return impl::connect_t<ConnectionT>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
		else
		{
			return impl::connect_t<ConnectionT>{}(service, endpoint, std::forward<ArgsT>(args)...);
		}
	}

}  // namespace cppcoro::net

TEMPLATE_TEST_CASE(
	"echo tcp server", "[cppcoro-http][server][echo]"//, net::socket
#ifdef CPPCORO_HTTP_MBEDTLS
	,
	net::ssl::socket
#endif
)
{
#ifdef CPPCORO_HTTP_MBEDTLS
	namespace ssl_args = net::ssl_args;
#endif
	http::logging::log_level = spdlog::level::debug;
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
			ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::none }
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
				ssl_args::verify_flags{ ssl_args::verify_flags::allow_untrusted },
				ssl_args::peer_verify_mode { ssl_args::peer_verify_mode::none }
#endif
			);
		}(),
		echoClient(),
		[&]() -> task<> {
			ioSvc.process_events();
			co_return;
		}()));
}

// auto receive = [&]() -> task<> {
//  std::uint8_t buffer[100];
//  std::uint64_t total_bytes_received = 0;
//  std::size_t bytes_received;
//  auto message = net::make_rx_message(con, std::span{ buffer });
//  while ((bytes_received = co_await message.receive()) != 0) {
//      spdlog::debug("client received: {}", std::span{buffer, bytes_received});
//      for (std::size_t i = 0; i < bytes_received; ++i)
//      {
//          std::uint64_t byte_index = total_bytes_received + i;
//          std::uint8_t expected_byte = 'a' + (byte_index % 26);
//          CHECK(buffer[i] == expected_byte);
//      }
//  }
//  CHECK(total_bytes_received == 1000);
//};
//
// auto send = [&]() -> task<> {
//  std::uint8_t buffer[100]{};
//  auto message = net::make_tx_message(con, std::span{buffer});
//  for (std::uint64_t i = 0; i < 1000; i += sizeof(buffer))
//  {
//      for (std::size_t j = 0; j < sizeof(buffer); ++j)
//      {
//          buffer[j] = 'a' + ((i + j) % 26);
//      }
//      auto sent_bytes = co_await message.send();
//      spdlog::info("client sent {} bytes", sent_bytes);
//      spdlog::debug("client sent: {}", std::span{buffer, sent_bytes});
//  }
//};

//
// TEST_CASE("echo http server", "[cppcoro-http][server][echo]")
//{
//	http::logging::log_level = spdlog::level::debug;
//	spdlog::set_level(spdlog::level::debug);
//
//	io_service ioSvc{ 512 };
//	constexpr size_t client_count = 25;
//	const auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4243");
//	cancellation_source source{};
//
//	sync_wait(when_all(
//		[&]() -> task<> {
//			auto _ = on_scope_exit([&] { ioSvc.stop(); });
//			co_await net::serve(
//				ioSvc,
//				endpoint,
//				[&](http::server_connection connection, cancellation_token ct) -> task<> {
//					std::array<uint8_t, 1024> buffer{};
//					auto parser = connection.make_parser();
//					bool done = co_await connection.receive(parser,
// std::as_writable_bytes(std::span{ buffer
//})); 					REQUIRE(done); 					co_await
// connection.send(http::status::HTTP_STATUS_OK, parser.body());
//
//					connection.close_send();
//
//					co_await connection.disconnect();
//				},
//				source.token());
//		}(),
//		[&]() -> task<> {
//			auto _ = on_scope_exit([&] { source.request_cancellation(); });
//			auto con =
//				co_await net::connect<http::client_connection>(ioSvc, endpoint, source.token());
//			auto response = co_await con.get("/", "hello");
//			spdlog::info("got: {}", *response);
//			co_return;
//		}(),
//		[&]() -> task<> {
//			ioSvc.process_events();
//			co_return;
//		}()));
//}
