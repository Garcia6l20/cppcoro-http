/**
 * @file cppcoro/http_response.hpp
 */
#pragma once

#include <cppcoro/http/details/static_parser_handler.hpp>

namespace cppcoro::http {
    class response : public detail::static_parser_handler<response>
    {
    public:
        http::status status;
        std::string url;
        http::headers headers {};
        std::string body;

        response() {
            init_parser(details::http_parser_type::HTTP_RESPONSE);
        }
        response(http::status status, std::string &&body, http::headers &&headers = {})
            : status{status}
            , body{std::forward<std::string>(body)}
            , headers{std::forward<http::headers>(headers)} {
        }

        response(http::status status, http::headers &&headers = {})
            : status{status}
            , headers{std::forward<http::headers>(headers)} {
        }


        std::string build_header() {
            std::string output = fmt::format("HTTP/1.1 {} {}\r\n", int(status) , http_status_str(status));
            auto write_header = [&output](const std::string& field, const std::string& value) {
                output += fmt::format("{}: {}\r\n", field, value);
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
