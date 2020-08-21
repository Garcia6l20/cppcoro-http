#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;

struct chunker
{
    std::reference_wrapper<io_service> service;
    int counter = 0;

    chunker(io_service &service) noexcept : service{service} {}

    chunker(chunker &&other) noexcept : service{other.service}, counter{other.counter} {}
    chunker &operator=(chunker &&other) noexcept {
        service = other.service;
        counter = other.counter;
        return *this;
    }

    ~chunker() {
        service.get().stop();
    }

    task<std::string_view> read(size_t sz) {
        fmt::print("chunk {}\n", counter);
        if (counter++ > 3) {
            co_return std::string_view{};
        }
        co_return std::string_view{"hello"};
    }
};

struct session
{
};
static_assert(http::detail::ro_chunked_body<chunker>);

using chunk_response = http::detail::abstract_response<chunker>;

template<typename ControllerT>
using chunk_controller_def = http::route_controller<
    R"(/)",  // route definition
    session,
    ControllerT>;

struct chunk_controller : chunk_controller_def<chunk_controller>
{
    explicit chunk_controller(io_service &service) noexcept: chunk_controller_def<chunk_controller>{service} {}

    auto on_get() -> task<chunk_response> {
        fmt::print("get ...\n");
        co_return chunk_response{http::status::HTTP_STATUS_OK,
                                 chunker{service()}};
    }
};

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {

    http::detail::abstract_response<std::string> response{http::status::HTTP_STATUS_OK};


    io_service ios;

    using chunk_server = http::controller_server<session, chunk_controller>;
    chunk_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};
    GIVEN("An chunk server") {
        WHEN("...") {
            http::client client{ios};
            sync_wait(when_all(
                [&]() -> task<> {
                    auto _ = on_scope_exit([&] {
                        ios.stop();
                    });
                    co_await server.serve();
                }(),
                [&]() -> task<> {
//                    auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
//                    auto response = co_await conn.get();
//                    auto body = co_await response->read_body();
//                    REQUIRE(body == "hellohellohello");
//                    server.stop();
                    co_return;
                }(),
                [&]() -> task<> {
                    ios.process_events();
                    co_return;
                }())
            );
        }
    }
}
