/**
 * @file cppcoro/http_response.hpp
 */
#pragma once

#include <cppcoro/http/http.hpp>

namespace cppcoro::http {
    class response
    {
    public:
        http::status status;
        http::headers headers {};
        std::string body;

        std::string build_header() {
            std::string output = "HTTP/1.1 " + std::to_string(status) + " " + http_status_str(status) + "\r\n";
            auto write_header = [&output](const std::string& field, const std::string& value) {
                output += field + ": " + value + "\r\n";
            };
            headers["Content-Length"] = std::to_string(body.size());
            for (auto &[field, value] : headers) {
                write_header(field, value);
            }
            output += "\r\n";
            return output;
        }
    };
}
