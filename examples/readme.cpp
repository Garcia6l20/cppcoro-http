#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/router.hpp>

#include <fmt/format.h>

using namespace cppcoro;

int main() {
    io_service service;

    auto do_serve = [&]() -> task<> {
        http::server server{service, *net::ip_endpoint::from_string("127.0.0.1:4242")};
        auto handle_conn = [](http::connection conn) mutable -> task<> {
            http::router router;
            // route definition
            auto &route = router.add_route<R"(/hello/(\w+))">();
            // complete handler
            route.complete([](const std::string &who) -> task<std::tuple<http::status, std::string>> {
                co_return std::tuple{
                    http::status::HTTP_STATUS_OK,
                    fmt::format("Hello {} !!", who)
                };
            });
            while (true) {
                try {
                    // wait next connection request
                    auto req = co_await conn.next();
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
                }
            }
        };
        async_scope scope;
        while (true) {
            scope.spawn(handle_conn(co_await server.listen()));
        }
    };
    (void) sync_wait(when_all(do_serve(), [&]() -> task<> {
        service.process_events();
        co_return;
    }()));
    return 0;
}
