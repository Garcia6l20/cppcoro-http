/**
 * @file cppcoro/http_request.hpp
 */
#pragma once

#include <cppcoro/http/details/static_parser_handler.hpp>

#include <stdexcept>

namespace cppcoro::http {

    class request : public detail::static_parser_handler<request>
    {
    public:

        request() {
            init_parser(details::http_parser_type::HTTP_REQUEST);
        }

        request(std::string url, std::string body, headers headers)
            : url{std::move(url)}
            , body{std::move(body)}
            , headers{std::move(headers)} {}

        request(http::method method, std::string &&url, std::string &&body, headers &&headers = {})
            : url{std::forward<std::string>(url)}
            , body{std::forward<std::string>(body)}
            , headers{std::forward<http::headers>(headers)} {
            this->method = method;
        }

        request(request &&other) noexcept = delete;

//                : headers{std::move(other.headers)}
//                , url{std::move(other.url)}
//                , body{std::move(other.body)}
//                , parser_{std::move(other.parser_)} {
//                parser_->data = this;
//            }
        request &operator=(request &&) = delete;

        request(const request &) = delete;

        request &operator=(const request &) = delete;

        std::string url;
        std::string body;
        http::headers headers;


        std::string build_header() {
            std::string output = fmt::format("{} {} HTTP/1.1\r\n"
                                             "UserAgent: cppcoro-http/0.0\r\n",
                                             method_str(),
                                             url);
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
