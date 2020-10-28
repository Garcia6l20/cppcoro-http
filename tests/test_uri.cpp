#include <catch2/catch.hpp>

#include <cppcoro/http/url.hpp>

using namespace cppcoro;

SCENARIO("uri should work", "[cppcoro-http][uri]")
{
	constexpr std::string_view input = "/this/is/a/path";
	auto result = http::uri::escape(input);
	REQUIRE(result == "%2Fthis%2Fis%2Fa%2Fpath");
    REQUIRE(http::uri::unescape(result) == input);
}