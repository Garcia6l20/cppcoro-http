#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>

#include <fmt/printf.h>

#include <csignal>
#include <iostream>
#include <thread>

using namespace cppcoro;

struct session
{
    int id = std::rand();
};

using add_controller_def = http::route_controller<
    R"(/add/(\d+)/(\d+))",  // route definition
    session,
    http::string_request,
    struct add_controller>;

struct add_controller : add_controller_def
{
    using add_controller_def::add_controller_def;

    auto on_get(int lhs, int rhs) -> task <http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                        fmt::format("{}", lhs + rhs)};
    }
};

using hello_controller_def = http::route_controller<
    R"(/hello/(\w+))",  // route definition
    session,
    http::string_request,
    struct hello_controller>;

struct hello_controller : hello_controller_def
{
    using hello_controller_def::hello_controller_def;

    auto on_get(const std::string &who) -> task <http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                        fmt::format("Hello {}", who)};
    }
};

using cat_controller_def = http::route_controller<
    R"(/cat)",  // route definition
    session,
    http::string_request,
    struct cat_controller>;

struct cat_controller : cat_controller_def
{
    using cat_controller_def::cat_controller_def;

    auto on_get() -> task <http::read_only_file_chunked_response> {
        co_return http::read_only_file_chunked_response{http::status::HTTP_STATUS_OK,
                                                        http::read_only_file_chunk_provider{service(), __FILE__}};
    }
};

using hello_server = http::controller_server<session,
    hello_controller,
    add_controller,
    cat_controller>;

std::optional<hello_server> g_server;

void at_exit(int) {
    fmt::print("exit requested\n");
    if (g_server) {
        fmt::print("stopping server\n");
        g_server->stop();
        fmt::print("done\n");
    }
}

int main(const int argc, const char **argv) {

    using namespace cppcoro;

    std::vector<std::string_view> args{argv + 1, argv + argc};
    auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
    fmt::print("listening at '{}'\n", server_endpoint->to_string());

    std::signal(SIGTERM, at_exit);
    std::signal(SIGINT, at_exit);

    io_service service;

#define SINGLE_THREAD
#ifndef SINGLE_THREAD
    static const constinit int thread_count = 5;
    std::array<std::thread, thread_count> thread_pool;
    std::generate(begin(thread_pool), end(thread_pool), [&] {
        return std::thread([&] {
            service.process_events();
        });
    });
#endif

    auto do_serve = [&]() -> task<> {
        auto _ = on_scope_exit([&] {
            service.stop();
        });
        g_server.emplace(
            service,
            *server_endpoint);
        co_await g_server->serve();
    };

    (void) sync_wait(when_all(
        do_serve()
//#ifdef SINGLE_THREAD
        , [&]() -> task<> {
            service.process_events();
            co_return;
        }()
//#endif
        ));

#ifndef SINGLE_THREAD
    std::for_each(begin(thread_pool), end(thread_pool), [](auto &&th) {
        th.join();
    });
#endif

    std::cout << "Bye !\n";
    return 0;
}
