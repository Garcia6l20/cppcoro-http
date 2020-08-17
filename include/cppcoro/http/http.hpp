/**
 * @file cppcoro/http.hpp
 */
#pragma once

#include <map>
#include <string>

namespace cppcoro::http {

    namespace detail {
        #include <http_parser.h>
    }

    enum class method
    {
        get = detail::HTTP_GET,
        put = detail::HTTP_PUT,
        del = detail::HTTP_DELETE,
        post = detail::HTTP_POST,
        head = detail::HTTP_HEAD,
        options = detail::HTTP_OPTIONS,
        patch = detail::HTTP_PATCH,
        unknown
    };

    using status = detail::http_status;
    using headers = std::map<std::string, std::string>;
}
