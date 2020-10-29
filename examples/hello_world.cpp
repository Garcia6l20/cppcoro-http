#include <cppcoro/async_scope.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/http/chunk_provider.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/server.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <fmt/printf.h>

#include <csignal>
#include <iostream>
#include <thread>

using namespace cppcoro;
namespace rng = std::ranges;

using config = http::default_session_config<>;

template <typename ConfigT>
struct add_controller;

template <typename ConfigT>
using add_controller_def = http::route_controller<
    R"(/add/(\d+)/(\d+))",  // route definition
    ConfigT,
    http::string_request,
    add_controller>;

template <typename ConfigT>
struct add_controller : add_controller_def<ConfigT>
{
    using add_controller_def<ConfigT>::add_controller_def;

    auto on_get(int lhs, int rhs) -> task <http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                        fmt::format("{}", lhs + rhs)};
    }
};

template <typename ConfigT>
struct hello_controller;

template <typename ConfigT>
using hello_controller_def = http::route_controller<
    R"(/hello/(\w+))",  // route definition
    ConfigT,
    http::string_request,
    hello_controller>;

template <typename ConfigT>
struct hello_controller : hello_controller_def<ConfigT>
{
    using hello_controller_def<ConfigT>::hello_controller_def;

    auto on_get(const std::string &who) -> task <http::string_response> {
        co_return http::string_response{http::status::HTTP_STATUS_OK,
                                        fmt::format("Hello {}", who)};
    }
};

template <typename ConfigT>
struct cat_controller;

template <typename ConfigT>
using cat_controller_def = http::route_controller<
    R"(/cat)",  // route definition
    ConfigT,
    http::string_request,
    cat_controller>;

template <typename ConfigT>
struct cat_controller : cat_controller_def<ConfigT>
{
    using cat_controller_def<ConfigT>::cat_controller_def;

    auto on_get() -> task <http::read_only_file_chunked_response> {
        co_return http::read_only_file_chunked_response{http::status::HTTP_STATUS_OK,
                                                        http::read_only_file_chunk_provider{this->service(), __FILE__}};
    }
};

using hello_server = http::controller_server<config,
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

    http::logging::log_level = spdlog::level::debug;

    using namespace cppcoro;

    std::vector<std::string_view> args{argv + 1, argv + argc};
    auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
    fmt::print("listening at '{}'\n", server_endpoint->to_string());

    std::signal(SIGTERM, at_exit);
    std::signal(SIGINT, at_exit);

    io_service service;

    static const constinit int thread_count = 5;
    std::array<std::thread, thread_count> thread_pool;
    rng::generate(thread_pool, [&] {
        return std::thread([&] {
            service.process_events();
        });
    });

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
        , [&]() -> task<> {
            service.process_events();
            co_return;
        }()
        ));

    rng::for_each(thread_pool, [](auto &&th) {
        th.join();
    });

    std::cout << "Bye !\n";
    return 0;
}
