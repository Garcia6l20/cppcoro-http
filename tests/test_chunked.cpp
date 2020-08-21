#define CATCH_CONFIG_MAIN

#include <catch2/catch.hpp>

#include <fmt/format.h>

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/http/http_client.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/generator.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/write_only_file.hpp>
#include <cppcoro/http/route_controller.hpp>

using namespace cppcoro;

namespace cppcoro::http {
    struct write_only_file_processor : abstract_chunk_base
    {
        using abstract_chunk_base::abstract_chunk_base;

        write_only_file file_;
        size_t offset = 0;

        write_only_file_processor(io_service &service, std::string_view path) noexcept:
            abstract_chunk_base{service}, file_{write_only_file::open(service, path)} {}

        task <size_t> write(std::string_view chunk) {
            auto size = co_await file_.write(offset, chunk.data(), chunk.size());
            offset += size;
            co_return size;
        }
    };

    static_assert(std::constructible_from<write_only_file_processor, io_service&>);
    static_assert(detail::wo_chunked_body<write_only_file_processor>);

    using write_only_file_chunked_response = abstract_response<write_only_file_processor>;
    using write_only_file_chunked_request = abstract_request<write_only_file_processor>;
}

SCENARIO("chunked transfers should work", "[cppcoro-http][server][chunked]") {
    io_service ios;

    GIVEN("An chunk server") {

        struct session
        {
        };

        using test_reader_controller_def = http::route_controller<
            R"(/read)",  // route definition
            session,
            http::string_request,
            struct test_reader_controller>;

        struct test_reader_controller : test_reader_controller_def
        {
            using test_reader_controller_def::test_reader_controller_def;

            auto on_get() -> task <http::read_only_file_chunked_response> {
                co_return http::read_only_file_chunked_response{http::status::HTTP_STATUS_OK,
                                                                http::read_only_file_chunk_provider{service(), __FILE__}};
            }
        };

        using test_writer_controller_def = http::route_controller<
            R"(/writer)",  // route definition
            session,
            http::write_only_file_chunked_request,
            struct test_writer_controller>;

        struct test_writer_controller : test_writer_controller_def {
            using test_writer_controller_def::test_writer_controller_def;

            auto on_post() -> task<http::string_response> {
                co_return http::string_response{
                    http::status::HTTP_STATUS_OK,
                    "success"
                };
            }
        };

        using chunk_server = http::controller_server<session,
            test_reader_controller,
            test_writer_controller>;
        chunk_server server{ios, *net::ip_endpoint::from_string("127.0.0.1:4242")};

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
                    auto _ = on_scope_exit([&] {
                        server.stop();
                    });
                    auto conn = co_await client.connect(*net::ip_endpoint::from_string("127.0.0.1:4242"));
                    auto f = read_only_file::open(ios, __FILE__);
                    std::string content;
                    content.resize(f.size());
                    auto[body, f_size] = co_await when_all(
                        [&]() -> task<std::string> {
                            auto response = co_await conn.get();
                            co_return std::string{co_await response->read_body()};
                        }(),
                        f.read(0, content.data(), content.size()));
                    REQUIRE(body == content);
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
