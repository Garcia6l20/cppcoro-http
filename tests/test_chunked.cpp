#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/generator.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::string_response response{http::status::HTTP_STATUS_OK};

    io_service ios;

    GIVEN("An chunk server") {

        struct session
        {
        };

        struct chunk_controller;

        using chunk_controller_def = http::route_controller<
            R"(/)",  // route definition
            session,
            chunk_controller>;

        struct chunk_controller : chunk_controller_def
        {
            using chunk_controller_def::chunk_controller_def;

            auto on_get() -> task <http::read_only_file_chunked_response> {
                fmt::print("get ...\n");
                co_return http::read_only_file_chunked_response{http::status::HTTP_STATUS_OK,
                                                                http::read_only_file_chunk_provider{service(), __FILE__}};
            }
        };

        using chunk_server = http::controller_server<session, chunk_controller>;
        chunk_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};

        WHEN("...") {
            http::client client{ios};
            sync_wait(when_all(
                [&]() -> task<> {
                    auto _ = on_scope_exit([&] {
                        ios.stop();
                    });
                    co_await server.serve();
                }(),
                [&]() -> task<> {
                    auto _ = on_scope_exit([&] {
                        server.stop();
                    });
                    auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
                    auto response = co_await conn.get();
                    auto f = read_only_file::open(ios, __FILE__);
                    std::string content;
                    content.resize(f.size());
                    auto[body, f_size] = co_await when_all(
                        response->read_body(),
                        f.read(0, content.data(), content.size()));
                    REQUIRE(body == content);
                    co_return;
                }(),
                [&]() -> task<> {
                    ios.process_events();
                    co_return;
                }())
            );
        }
    }
}
