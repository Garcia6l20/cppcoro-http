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
        static_parser_handler(static_parser_handler &&other) noexcept
            : parser_{std::move(other.parser_)}
            , method{std::move(other.method)}
            , header_field_{std::move(other.header_field_)}
            , state_{std::move(other.state_)} {
            if (parser_) {
                parser_->data = this;
            }
        }
        static_parser_handler& operator=(static_parser_handler &&other) noexcept {
            parser_ = std::move(other.parser_);
            method = std::move(other.method);
            header_field_ = std::move(other.header_field_);
            state_ = std::move(other.state_);
            if (parser_) {
                parser_->data = this;
            }
            return *this;
        }
        static_parser_handler(const static_parser_handler &) noexcept = delete;
        static_parser_handler& operator=(const static_parser_handler &) noexcept = delete;

        auto method_str() const {
            return http_method_str(static_cast<detail::http_method>(method));
        }

        bool parse(const char *data, size_t len) {
            const auto count = execute_parser(data, len);
            if (count < len) {
                throw std::runtime_error{
                    std::string("parse error: ") + http_errno_description(detail::http_errno(parser_->http_errno))
                };
            }
//            if (!parser_->upgrade &&
//                parser_->status != detail::s_message_done)
//                return false;
            method = static_cast<http::method>(parser_->method);
            return state_ == status::on_message_complete;
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

        inline static auto &instance(detail::http_parser *parser) {
            return *static_cast<BaseT *>(parser->data);
        }

        static inline int on_message_begin(detail::http_parser *parser)  {
            auto &this_ = instance(parser);
            this_.state_ = status::on_message_begin;
            return 0;
        }

        static inline int on_url(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.url = {data, data + len};
            this_.state_ = status::on_url;
            return 0;
        }

        static inline int on_status(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_status;
            return 0;
        }

        static inline int on_header_field(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_headers;
            this_.header_field_ = {data, data + len};
            return 0;
        }

        static inline int on_header_value(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);

            this_.state_ = status::on_headers;
            this_.headers[std::move(this_.header_field_)] = {data, data + len};
            return 0;
        }

        static inline int on_headers_complete(detail::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_headers_complete;
            return 0;
        }

        static inline int on_body(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.body = {data, data + len};
            this_.state_ = status::on_body;
            return 0;
        }

        static inline int on_message_complete(detail::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_message_complete;
            return 0;
        }

        static inline int on_chunk_header(detail::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_chunk_header;
            return 0;
        }

        static inline int on_chunk_complete(detail::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_chunk_header_compete;
            return 0;
        }

        void init_parser(detail::http_parser_type type) {
            if (!parser_) {
                parser_ = std::make_unique<detail::http_parser>();
                http_parser_init(parser_.get(), type);
                parser_->data = this;
            }
        }

        auto execute_parser(const char *data, size_t len) {
            return http_parser_execute(parser_.get(), &http_parser_settings_, data, len);
        }


        [[nodiscard]] status state() const { return state_; }

        template<typename T>
        friend class cppcoro::http::connection;

    private:
        std::unique_ptr <detail::http_parser> parser_;
        inline static detail::http_parser_settings http_parser_settings_ = {
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