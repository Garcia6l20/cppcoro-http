/**
 * @file cppcoro/http_response.hpp
 */
#pragma once

#include <cppcoro/http/details/static_parser_handler.hpp>

namespace cppcoro::http {

    namespace details {

        template <typename BodyT>
        concept ro_chunked_body = requires(BodyT body) {
            {body.read((void*) nullptr, size_t(0))} -> std::same_as<size_t>;
        };
        template <typename BodyT>
        concept wo_chunked_body = requires(BodyT body) {
            {body.write((void*) nullptr, size_t(0))} -> std::same_as<size_t>;
        };
        template <typename BodyT>
        concept rw_chunked_body = ro_chunked_body<BodyT> and wo_chunked_body<BodyT>;

        template <typename BodyT>
        concept ro_basic_body = requires(BodyT body) {
            {body.data()} -> std::same_as<char*>;
            {body.size()} -> std::same_as<size_t>;
        };
        template <typename BodyT>
        concept wo_basic_body = requires(BodyT body) {
            {BodyT((char *) nullptr, (char *) nullptr)} -> std::same_as<BodyT>;
        };
        template <typename BodyT>
        concept rw_basic_body = ro_basic_body<BodyT> and wo_basic_body<BodyT>;

        template <typename BodyT>
        concept is_body = ro_basic_body<BodyT> and wo_basic_body<BodyT>;

        template <typename BodyT>
        concept readable_body = ro_basic_body<BodyT> or ro_chunked_body<BodyT>;

        template <typename BodyT>
        concept writeable_body = wo_basic_body<BodyT> or wo_chunked_body<BodyT>;

        template<bool _is_response, is_body BodyT>
        struct abstract_message
        {
            static constexpr bool is_response = _is_response;
            using body_type = BodyT;
            std::string url;
            http::headers headers;
            BodyT body;
        };

        template <is_body BodyT>
        struct abstract_response : abstract_message<true, BodyT>
        {
            http::status status;

            std::string build_header() {
                std::string output = fmt::format("HTTP/1.1 {} {}\r\n", int(this->status) , http_status_str(this->status));
                auto write_header = [&output](const std::string& field, const std::string& value) {
                    output += fmt::format("{}: {}\r\n", field, value);
                };
                if constexpr (ro_basic_body<BodyT>) {
                    this->headers["Content-Length"] = std::to_string(this->body.size());
                }
                for (auto &[field, value] : this->headers) {
                    write_header(field, value);
                }
                output += "\r\n";
                return output;
            }
        };

        template <is_body BodyT>
        struct abstract_request : abstract_message<false, BodyT>
        {

            http::method method;

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

    using string_response = details::abstract_response<std::string>;
    using string_request = details::abstract_response<std::string>;

    template <typename MessageT>
    requires details::is_body<decltype(MessageT::body)>
    auto make_parser() {
        auto parser = detail::static_parser_handler<MessageT>{};
        if constexpr (MessageT::is_request) {
            parser.init_parser(detail::http_parser_type::HTTP_REQUEST);
        } else {
            parser.init_parser(detail::http_parser_type::HTTP_RESPONSE);
        }
        return parser;
    }

    class response : public detail::static_parser_handler<response>
    {
    public:
        http::status status;
        std::string url;
        http::headers headers {};
        std::string body;

        response() {
            init_parser(detail::http_parser_type::HTTP_RESPONSE);
        }

        response(response && other) noexcept = default;
        response &operator=(response && other) noexcept = default;
        response(const response & other) noexcept = delete;
        response &operator=(const response & other) noexcept = delete;

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
