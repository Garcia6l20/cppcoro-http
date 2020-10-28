//#define CPPCORO_SSL_DEBUG

#include <cppcoro/http/http_chunk_provider.hpp>
#include <cppcoro/http/route_controller.hpp>
#include <cppcoro/http/session.hpp>
#include <cppcoro/http/config.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <lyra/arg.hpp>
#include <lyra/cli_parser.hpp>
#include <lyra/help.hpp>
#include <lyra/opt.hpp>

#include <cppcoro/net/ssl/socket.hpp>
#include <ranges>
#include <thread>

using namespace cppcoro;
namespace fs = std::filesystem;
namespace rng = std::ranges;

using http_config = http::config<>;
#ifdef CPPCORO_HTTP_MBEDTLS
#include "cert.hpp"
using https_config = http::config<http::session, ipv4_ssl_server_provider>;
#endif

template<typename>
struct home_controller;

template<typename ConfigT>
using home_controller_def =
	http::route_controller<R"(/?([^\s]*)?)", ConfigT, http::string_request, home_controller>;

template<typename ConfigT>
struct home_controller : home_controller_def<ConfigT>
{
	fs::path path_;

	home_controller(io_service& service, std::string_view path)
		: home_controller_def<ConfigT>{ service }
		, path_{ path }
	{
	}

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
        <ol class="breadcrumb">
        {breadcrumb}
        </ol>
        </nav>
        {body}
    </div>
    <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js" integrity="sha384-Q6E9RHvbIyZFJoft+2mJbHaEWldlvI9IOYy5n3zV9zzTtmI3UksdQRVvoxMfooAo" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/js/bootstrap.min.js" integrity="sha384-oesi62hOLfzrys4LxRF63OJCXdXDipiYWBnvTl9Y9/TRlw5xlKIEHpNyvvDShgf/" crossorigin="anonymous"></script>
</body>
</html>
)";

	fmt::memory_buffer make_body(std::string_view path)
	{
		auto link_path = fs::relative(path, path_).string();
		if (link_path.empty())
			link_path = ".";
		fmt::print("-- get: {} ({})\n", path, link_path);
		fmt::memory_buffer out;
		fmt::format_to(out, R"(<div class="list-group">)");
		for (auto& p : fs::directory_iterator(path_ / path))
		{
			fmt::format_to(
				out,
				R"(<a class="list-group-item-action" href="/{link_path}">{path}</a>)",
				fmt::arg("path", fs::relative(p.path(), p.path().parent_path()).c_str()),
				fmt::arg("link_path", fs::relative(p.path(), path_).c_str()));
		}
		fmt::format_to(out, "</div>");
		return out;
	}

	fmt::memory_buffer make_breadcrumb(std::string_view path)
	{
		fmt::memory_buffer buff;
		constexpr std::string_view init =
			R"(<li class="breadcrumb-item"><a href="/">Home</a></li>)";
		buff.append(std::begin(init), std::end(init));
		fs::path p = path;
		std::vector<std::string> elems = { { fmt::format(
			R"(<li class="breadcrumb-item active" aria-current="page"><a href="/{}">{}</a></li>)",
			p.string(),
			p.filename().c_str()) } };
		p = p.parent_path();
		while (not p.filename().empty())
		{
			elems.emplace_back(fmt::format(
				R"(<li class="breadcrumb-item"><a href="/{}">{}</a></li>)",
				p.string(),
				p.filename().c_str()));
			p = p.parent_path();
		}
		for (auto& elem : elems | std::views::reverse)
		{
			fmt::format_to(buff, "{}", elem);
		}
		return buff;
	}

	using response_type =
		std::variant<http::string_response, http::read_only_file_chunked_response>;
	static_assert(http::detail::is_visitable<response_type>);

	task<response_type> on_get(std::string_view path)
	{
		auto hello = this->session().template cookie<std::string>("Hello", "world");
		spdlog::info("hello cookie: {}", hello);
		auto status = http::status::HTTP_STATUS_OK;
		spdlog::info("link_path: {}", path);
		if (fs::is_directory(path_ / path))
		{
			spdlog::info("get directory: {}", path);
			fmt::memory_buffer body;
			try
			{
				body = make_body(path);
			}
			catch (fs::filesystem_error& error)
			{
				body = fmt::memory_buffer{};
				fmt::format_to(body, R"(<div><h6>Not found</h6><p>{}</p></div>)", error.what());
				status = http::status::HTTP_STATUS_NOT_FOUND;
			}
			auto breadcrumb = make_breadcrumb(path);
			co_return http::string_response{
				status,
				fmt::format(
					template_,
					fmt::arg("title", path),
					fmt::arg("body", std::string_view{ body.data(), body.size() }),
					fmt::arg("path", path),
					fmt::arg(
						"breadcrumb", std::string_view{ breadcrumb.data(), breadcrumb.size() })),
				http::headers{ { "Content-Type", "text/html" } }
			};
		}
		else
		{
			spdlog::info("get file: {}", path);
			try
			{
				co_return http::read_only_file_chunked_response{
					http::status::HTTP_STATUS_OK,
					http::read_only_file_chunk_provider{ this->service(), path_ / path }
				};
			}
			catch (fs::filesystem_error& error)
			{
				spdlog::error("error {}", error.what());
				if (error.code() == std::errc::no_such_file_or_directory)
				{
					co_return http::string_response{ http::status::HTTP_STATUS_NOT_FOUND,
													 { error.what() } };
				}
				else
				{
					co_return http::string_response{
						http::status::HTTP_STATUS_INTERNAL_SERVER_ERROR, { error.what() }
					};
				}
			}
		}
	}
};

