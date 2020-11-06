/** @file cppcoro/net/ssl/concepts.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/net/concepts.hpp>

namespace cppcoro::net::ssl
{
    // clang-format off
	template <typename T>
	concept is_socket = cppcoro::net::is_socket<T> and requires (T v)
    {
        { v.encrypt(std::declval<cancellation_token>()) } -> awaitable;
    };
    // clang-format on
}
