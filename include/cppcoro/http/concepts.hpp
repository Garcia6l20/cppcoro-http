/** @file cppcoro/http/concepts.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <type_traits>
#include <cppcoro/net/concepts.hpp>

namespace cppcoro::http {

	// clang-format: off
	template <typename T>
	concept is_config = requires (T) {
		typename T::socket_provider;
        (net::is_socket_provider<typename T::socket_provider>);
        typename T::session_type;
        typename T::connection_socket_type;
	};

    template<typename T, typename ServerT>
	concept is_server_session = requires(T) {
        {T{std::declval<ServerT&>()}};
	};

    template<typename T>
    concept is_cookies_session = requires(T v) {
        { v.extract_cookies(std::declval<http::headers const&>()) };
        { v.load_cookies(std::declval<http::headers&>()) };
    };
    // clang-format: on
}
