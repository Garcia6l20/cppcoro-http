#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_server.hpp>

#include <fmt/format.h>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;

int main() {
    struct session {
        int id = std::rand();
    };

    struct hello_controller : http::route_controller<
        R"(/hello/(\w+))",  // route definition
        session,
        hello_controller>
    {
        // method handlers
        auto on_post(std::string_view who) -> task<http::response> {
            co_return http::response{http::status::HTTP_STATUS_OK,
                                     fmt::format("post at {}: hello {}", session().id, who)};
        }
        auto on_get(std::string_view who) -> task<http::response> {
            co_return http::response{http::status::HTTP_STATUS_OK,
                                     fmt::format("get at {}: hello {}", session().id, who)};
        }
    };

    io_service service;

    auto do_serve = [&]() -> task<> {
        auto _ = on_scope_exit([&] {
            service.stop();
        });
        http::controller_server<session, hello_controller> server{
            service,
            *net::ip_endpoint::from_string("127.0.0.1:4242")};
        co_await server.serve();
    };
    (void) sync_wait(when_all(
        do_serve(),
        [&]() -> task<> {
            service.process_events();
            co_return;
        }()));
    return 0;
}
