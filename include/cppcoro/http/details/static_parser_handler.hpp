#pragma once

#include <cppcoro/http/http.hpp>

#include <memory>

namespace cppcoro::http {
    template<typename T>
    class connection;
}

namespace cppcoro::http::detail {
    template <typename BaseT>
    class static_parser_handler {
    public:
        http::method method;

        static_parser_handler() = default;
        static_parser_handler(static_parser_handler &&) noexcept = default;
        static_parser_handler& operator=(static_parser_handler &&) noexcept = default;
        static_parser_handler(const static_parser_handler &) noexcept = delete;
        static_parser_handler& operator=(const static_parser_handler &) noexcept = delete;

        auto method_str() const {
            return http_method_str(static_cast<details::http_method>(method));
        }

        explicit operator bool() const {
            return state_ == status::on_message_complete;
        }

        void parse(const char *data, size_t len) {
            const auto count = execute_parser(data, len);
            if (count < len) {
                throw std::runtime_error{
                    std::string("parse error: ") + http_errno_description(details::http_errno(parser_->http_errno))
                };
            }
            method = static_cast<http::method>(parser_->method);
        }

    protected:
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

        inline static auto &instance(details::http_parser *parser) {
            return *static_cast<BaseT *>(parser->data);
        }

        static inline int on_message_begin(details::http_parser *parser)  {
            auto &this_ = instance(parser);
            this_.state_ = status::on_message_begin;
            return 0;
        }

        static inline int on_url(details::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.url = {data, data + len};
            this_.state_ = status::on_url;
            return 0;
        }

        static inline int on_status(details::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_status;
            return 0;
        }

        static inline int on_header_field(details::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_headers;
            this_.header_field_ = {data, data + len};
            return 0;
        }

        static inline int on_header_value(details::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);

            this_.state_ = status::on_headers;
            this_.headers[std::move(this_.header_field_)] = {data, data + len};
            return 0;
        }

        static inline int on_headers_complete(details::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_headers_complete;
            return 0;
        }

        static inline int on_body(details::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.body = {data, data + len};
            this_.state_ = status::on_body;
            return 0;
        }

        static inline int on_message_complete(details::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_message_complete;
            return 0;
        }

        static inline int on_chunk_header(details::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_chunk_header;
            return 0;
        }

        static inline int on_chunk_complete(details::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_chunk_header_compete;
            return 0;
        }

        void init_parser(details::http_parser_type type) {
            if (!parser_) {
                parser_ = std::make_unique<details::http_parser>();
            }
            http_parser_init(parser_.get(), type);
            parser_->data = this;
        }

        auto execute_parser(const char *data, size_t len) {
            return http_parser_execute(parser_.get(), &http_parser_settings_, data, len);
        }


        [[nodiscard]] status state() const { return state_; }

        template<typename T>
        friend class cppcoro::http::connection;

    private:
        std::unique_ptr <details::http_parser> parser_;
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
        std::string header_field_;
        status state_{status::none};
    };
}