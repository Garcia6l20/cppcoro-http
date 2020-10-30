#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/generator.hpp>
#include <cppcoro/http/chunk_provider.hpp>
#include <cppcoro/http/client.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/server.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/write_only_file.hpp>

using namespace cppcoro;

template<typename ConfigT>
struct test_reader_controller;

template<typename ConfigT>
using test_reader_controller_def = http::route_controller<
	R"(/read)",  // route definition
	ConfigT,
	http::string_request,
	test_reader_controller>;

template<typename ConfigT>
struct test_reader_controller : test_reader_controller_def<ConfigT>
{
	using test_reader_controller_def<ConfigT>::test_reader_controller_def;

	auto on_get() -> task<http::read_only_file_chunked_response>
	{
		co_return http::read_only_file_chunked_response{ http::status::HTTP_STATUS_OK,
														 http::read_only_file_chunk_provider{
															 this->service(), __FILE__ } };
	}
};

template<typename ConfigT>
struct test_writer_controller;

template<typename ConfigT>
using test_writer_controller_def = http::route_controller<
	R"(/write/([\w\.]+))",  // route definition
	ConfigT,
	http::write_only_file_chunked_request,
	test_writer_controller>;

template<typename ConfigT>
struct test_writer_controller : test_writer_controller_def<ConfigT>
{
	using test_writer_controller_def<ConfigT>::test_writer_controller_def;

	void init_request(std::string_view filename, http::write_only_file_chunked_request& request)
	{
		request.body_access.init(filename);
	}

	task<http::string_response> on_post(std::string_view filename)
	{
		co_return http::string_response{ http::status::HTTP_STATUS_OK,
										 fmt::format("successfully wrote {}", filename) };
	}
};

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]")
{
	io_service ios;

	GIVEN("An chunk server")
	{
		using chunk_server =
			http::controller_server<http::default_session_config<>, test_reader_controller, test_writer_controller>;
		chunk_server server{ ios, *net::ip_endpoint::from_string("127.0.0.1:4242") };

		WHEN("...")
		{
			http::client<tcp::ipv4_socket_provider> client{ ios };
			sync_wait(when_all(
				[&]() -> task<> {
					auto _ = on_scope_exit([&] { ios.stop(); });
					co_await server.serve();
				}(),
				[&]() -> task<> {
					auto _ = on_scope_exit([&] { server.stop(); });
					auto conn =
						co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
					auto f = read_only_file::open(ios, __FILE__);
					std::string content;
					content.resize(f.size());
					auto [body, f_size] = co_await when_all(
						[&]() -> task<std::string> {
							auto response = co_await conn.get("/read");
							REQUIRE(response->status == http::status::HTTP_STATUS_OK);
							co_return std::string{ co_await response->read_body() };
						}(),
						f.read(0, content.data(), content.size()));
					REQUIRE(body == content);
					auto response = co_await conn.post("/write/test.txt", std::move(body));
					REQUIRE(response->status == http::status::HTTP_STATUS_OK);
					auto f2 = read_only_file::open(ios, "test.txt");
					std::string content2;
					content2.resize(f2.size());
					co_await f2.read(0, content2.data(), content2.size());
					REQUIRE(content2 == content);  // copied successful
					co_return;
				}(),
				[&]() -> task<> {
					ios.process_events();
					co_return;
				}()));
		}
	}
}
