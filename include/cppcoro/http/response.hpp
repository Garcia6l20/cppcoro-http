/**
 * @file cppcoro/http_response.hpp
 */
#pragma once

#include <cppcoro/http/message.hpp>

namespace cppcoro::http {
    using string_response = abstract_response<std::string>;
}
