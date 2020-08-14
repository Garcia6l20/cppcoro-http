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

auto do_serve = [](io_service &service, http::server& server) -> task<> {
    auto _ = on_scope_exit([&service] {
       service.stop();
    });
    auto handle_conn = [](http::server::connection_type conn) mutable -> task<> {
        http::router router;
        // route definition
        auto &route = router.add_route<R"(/echo/(\w+))">();
        // complete handler
        route.on_complete([](const std::string &input) -> task<std::tuple<http::status, std::string>> {
            co_return std::tuple{
                http::status::HTTP_STATUS_OK,
                input
            };
        });
        while (true) {
            try {
                // wait next connection request
                auto *req = co_await conn.next();
                if (req == nullptr)
                    break; // connection closed
                // wait next router response
                auto resp = co_await router.process(*req);
                co_await conn.send(resp);
            } catch (std::system_error &err) {
                if (err.code() == std::errc::connection_reset) {
                    break; // connection reset by peer
                } else {
                    throw err;
                }
            } catch (operation_cancelled &) {
                break;
            }
        }
    };
    async_scope scope;
    try {
        while (true) {
            scope.spawn(handle_conn(co_await server.listen()));
        }
    } catch (operation_cancelled &) {}
    co_await scope.join();
};

SCENARIO("echo server should work", "[cppcoro-net][server]") {
    io_service ios;
    GIVEN("An echo server") {
        http::server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};
        WHEN("...") {
            http::client client{ios};
            sync_wait(when_all(do_serve(ios, server),
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
