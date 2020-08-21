/**
 * @file cppcoro/http_request.hpp
 */
#pragma once

#include <cppcoro/http/http_message.hpp>

namespace cppcoro::http {
    using string_request = abstract_request<std::string>;
}
