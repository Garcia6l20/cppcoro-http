#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/generator.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;


SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::detail::abstract_response<std::string> response{http::status::HTTP_STATUS_OK};


    io_service ios;

    GIVEN("An chunk server") {

        struct chunker
        {
            std::reference_wrapper<io_service> service;

            chunker(io_service &service) noexcept : service{service} {}

            chunker(chunker &&other) noexcept = default;
            chunker &operator=(chunker &&other) noexcept = default;

            generator<std::string_view> read(size_t sz) {
                co_yield std::string_view{"chunk 0\n"};
                co_yield std::string_view{"chunk 1\n"};
                co_yield std::string_view{"chunk 2\n"};
            }
        };

        struct session
        {
        };
        static_assert(http::detail::ro_chunked_body<chunker>);

        using chunk_response = http::detail::abstract_response<chunker>;

        struct chunk_controller;

        using chunk_controller_def = http::route_controller<
            R"(/)",  // route definition
            session,
            chunk_controller>;

        struct chunk_controller : chunk_controller_def
        {
            using chunk_controller_def::chunk_controller_def;
            auto on_get() -> task<chunk_response> {
                fmt::print("get ...\n");
                co_return chunk_response{http::status::HTTP_STATUS_OK,
                                         chunker{service()}};
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
                    auto body = co_await response->read_body();
                    REQUIRE(body == "chunk 0\n"
                                    "chunk 1\n"
                                    "chunk 2\n");
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
