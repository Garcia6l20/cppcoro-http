//#define CPPCORO_SSL_DEBUG

#include <cppcoro/http/connection.hpp>
#include <cppcoro/http/router.hpp>
#include <cppcoro/on_scope_exit.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/sync_wait.hpp>
#include <cppcoro/when_all.hpp>

#include <lyra/arg.hpp>
#include <lyra/cli_parser.hpp>
#include <lyra/help.hpp>
#include <lyra/opt.hpp>

#include <cppcoro/net/socket.hpp>
#include <ranges>
#include <thread>

#include <csignal>
#include <iostream>

using namespace cppcoro;
namespace fs = cppcoro::filesystem;
namespace rng = std::ranges;

static constexpr std::string_view list_dir_template_ = R"(
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
        <nav aria-label="breadcrumb"><ol class="breadcrumb">{breadcrumb}</ol></nav>
        {body}
    </div>
    <script src="https://cdn.jsdelivr.net/npm/popper.js@1.16.0/dist/umd/popper.min.js" integrity="sha384-Q6E9RHvbIyZFJoft+2mJbHaEWldlvI9IOYy5n3zV9zzTtmI3UksdQRVvoxMfooAo" crossorigin="anonymous"></script>
    <script src="https://stackpath.bootstrapcdn.com/bootstrap/5.0.0-alpha1/js/bootstrap.min.js" integrity="sha384-oesi62hOLfzrys4LxRF63OJCXdXDipiYWBnvTl9Y9/TRlw5xlKIEHpNyvvDShgf/" crossorigin="anonymous"></script>
</body>
</html>
)";

fmt::memory_buffer make_body(const fs::path& root, std::string_view path)
{
	auto link_path = fs::relative(path, root).string();
	if (link_path.empty())
		link_path = ".";
	fmt::print("-- get: {} ({})\n", path, link_path);
	fmt::memory_buffer out;
	fmt::format_to(out, R"(<div class="list-group">)");
	for (auto& p : fs::directory_iterator(root / path))
	{
		fmt::format_to(
			out,
			R"(<a class="list-group-item-action" href="/{link_path}">{path}</a>)",
			fmt::arg("path", fs::relative(p.path(), p.path().parent_path()).c_str()),
			fmt::arg("link_path", fs::relative(p.path(), root).c_str()));
	}
	fmt::format_to(out, "</div>");
	return out;
}

fmt::memory_buffer make_breadcrumb(std::string_view path)
{
	fmt::memory_buffer buff;
	constexpr std::string_view init = R"(<li class="breadcrumb-item"><a href="/">Home</a></li>)";
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

namespace
{
    cancellation_source g_cancellation_source{};
}

void at_exit(int)
{
    fmt::print("exit requested\n");
    g_cancellation_source.request_cancellation();
}

int main(int argc, char** argv)
{
	bool debug = false;
	std::string endpoint_input = "127.0.0.1:4242";
	std::string root = ".";
	bool with_ssl = false;
	uint32_t thread_count = std::thread::hardware_concurrency() - 1;
	bool help = false;
	auto cli = lyra::help(help) | lyra::opt(debug)["-d"]["--debug"]("Enable debug output") |
		lyra::opt(thread_count, "thread_count")["-t"]["--threads"]("Thread count") |
#ifdef CPPCORO_HTTP_MBEDTLS
		lyra::opt(with_ssl)["-s"]["--ssl"]("Use ssl connection") |
#endif
		lyra::arg(endpoint_input, "endpoint")("Server endpoint") | lyra::arg(root, "path")("Path");
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

    std::signal(SIGTERM, at_exit);
    std::signal(SIGINT, at_exit);

	if (debug)
	{
		spdlog::set_level(spdlog::level::debug);
		//		http::logging::log_level = spdlog::level::debug;
	}

	io_service service{ 256 };
	auto server_endpoint = net::ip_endpoint::from_string(endpoint_input);

	std::clamp(thread_count, 1u, 256u);

	std::string scheme = "http";
#ifdef CPPCORO_HTTP_MBEDTLS
	if (with_ssl)
	{
		scheme = "https";
	}
#endif

	spdlog::info(
		"servicing {} at {}://{} on {} threads",
		root,
		scheme,
		server_endpoint->to_string(),
		thread_count + 1);

	std::vector<std::thread> tp{ thread_count };

	rng::generate(tp, [&service] {
		return std::thread{ [&service]() mutable { service.process_events(); } };
	});

	fs::path root_path = root;

	router::router router{
		std::make_tuple(),
		http::route::get<R"(/(.*))">(
			[&](std::string_view path,
				router::context<http::server_connection<net::socket>> con) -> task<> {
				if (fs::is_directory(root_path / path))
				{
					fmt::memory_buffer body;
					try
					{
						body = make_body(root, path);
					}
					catch (fs::filesystem_error& error)
					{
						co_await net::make_tx_message(
							*con,
							http::status::HTTP_STATUS_NOT_FOUND,
							fmt::format(R"(<div><h6>Not found</h6><p>{}</p></div>)", error.what()));
					}
					auto breadcrumb = make_breadcrumb(path);
					http::headers hdrs{ { "Content-Type", "text/html" } };
					co_await net::make_tx_message(
						*con,
						http::status::HTTP_STATUS_OK,
						fmt::format(
							list_dir_template_,
							fmt::arg("title", path),
							fmt::arg("body", std::string_view{ body.data(), body.size() }),
							fmt::arg("path", path),
							fmt::arg(
								"breadcrumb",
								std::string_view{ breadcrumb.data(), breadcrumb.size() })),
						std::move(hdrs));
				}
				else
				{
					try
					{
						auto file = read_only_file::open(
							service,
							root_path / path,
							file_share_mode::read,
							file_buffering_mode::default_);
						net::byte_buffer<256> buffer{};
						http::headers hdrs{ { "Transfer-Encoding", "chunked" } };
						auto tx = co_await net::make_tx_message(
							*con, http::status::HTTP_STATUS_OK, std::move(hdrs));

						size_t read_size;
						uint64_t offset = 0;
						do
						{
							read_size = co_await file.read(offset, buffer.data(), buffer.size());
							offset += read_size;
							co_await tx.send(std::span{ buffer.data(), read_size });
						} while (read_size);
					}
					catch (std::system_error& error)
					{
						co_await net::make_tx_message(
							*con,
							http::status::HTTP_STATUS_NOT_FOUND,
							fmt::format(R"(<div><h6>Not found</h6><p>{}</p></div>)", error.what()));
					}
				}
			}),
	};

	auto do_serve = [&]() -> task<> {
		auto _ = on_scope_exit([&] { service.stop(); });
		co_await http::router::serve(service, *server_endpoint, std::ref(router), std::ref(g_cancellation_source));
	};

	(void)sync_wait(when_all(
		[&]() -> task<> {
			auto _ = on_scope_exit([&] { service.stop(); });
			co_await do_serve();
		}(),
		[&]() -> task<> {
			service.process_events();
			co_return;
		}()));

	rng::for_each(tp, [](auto&& thread) { thread.join(); });
}
