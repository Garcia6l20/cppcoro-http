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

//SCENARIO("echo server should work", "[cppcoro-http][server][echo]") {
//    io_service ios;
//    struct session {};
//    struct echo_controller : http::route_controller<
//        R"(/echo/(\w+))",  // route definition
//        session,
//        echo_controller>
//    {
//        auto on_get(const std::string &what) -> task<http::response> {
//            co_return http::response{http::status::HTTP_STATUS_OK,
//                                     fmt::format("{}", what)};
//        }
//    };
//    using echo_server = http::controller_server<session, echo_controller>;
//    echo_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};
//    GIVEN("An echo server") {
//        WHEN("...") {
//            http::client client{ios};
//            sync_wait(when_all(
//            [&]() -> task<> {
//                auto _ = on_scope_exit([&] {
//                    ios.stop();
//                });
//                co_await server.serve();
//            } (),
//            [&]() -> task<> {
//                auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
//                auto response = co_await conn.get("/echo/hello", "");
//                REQUIRE(response->body == "hello");
//                server.stop();
//            }(),
//            [&]() -> task<> {
//                ios.process_events();
//                co_return;
//            }()));
//        }
//    }
//}

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::details::abstract_response<std::string> response;

    io_service ios;
    struct session {};
    struct chunk_controller : http::route_controller<
        R"(/)",  // route definition
        session,
        chunk_controller>
    {
        auto on_get() -> task<http::response> {
            co_return http::response{http::status::HTTP_STATUS_OK,
                                     fmt::format("hello")};
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
                    auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
                    auto response = co_await conn.get();
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
