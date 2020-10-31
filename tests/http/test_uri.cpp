#include <catch2/catch.hpp>

#include <cppcoro/net/uri.hpp>

#include <spdlog/spdlog.h>

using namespace cppcoro;

bool conv_test(std::string_view input) {
    spdlog::debug("input: {}", input);
    auto tmp = net::uri::escape(input);
	spdlog::debug("tmp: {}", tmp);
	auto output = net::uri::unescape(tmp);
    spdlog::debug("output: {}", output);
    return output == input;
}

SCENARIO("uri should work", "[cppcoro-http][uri]")
{
	spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
	REQUIRE(conv_test("/this/is/a/path"));
    REQUIRE(conv_test("Téléchargements"));

	auto test = net::uri{"http://localhost:4242/hello/world#p1=1&p2=2"};
    REQUIRE(test.scheme == "http");
    REQUIRE(test.host == "localhost");
    REQUIRE(test.port == "4242");
    REQUIRE(test.path == "/hello/world");
    REQUIRE(test.parameters == "p1=1&p2=2");
}