#include <cppcoro/http/connection.hpp>
#include <cppcoro/http/router.hpp>
#include <cppcoro/net/serve.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

using namespace cppcoro;

int main()
{
    io_service service{ 128 };
    cancellation_source cancel{};

    router::router router{
        std::make_tuple(),
        http::route::get<R"(/hello/(\w+))">(
            [](const std::string& who,
               router::context<http::server_connection<net::socket>> con) -> task<> {
              co_await net::make_tx_message(
                  *con, http::status::HTTP_STATUS_OK, fmt::format("Hello {} !", who));
            }),
        http::route::get<R"(/add/(\d+)/(\d+))">(
            [](int lhs,
               int rhs,
               router::context<http::server_connection<net::socket>> con) -> task<> {
              co_await net::make_tx_message(
                  *con, http::status::HTTP_STATUS_OK, fmt::format("{}", lhs + rhs));
            }),
        router::on<R"(.*)">(
            [](router::context<http::server_connection<net::socket>> con) -> task<> {
              co_await net::make_tx_message(
                  *con, http::status::HTTP_STATUS_NOT_FOUND, "route not found");
            }),
    };

    auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");

    auto do_serve = [&]() -> task<> {
      auto _ = on_scope_exit([&] { service.stop(); });
      co_await http::router::serve(service, endpoint, std::ref(router), std::ref(cancel));
    };
    (void)sync_wait(when_all(do_serve(), [&]() -> task<> {
      service.process_events();
      co_return;
    }()));
    return 0;
}
