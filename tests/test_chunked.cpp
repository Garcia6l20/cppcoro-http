#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::detail::abstract_response<std::string> response{http::status::HTTP_STATUS_OK};

    struct chunker {
        int counter = 0;
        task<std::string_view> read(size_t sz) {
            fmt::print("chunk {}\n", counter);
            if (counter++ > 3)
                co_return std::string_view{};
            co_return std::string_view{"hello"};
        }
    };
    static_assert(http::detail::ro_chunked_body<chunker>);

    using chunk_response = http::detail::abstract_response<chunker>;

    io_service ios;
    struct session {};
    struct chunk_controller : http::route_controller<
        R"(/)",  // route definition
        session,
        chunk_controller>
    {
        auto on_get() -> task<chunk_response> {
            fmt::print("get ...\n");
            co_return chunk_response {http::status::HTTP_STATUS_OK,
                                      chunker{}};
        }
    };
    using chunk_server = http::controller_server<session, chunk_controller>;
    chunk_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};
    GIVEN("An chunk server") {
        WHEN("...") {
            http::client client{ios};
            sync_wait(when_all(
                [&]() -> task<> {
                    auto _ = on_scope_exit([&] {
                        ios.stop();
                    });
                    co_await server.serve();
                } (),
                [&]() -> task<> {
//                    auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
//                    auto response = co_await conn.get();
//                    auto body = co_await response->read_body();
//                    REQUIRE(body == "hellohellohello");
//                    server.stop();
                    co_return;
                }(),
                [&]() -> task<> {
                    ios.process_events();
                    co_return;
                }()));
        }
    }
}
