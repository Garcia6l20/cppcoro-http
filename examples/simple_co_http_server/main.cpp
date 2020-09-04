#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/http_chunk_provider.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/on_scope_exit.hpp>

#include <lyra/cli_parser.hpp>
#include <lyra/opt.hpp>
#include <lyra/arg.hpp>

#include <thread>
#include <ranges>

using namespace cppcoro;
namespace fs = std::filesystem;
namespace rng = std::ranges;

struct session
{

};

using home_controller_def = http::route_controller<
    R"(/?([^\s]*)?)",
    session,
    http::string_request,
    struct home_controller>;

struct home_controller : home_controller_def
{
    using home_controller_def::home_controller_def;

    static constexpr std::string_view template_ = R"(
<!DOCTYPE html>
<html lang="en">
<head lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/css/bootstrap.min.css" integrity="sha384-r4NyP46KrjDleawBgD5tp8Y7UzmLA05oM1iAEQ17CSuDqnUK2+k9luXQOfXJCJ4I" crossorigin="anonymous">
<title>{title}</title>
</head>
<body class="bg-light">
    <div class="container">
        <div class="py-5 text-center">
            <h2>Simple CppCoro HTTP Server</h2>
            <p class="lead">{path}</p>
        </div>
        <nav aria-label="breadcrumb">
        {breadcrumb}
        </nav>
        {body}
    </div>
    <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js" integrity="sha384-Q6E9RHvbIyZFJoft+2mJbHaEWldlvI9IOYy5n3zV9zzTtmI3UksdQRVvoxMfooAo" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/js/bootstrap.min.js" integrity="sha384-oesi62hOLfzrys4LxRF63OJCXdXDipiYWBnvTl9Y9/TRlw5xlKIEHpNyvvDShgf/" crossorigin="anonymous"></script>
</body>
</html>
)";

    fmt::memory_buffer make_body(std::string_view path) {
        auto chrooted_path = (fs::relative(fs::path{path.data(), path.data() + path.size()})).string();
        if (chrooted_path.empty())
            chrooted_path = ".";
        fmt::print("-- get: {} ({})\n", path, chrooted_path);
        fmt::memory_buffer out;
        fmt::format_to(out, R"(<div class="list-group">)");
        for (auto &p: fs::directory_iterator(chrooted_path)) {
            fmt::format_to(out, R"(<a class="list-group-item-action" href="/{full_path}">{path}</a>)",
                           fmt::arg("path", fs::relative(p.path(), chrooted_path).c_str()),
                           fmt::arg("full_path", fs::relative(p.path()).c_str()));
        }
        fmt::format_to(out, "</div>");
        return out;
    }

    fmt::memory_buffer make_breadcrumb(std::string_view path) {
        fmt::memory_buffer buff;
        constexpr std::string_view init = R"(<nav aria-label="breadcrumb"><ol class="breadcrumb">)"
                                          R"(<li class="breadcrumb-item"><a href="/">Home</a></li>)";
        buff.append(std::begin(init), std::end(init));
        fs::path p = path;
        std::vector<std::string> elems = {
            {fmt::format(R"(<li class="breadcrumb-item active" aria-current="page"><a href="/{}">{}</a></li>)", p.string(), p.filename().c_str())}
        };
        p = p.parent_path();
        while (p.has_parent_path()) {
            elems.emplace_back(fmt::format(R"(<li class="breadcrumb-item"><a href="/{}">{}</a></li>)", p.string(), p.filename().c_str()));
            p = p.parent_path();
        }
        for (auto &elem : elems | std::views::reverse) {
            fmt::format_to(buff, "{}", elem);
        }
        return buff;
    }

    using response_type =
    std::variant<http::string_response, http::read_only_file_chunked_response>;
    static_assert(http::detail::is_visitable<response_type>);

    task<response_type> on_get(std::string_view path) {
        auto status = http::status::HTTP_STATUS_OK;
        auto chrooted_path = (fs::relative(fs::path{path.data(), path.data() + path.size()})).string();
        if (chrooted_path.empty())
            chrooted_path = ".";
        spdlog::info("chrooted_path: {}\n", chrooted_path);
        if (fs::is_directory(chrooted_path)) {
            spdlog::info("get directory: {}\n", path);
            fmt::memory_buffer body;
            try {
                body = make_body(path);
            } catch (fs::filesystem_error &error) {
                body = fmt::memory_buffer{};
                fmt::format_to(body, R"(<div><h6>Not found</h6><p>{}</p></div>)", error.what());
                status = http::status::HTTP_STATUS_NOT_FOUND;
            }
            auto breadcrumb = make_breadcrumb(path);
            co_return http::string_response{
                status,
                fmt::format(template_,
                            fmt::arg("title", path),
                            fmt::arg("body", std::string_view{body.data(), body.size()}),
                            fmt::arg("path", path),
                            fmt::arg("breadcrumb", std::string_view{breadcrumb.data(), breadcrumb.size()})),
                http::headers{
                    {"Content-Type", "text/html"}
                }
            };
        } else {
            spdlog::info("get file: {}\n", path);
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
                    co_return http::string_response{
                        http::status::HTTP_STATUS_NOT_FOUND,
                        {error.what()}
                    };
                } else {
                    co_return http::string_response{
                        http::status::HTTP_STATUS_INTERNAL_SERVER_ERROR,
                        {error.what()}
                    };
                }
            }
        }
    }
};

using simple_co_server = http::controller_server<session,
    home_controller>;


int main(int argc, char **argv) {
    bool debug = false;
    std::string endpoint_input = "127.0.0.1:4242";
    uint32_t thread_count = 1;
    auto cli
        = lyra::opt(debug)
          ["-d"]["--debug"]
              ("Enable debug output")
          | lyra::opt(thread_count, "thread_count")
          ["-t"]["--threads"]
              ("Thread count")
          | lyra::arg(endpoint_input, "endpoint")
              ("Server endpoint");
    auto result = cli.parse({argc, argv});
    if (!result) {
        std::cerr << "Error in command line: " << result.errorMessage() << std::endl;
        exit(1);
    }

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
        http::logging::log_level = spdlog::level::debug;
    }

    io_service service;
    auto server_endpoint = net::ip_endpoint::from_string(endpoint_input);

    // TODO - fix multithreading
    std::clamp(thread_count, 1u, 256u);

    spdlog::info("listening at '{}' on {} threads\n", server_endpoint->to_string(), thread_count + 1);

    std::vector<std::thread> tp{thread_count};

    rng::generate(tp, [&service] {
        return std::thread{[&service]() mutable {
            service.process_events();
        }};
    });
    (void) sync_wait(when_all([&]() -> task<> {
        auto _ = on_scope_exit([&] {
            service.stop();
        });
        simple_co_server server{service, *server_endpoint};
        co_await server.serve();
    }(), [&]() -> task<> {
        service.process_events();
        co_return;
    }()));

    rng::for_each(tp, [](auto &&thread) {
        thread.join();
    });
}
