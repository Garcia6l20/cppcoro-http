#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <cppcoro/http/http_request.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/is_awaitable.hpp>

using namespace cppcoro;

SCENARIO("requests should be customizable", "[cppcoro-http][messages][request]") {

    GIVEN("A simple string request") {
        http::string_request req{http::method::post, "/", "hello"};
        WHEN("The http header is built") {
            auto header = req.build_header();
            THEN("It can be parsed") {
                http::string_request result;
                http::request_parser parser;
                parser.parse(header);
                REQUIRE(!parser);
                parser.parse(req.body_access);
                REQUIRE(parser);
                parser.load(result);
                REQUIRE(result.method == http::method::post);
                REQUIRE(result.body_access == "hello");
                AND_THEN("The parsed request should give same header") {
                    REQUIRE(result.build_header() == header);
                }
            }
        }
    }
}
