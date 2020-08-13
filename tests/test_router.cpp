#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/router.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/details/router.hpp>

using namespace cppcoro;

SCENARIO("router can handle arguments", "[cppcoro-net][router]") {
    io_service service;
    http::router router;
    auto process_request = [&](const http::request &request) {
        auto [response, _] = sync_wait(when_all(
            [&]() -> task<http::response> {
                auto _ = on_scope_exit([&] {
                    service.stop();
                });
                http::response response = co_await router.process(request);
                co_return response;
            }(),
            [&]() -> task<> {
                service.process_events();
                co_return;
            }()
        ));
        return response;
    };
    GIVEN("simple route without args") {
        auto &route = router.add_route<"/hello">();
        route.complete([]() -> task<http::status> {
            co_return http::status::HTTP_STATUS_OK;
        });
        WHEN("This route is hit") {
            http::request request {"/hello", "body", {}};
            http::response response = process_request(request);
            REQUIRE(response.status == http::status::HTTP_STATUS_OK);
        }
        AND_WHEN("An unexpected route is hit") {
            http::request request {"/fake", "body", {}};
            REQUIRE_THROWS_AS(process_request(request), http::router::not_found);
        }
    }
    GIVEN("one arg route") {
        auto &route = router.add_route<R"(/hello/(\w+))">();
        route.complete([](const std::string &who) -> task<std::tuple<http::status, std::string>> {
            co_return std::make_tuple(http::status::HTTP_STATUS_OK, fmt::format("Hello {}", who));
        });
        WHEN("This route is hit") {
            http::request request {"/hello/world", "body", {}};
            http::response response = process_request(request);
            REQUIRE(response.status == http::status::HTTP_STATUS_OK);
            REQUIRE(response.body == "Hello world");
        }
    }
    GIVEN("two args route") {
        auto &route = router.add_route<R"(/hello/(\w+)/(\d+))">();
        route.complete([](const std::string &who, int count) -> task<std::tuple<http::status, std::string>> {
            co_return std::make_tuple(http::status::HTTP_STATUS_OK, fmt::format("Hello {} {}", who, count));
        });
        WHEN("This route is hit") {
            http::request request {"/hello/world/42", "body", {}};
            http::response response = process_request(request);
            REQUIRE(response.status == http::status::HTTP_STATUS_OK);
            REQUIRE(response.body == "Hello world 42");
        }
    }
}
