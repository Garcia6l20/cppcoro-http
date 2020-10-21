/** @file cppcoro/net/ssl/context.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/detail/linux_io_operation.hpp>
#include <mbedtls/ssl.h>

namespace cppcoro::net::ssl
{
	class handshake_operation :
        public detail::io_operation<handshake_operation>
    {
	private:
        handshake_operation(detail::lnx::io_queue& io_queue, mbedtls_ssl_context &ssl_context)
            : detail::io_operation<handshake_operation>{
            io_queue
        }
            , ssl_context_{ssl_context}
        {}
		friend class socket;
		mbedtls_ssl_context &ssl_context_;
	};
}
