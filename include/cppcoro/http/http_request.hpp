/**
 * @file cppcoro/http_request.hpp
 */
#pragma once

#include <cppcoro/http/http.hpp>

#include <memory>
#include <stdexcept>

namespace cppcoro::http {
    class request
    {
    public:
        enum class status
        {
            none,
            on_message_begin,
            on_url,
            on_status,
            on_headers,
            on_headers_complete,
            on_body,
            on_message_complete,
            on_chunk_header,
            on_chunk_header_compete,
        };

        request() :
            parser_{std::make_unique<details::http_parser>()} {
            http_parser_init(parser_.get(), details::http_parser_type::HTTP_REQUEST);
            parser_->data = this;
        }

        request(std::string url, std::string body, headers headers)
            : url{std::move(url)}
            , body{std::move(body)}
            , headers{std::move(headers)} {}

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

        [[nodiscard]] http::method method() const {
            return http::method(parser_->method);
        }

        auto method_str() const {
            return http_method_str(static_cast<details::http_method>(parser_->method));
        }

        [[nodiscard]] status state() const { return state_; }

        explicit operator bool() const {
            return state_ == status::on_message_complete;
        }

        void parse(const char *data, size_t len) {
            const auto count = http_parser_execute(parser_.get(), &http_parser_settings_, data, len);
            if (count < len) {
                throw std::runtime_error{
                    std::string("parse error: ") + http_errno_description(details::http_errno(parser_->http_errno))
                };
            }
        }

    private:
        inline static auto &instance(details::http_parser *parser) {
            return *static_cast<request *>(parser->data);
        }

        static int on_message_begin(details::http_parser *parser);
        static int on_url(details::http_parser *parser, const char *data, size_t len);
        static int on_status(details::http_parser *parser, const char *data, size_t len);
        static int on_header_field(details::http_parser *parser, const char *data, size_t len);
        static int on_header_value(details::http_parser *parser, const char *data, size_t len);
        static int on_headers_complete(details::http_parser *parser);
        static int on_body(details::http_parser *parser, const char *data, size_t len);
        static int on_message_complete(details::http_parser *parser);
        static int on_chunk_header(details::http_parser *parser);
        static int on_chunk_complete(details::http_parser *parser);

        inline static details::http_parser_settings http_parser_settings_ = {
            on_message_begin,
            on_url,
            on_status,
            on_header_field,
            on_header_value,
            on_headers_complete,
            on_body,
            on_message_complete,
            on_chunk_header,
            on_chunk_complete,
        };
        std::unique_ptr <details::http_parser> parser_;
        std::string header_field_;
        status state_{status::none};
    };
}
