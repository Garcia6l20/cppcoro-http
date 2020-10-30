/** @file cppcoro/net/ssl/error.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <mbedtls/error.h>

#include <system_error>
#include <string>

namespace cppcoro::net::ssl
{
	struct error_category_t : std::error_category {
        virtual const char* name() const noexcept { return "ssl"; }
        virtual std::string message(int error) const noexcept {
			char error_buf[128];
            mbedtls_strerror(error, error_buf, sizeof(error_buf));
			return error_buf;
		}
	} error_category{};
}
