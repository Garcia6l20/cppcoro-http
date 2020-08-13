/**
 * @file cppcoro/http.hpp
 */
#pragma once

#include <map>
#include <string>

namespace cppcoro::http {

    namespace details {
        #include <http_parser.h>
    }

    enum class method
    {
        get = details::HTTP_GET,
        put = details::HTTP_PUT,
        del = details::HTTP_DELETE,
        post = details::HTTP_POST,
        head = details::HTTP_HEAD,
        options = details::HTTP_OPTIONS,
        patch = details::HTTP_PATCH,
        unknown
    };

    using status = details::http_status;
    using headers = std::map<std::string, std::string>;
}
