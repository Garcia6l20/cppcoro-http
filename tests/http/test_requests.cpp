#include <catch2/catch.hpp>

#include <cppcoro/http/request.hpp>
#include <cppcoro/http/response.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/is_awaitable.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

SCENARIO("requests should be customizable", "[cppcoro-http][messages][request]") {
    io_service ios;
    GIVEN("A simple string request") {
        http::string_request req{http::method::post, "/", "hello"};
        WHEN("The http header is built") {
            auto header = req.build_header();
            THEN("It can be parsed") {
                (void) sync_wait(when_all([&]() -> task<> {
                    auto _ = on_scope_exit([&] {
                        ios.stop();
                    });
                    http::string_request result;
                    http::request_parser parser;
                    parser.parse(header);
                    REQUIRE(!parser);
                    parser.parse(req.body_access);
                    REQUIRE(parser);
                    co_await parser.load(result);
                    REQUIRE(result.method == http::method::post);
                    REQUIRE(result.body_access == "hello");
                    AND_THEN("The parsed request should give same header") {
                        REQUIRE(result.build_header() == header);
                    }
                }(),
                [&]() -> task<> {
                    ios.process_events();
                    co_return;
                }()));
            }
        }
    }
}
