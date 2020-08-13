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
    std::optional<cppcoro::http::server> g_server;

    void at_exit(int) {
        if (g_server)
            g_server->stop();
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

    auto th = std::thread([&] {
        ios.process_events();
    });

    (void) sync_wait(when_all(
        [&]() -> task<> {
            auto _ = on_scope_exit([&] {
                ios.stop();
            });
            g_server.emplace(ios, *server_endpoint);
            cppcoro::async_scope scope;
            auto handle_conn = [](http::connection conn) mutable -> task<> {
                fmt::print("connection from: '{}'\n", conn.peer_address().to_string());
                http::router router;
                router.add_route<R"(/hello/(\w+))">().complete([](const std::string& who) -> task <std::tuple<http::status, std::string>> {
                    co_return std::tuple{http::status::HTTP_STATUS_OK, fmt::format("Hello {} !!", who)};
                });
                router.add_route<R"(/add/(\d+)/(\d+))">().complete([](int lhs, int rhs) -> task <std::tuple<http::status, std::string>> {
                    co_return std::tuple{http::status::HTTP_STATUS_OK, fmt::format("{}", lhs + rhs)};
                });
                while (true) {
                    try {
                        auto req = co_await conn.next();
                        if (req == nullptr)
                            break;
                        fmt::print("url : {}\n", req->url);
                        fmt::print("method: {}\n", req->method_str());
                        fmt::print("headers:\n");
                        for (auto &[field, value] : req->headers) {
                            fmt::print(" - {}: {}\n", field, value);
                        }
                        auto resp = co_await router.process(*req);
                        co_await conn.send(resp);

                    } catch (http::router::not_found &) {
                        http::response resp{http::status::HTTP_STATUS_NOT_FOUND};
                        co_await conn.send(resp);
                    } catch (std::system_error &err) {
                        if (err.code() == std::errc::connection_reset) {
                            break;
                        } else {
                            throw err;
                        }
                    }
                }
                std::cout << conn.peer_address().to_string() << " disconnected\n";
            };
            while (true) {
                scope.spawn(handle_conn(co_await g_server->listen()));
            }
            co_return;
        }()
        /*, [&]() -> task<> {
            ios.process_events();
            co_return;
        }()//*/
    ));
    th.join();
    std::cout << "Bye !\n";
    return 0;
}
