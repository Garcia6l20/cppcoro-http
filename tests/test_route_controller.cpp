#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>

using namespace cppcoro;

struct session
{
    int id = std::rand();
};

struct hello_controller : cppcoro::http::route_controller<R"(/hello/(\w+))", session, hello_controller>
{
    auto on_post(std::string_view who) -> task<http::response> {
        fmt::print("post on {}\n", session().id);
        co_return http::response{http::status::HTTP_STATUS_OK, fmt::format("post: {}", who)};
    }
    auto on_get(const std::string &who) -> task<http::response> {
        fmt::print("get on {}\n", session().id);
        co_return http::response{http::status::HTTP_STATUS_OK, fmt::format("get: {}", who)};
    }
};

SCENARIO("route controller are easy to use", "[cppcoro-http][router]") {
    cppcoro::io_service ios;
    static const auto test_endpoint = net::ip_endpoint::from_string("127.0.0.1:4242");
    GIVEN("A simple route controller") {
        http::controller_server<session, hello_controller> server{ios,
                                                                  *test_endpoint};
        http::client client{ios};

        (void) sync_wait(when_all(
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
                using namespace std::chrono_literals;
                auto conn = co_await client.connect(*test_endpoint);
                auto resp = co_await conn.post("/hello/world");
                REQUIRE(resp->body == "post: world");
                resp = co_await conn.get("/hello/world");
                REQUIRE(resp->body == "get: world");
            }(),
            [&]() -> task<> {
                ios.process_events();
                co_return;
            }()
        ));
    }
}
