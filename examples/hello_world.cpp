#include <cppcoro/task.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/cancellation_source.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/router.hpp>

#include <fmt/printf.h>

#include <csignal>
#include <iostream>
#include <thread>

namespace {
    std::optional<cppcoro::http::route_server> g_server;

    void at_exit(int) {
        fmt::print("exit requested\n");
        if (g_server) {
            fmt::print("stopping server\n");
            g_server->stop();
            fmt::print("done\n");
        }
    }
}

int main(const int argc, const char **argv) {

    using namespace cppcoro;

    std::vector<std::string_view> args{argv + 1, argv + argc};
    auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
    fmt::print("listening at '{}'\n", server_endpoint->to_string());

    io_service ios;

    std::signal(SIGTERM, at_exit);
    std::signal(SIGINT, at_exit);

    static const constinit int thread_count = 5;
    std::array<std::thread, thread_count> thread_pool;
    std::generate(begin(thread_pool), end(thread_pool), [&] {
        return std::thread([&] {
            ios.process_events();
        });
    });

    (void) sync_wait(when_all(
        [&]() -> task<> {
            auto _ = on_scope_exit([&] {
                ios.stop();
            });
            g_server.emplace(ios, *server_endpoint);
            g_server->add_route<R"(/hello/(\w+))">().on_complete([](const std::string& who) -> task <std::tuple<http::status, std::string>> {
                fmt::print("thread id: {}\n", std::this_thread::get_id());
                co_return std::tuple{http::status::HTTP_STATUS_OK, fmt::format("Hello {} !!", who)};
            });
            g_server->add_route<R"(/add/(\d+)/(\d+))">().on_complete([](int lhs, int rhs) -> task <std::tuple<http::status, std::string>> {
                co_return std::tuple{http::status::HTTP_STATUS_OK, fmt::format("{}", lhs + rhs)};
            });
            co_await g_server->serve();
        }()
    ));

    std::for_each(begin(thread_pool), end(thread_pool), [](auto &&th) {
        th.join();
    });
    std::cout << "Bye !\n";
    return 0;
}
