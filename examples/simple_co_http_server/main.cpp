#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>


using namespace cppcoro;
namespace fs = std::filesystem;

struct session
{

};

using get_file_controller_def = http::route_controller<
    R"(/?([^\.]+\.\w+))",
    session,
    http::string_request,
    struct get_file_controller>;

struct get_file_controller : get_file_controller_def
{
    using get_file_controller_def::get_file_controller_def;

    task<http::read_only_file_chunked_response> on_get(std::string_view path) {
        fmt::print("-- get file: {}\n", path);
        try {
            auto chrooted_path = (fs::relative(fs::path{path.data(), path.data() + path.size()})).string();
            co_return http::read_only_file_chunked_response{
                http::status::HTTP_STATUS_OK,
                http::read_only_file_chunk_provider{service(),
                                                    chrooted_path}
            };
        } catch (fs::filesystem_error &error) {
            spdlog::error("error {}", error.what());
            if (error.code() == std::errc::no_such_file_or_directory) {
                co_return http::read_only_file_chunked_response{
                    http::status::HTTP_STATUS_NOT_FOUND,
                    http::read_only_file_chunk_provider{service()}
                };
            } else {
                co_return http::read_only_file_chunked_response{
                    http::status::HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    http::read_only_file_chunk_provider{service()}
                };
            }
        }
    }
};

using home_controller_def = http::route_controller<
    R"(/?([^\s]*)?)",
    session,
    http::string_request,
    struct home_controller>;

struct home_controller : home_controller_def
{
    using home_controller_def::home_controller_def;

    fmt::memory_buffer make_body(std::string_view path) {
        auto chrooted_path = (fs::relative(fs::path{path.data(), path.data() + path.size()})).string();
        if (chrooted_path.empty())
            chrooted_path = ".";
        fmt::print("-- get: {} ({})\n", path, chrooted_path);
        fmt::memory_buffer out;
        fmt::format_to(out, "<ul>");
        for (auto &p: fs::directory_iterator(chrooted_path)) {
            fmt::format_to(out, R"(<li><a href="/{full_path}">{path}</a></li>)",
                           fmt::arg("path", fs::relative(p.path(), chrooted_path).c_str()),
                           fmt::arg("full_path", fs::relative(p.path()).c_str()));
        }
        fmt::format_to(out, "</ul>");
        return out;
    }

    task<http::string_response> on_get(std::string_view path) {
        auto status = http::status::HTTP_STATUS_OK;
        fmt::memory_buffer body;
        try {
            body = make_body(path);
        } catch (fs::filesystem_error &error) {
            body = fmt::memory_buffer{};
            fmt::format_to(body, R"(<div><h6>Not found</h6><p>{}</p></div>)", error.what());
            status = http::status::HTTP_STATUS_NOT_FOUND;
        }
        co_return http::string_response{
            status,
            fmt::format(R"(
<!DOCTYPE html>
<head lang="en">
<title>{title}</title>
</head>
<body>
{body}
</body>
)",
                        fmt::arg("title", path),
                        fmt::arg("body", std::string_view{body.data(), body.size()})),
            http::headers{
                {"Content-Type", "text/html"}
            }
        };
    }
};

using simple_co_server = http::controller_server<session,
    get_file_controller,
    home_controller>;

int main(int argc, char **argv) {
    spdlog::set_level(spdlog::level::debug);
    io_service service;
    std::vector<std::string_view> args{argv + 1, argv + argc};
    auto server_endpoint = net::ip_endpoint::from_string(args.empty() ? "127.0.0.1:4242" : args.at(0));
    fmt::print("listening at '{}'\n", server_endpoint->to_string());
    (void) sync_wait(when_all(
        [&]() -> task<> {
            auto _ = on_scope_exit([&] {
                service.stop();
            });
            simple_co_server server{service, *server_endpoint};
            co_await server.serve();
        }(),
        [&]() -> task<> {
            service.process_events();
            co_return;
        }()));
}
