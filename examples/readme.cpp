#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_server.hpp>

#include <fmt/format.h>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>

using namespace cppcoro;

int main() {
    struct session {
        int id = std::rand();
    };

    using hello_controller_def = http::route_controller<
        R"(/hello/(\w+))",  // route definition
        session,
        http::string_request,
        struct hello_controller>;

    struct hello_controller : hello_controller_def
    {
        using hello_controller_def::hello_controller_def;
        // method handlers
        auto on_post(std::string_view who) -> task<http::string_response> {
            co_return http::string_response{http::status::HTTP_STATUS_OK,
                                     fmt::format("post at {}: hello {}", session().id, who)};
        }
        auto on_get(std::string_view who) -> task<http::string_response> {
            co_return http::string_response{http::status::HTTP_STATUS_OK,
                                     fmt::format("get at {}: hello {}", session().id, who)};
        }
    };

    struct hello_chunk_provider : http::abstract_chunk_base {
        std::string_view who;
        using http::abstract_chunk_base::abstract_chunk_base;
        hello_chunk_provider(io_service& service, std::string_view who) : http::abstract_chunk_base{service}, who{who} {}
        async_generator<std::string_view> read(size_t) {
            co_yield "hello\n";
            co_yield fmt::format("{}\n", who);
        }
    };
    using hello_chunked_response = http::abstract_response<hello_chunk_provider>;

    using hello_chunk_controller_def = http::route_controller<
        R"(/chunk/(\w+))",  // route definition
        session,
        http::string_request,
        struct hello_chunk_controller>;

    struct hello_chunk_controller : hello_chunk_controller_def
    {
        using hello_chunk_controller_def::hello_chunk_controller_def;
        // method handlers
        task<hello_chunked_response> on_get(std::string_view who) {
            co_return hello_chunked_response{http::status::HTTP_STATUS_OK,
                                             hello_chunk_provider {service(), who}};
        }
    };

    io_service service;

    auto do_serve = [&]() -> task<> {
        auto _ = on_scope_exit([&] {
            service.stop();
        });
        http::controller_server<
            session,
            hello_controller,
            hello_chunk_controller> server {
            service,
            *net::ip_endpoint::from_string("127.0.0.1:4242")
        };
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
