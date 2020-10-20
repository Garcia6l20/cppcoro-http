#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <thread>

using namespace cppcoro;

namespace rng = std::ranges;

constexpr auto test_thread_count = 3;
constexpr auto test_endpoint = "127.0.0.1:4242";

SCENARIO("echo server should work", "[cppcoro-http][server][echo]") {
    http::logging::log_level = spdlog::level::debug;

    io_service ios;

    struct session {};

    using echo_route_controller_def = http::route_controller<
        R"(/echo)",  // route definition
        session,
        http::string_request,
        struct echo_controller>;

    struct echo_controller : echo_route_controller_def
    {
        using echo_route_controller_def::echo_route_controller_def;
        auto on_get() -> task<http::string_response> {
            co_return http::string_response {http::status::HTTP_STATUS_OK,
                                     fmt::format("{}", co_await request().read_body())};
        }
    };
    using echo_server = http::controller_server<session, echo_controller>;
    echo_server server{ios, *net::ip_endpoint::from_string(test_endpoint)};
    std::vector<std::thread> tp{test_thread_count};
    auto start_threads = [&tp, &ios] {
        rng::generate(tp, [&ios] {
            return std::thread{[&] {
                ios.process_events();
            }};
        });
    };
    auto join_treads = [&tp] {
        rng::for_each(tp, [](auto &&t) {
            t.join();
        });
    };
    GIVEN("An echo server") {
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
                auto _ = on_scope_exit([&] {
                    server.stop();
                });
                auto conn = co_await client.connect(*net::ip_endpoint::from_string(test_endpoint));
                auto response = co_await conn.get("/echo", "hello");
                REQUIRE(response->status == http::status::HTTP_STATUS_OK);
                REQUIRE(co_await response->read_body() == "hello");
            }(),
//            [&]() -> task<> {
//                auto conn = co_await client.connect(*net::ip_endpoint::from_string(test_endpoint));
//                auto response = co_await conn.get("/echo", "olleh");
//                REQUIRE(response->status == http::status::HTTP_STATUS_OK);
//                REQUIRE(co_await response->read_body() == "olleh");
//            }(),
            [&]() -> task<> {
                start_threads();
                co_return;
            }()));
            join_treads();
        }
    }
}
