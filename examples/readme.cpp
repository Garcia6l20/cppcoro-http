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
        http::route_server server{service, *net::ip_endpoint::from_string("127.0.0.1:4242")};
        // route definition
        auto &route = server.add_route<R"(/hello/(\w+))">();
        // complete handler
        route.on_complete([](const std::string &who) -> task<std::tuple<http::status, std::string>> {
            co_return std::tuple{
                http::status::HTTP_STATUS_OK,
                fmt::format("Hello {} !!", who)
            };
        });
        co_await server.serve();
    };
    (void) sync_wait(when_all(do_serve(), [&]() -> task<> {
        service.process_events();
        co_return;
    }()));
    return 0;
}