template<typename ConfigT = http_config>
using simple_co_server = http::controller_server<ConfigT, home_controller>;

int main(int argc, char** argv)
{
	bool debug = false;
	std::string endpoint_input = "127.0.0.1:4242";
	std::string path = ".";
	bool with_ssl = false;
	uint32_t thread_count = std::thread::hardware_concurrency() - 1;
	bool help = false;
	auto cli = lyra::help(help) | lyra::opt(debug)["-d"]["--debug"]("Enable debug output") |
		lyra::opt(thread_count, "thread_count")["-t"]["--threads"]("Thread count") |
#ifdef CPPCORO_HTTP_MBEDTLS
		lyra::opt(with_ssl)["-s"]["--ssl"]("Use ssl connection") |
#endif
		lyra::arg(endpoint_input, "endpoint")("Server endpoint") | lyra::arg(path, "path")("Path");
	auto result = cli.parse({ argc, argv });
	if (!result)
	{
		std::cerr << "Error in command line: " << result.errorMessage() << std::endl;
		exit(1);
	}

	if (help)
	{
		std::cout << cli << '\n';
		return 1;
	}

	if (debug)
	{
		spdlog::set_level(spdlog::level::debug);
		http::logging::log_level = spdlog::level::debug;
	}

	io_service service{ 256 };
	auto server_endpoint = net::ip_endpoint::from_string(endpoint_input);

	std::clamp(thread_count, 1u, 256u);

	std::string scheme = "http";
#ifdef CPPCORO_HTTP_MBEDTLS
	if (with_ssl) {
        scheme = "https";
	}
#endif

	spdlog::info(
		"servicing {} at {}://{} on {} threads", path, scheme, server_endpoint->to_string(), thread_count + 1);

	std::vector<std::thread> tp{ thread_count };

	rng::generate(tp, [&service] {
		return std::thread{ [&service]() mutable { service.process_events(); } };
	});
	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { service.stop(); });
#ifdef CPPCORO_HTTP_MBEDTLS
			if (with_ssl)
			{
				simple_co_server<https_config> server{ service,
													   *server_endpoint,
													   std::string_view{ path } };
				co_await server.serve();
			}
			else
			{
#endif
				simple_co_server server{ service, *server_endpoint, std::string_view{ path } };
				co_await server.serve();
#ifdef CPPCORO_HTTP_MBEDTLS
			}
#endif
		}(),
		[&]() -> task<> {
			service.process_events();
			co_return;
		}()));

	rng::for_each(tp, [](auto&& thread) { thread.join(); });
}
