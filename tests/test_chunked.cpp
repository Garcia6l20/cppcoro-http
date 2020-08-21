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
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;


SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::detail::abstract_response<std::string> response{http::status::HTTP_STATUS_OK};

    io_service ios;

    GIVEN("An chunk server") {

        struct chunker
        {
            std::reference_wrapper<io_service> service;
            std::string path_;

            // needed for default construction
            chunker(io_service &service) noexcept: service{service} {}

            chunker(io_service &service, std::string_view path) noexcept: service{service}, path_{path} {}

            chunker(chunker &&other) noexcept = default;

            chunker &operator=(chunker &&other) noexcept = default;

            async_generator <std::string_view> read(size_t sz) {
                auto f = read_only_file::open(service, path_);
                std::string buffer;
                buffer.resize(sz);
                uint64_t offset = 0;
                auto to_send = f.size();
                size_t res;
                do {
                    res = co_await f.read(offset, buffer.data(), sz);
                    to_send -= res;
                    offset += res;
                    co_yield std::string_view{buffer.data(), res};
                } while (to_send);
            }
        };
        static_assert(std::constructible_from<chunker, io_service &>);

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

            auto on_get() -> task <chunk_response> {
                fmt::print("get ...\n");
                co_return chunk_response{http::status::HTTP_STATUS_OK,
                                         chunker{service(), __FILE__}};
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
