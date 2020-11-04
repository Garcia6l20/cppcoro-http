#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

#include <cppcoro/net/make_socket.hpp>
//#include <cppcoro/ssl/make_socket.hpp>

using namespace cppcoro;

#include "../ssl/cert.hpp"

SCENARIO("optional args should be optional", "[cppcoro-http][net][any_socket]") {
	auto test_value = [](net::kw_args<int> kw_args = {}) {
        kw_args.set_default_values(-1);
		return kw_args.value<int>();
	};
	REQUIRE(test_value() == -1);
    REQUIRE(test_value(42) == 42);
    auto test_value_or = [](net::kw_args<int> kw_args = {}) {
      return kw_args.value_or(-1);
    };
    REQUIRE(test_value_or() == -1);
    REQUIRE(test_value_or(42) == 42);
}

SCENARIO("optional args should discard extra args", "[cppcoro-http][net][any_socket]") {

	REQUIRE(net::kw_args<int>{}.has_value<bool>() == false);

    auto test_value = [](net::kw_args<int> kw_args) {
		kw_args.set_default_values(-1);
      return kw_args.value<int>();
    };
    REQUIRE(test_value(net::kw_args<int, bool>{}) == -1);
    REQUIRE(test_value(net::kw_args<int, bool>{ 42, false }) == 42);
    auto test_value_or = [](net::kw_args<int> kw_args = {}) {
      return kw_args.value_or(-1);
    };
    REQUIRE(test_value_or(net::kw_args<int, bool>{}) == -1);
    REQUIRE(test_value_or(net::kw_args<int, bool>{ 42, false }) == 42);
}

TEST_CASE("base test", "[cppcoro-http][net][any_socket]")
{
	auto endpoint = *net::ip_endpoint::from_string("127.0.0.1:4242");
	io_service service;
	{
		auto socket = net::make_socket(service, endpoint);
	}
	{
		auto socket = net::make_socket<net::ssl::socket>(
			service,
			endpoint, net::kw_args{ net::ssl::certificate{ cert }, net::ssl::private_key{ key } });
	}
//	{
//        auto socket = net::make_socket(net::ssl_client_socket, service, endpoint);
//    }
//    {
//        auto socket = net::make_socket(
//            net::ssl_client_socket,
//            service,
//            endpoint,
//            net::ssl::certificate{ cert },
//            net::ssl::private_key{ key });
//    }
	spdlog::info("Hello !!");
}
