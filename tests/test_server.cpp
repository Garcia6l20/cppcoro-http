#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/router.hpp>
#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <thread>

using namespace cppcoro;

SCENARIO("echo server should work", "[cppcoro-net][server]") {
    io_service ios;
    http::route_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};
    GIVEN("An echo server") {
        auto &route = server.add_route<R"(/echo/(\w+))">();
        // complete handler
        route.on_complete([](const std::string &input) -> task<std::tuple<http::status, std::string>> {
            co_return std::tuple{
                http::status::HTTP_STATUS_OK,
                input
            };
        });
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
                auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
                auto response = co_await conn.post("/echo/hello", "");
                REQUIRE(response->body == "hello");
                server.stop();
            }(),
            [&]() -> task<> {
                ios.process_events();
                co_return;
            }()));
        }
    }
}
