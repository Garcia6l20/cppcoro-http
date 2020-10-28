#pragma once

#include <cppcoro/http/http.hpp>
#include <cppcoro/http/url.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/async_generator.hpp>

#include <fmt/format.h>

#include <memory>

namespace cppcoro::http::detail {

    template<typename BodyT>
    concept ro_chunked_body = requires(BodyT &&body) {
        { body.read(size_t(0)) } -> std::same_as<async_generator<std::string_view>>;
    };
    template<typename BodyT>
    concept wo_chunked_body = requires(BodyT &&body) {
        { body.write(std::string_view{}) } -> std::same_as<task<size_t>>;
    };
    template<typename BodyT>
    concept rw_chunked_body = ro_chunked_body<BodyT> and wo_chunked_body<BodyT>;

    template<typename BodyT>
    concept ro_basic_body = requires(BodyT &&body) {
        { body.data() } -> std::same_as<char *>;
        { body.size() } -> std::same_as<size_t>;
    };
    template<typename BodyT>
    concept wo_basic_body = requires(BodyT &&body) {
        //{ BodyT((char *) nullptr, (char *) nullptr) } -> std::same_as<BodyT>;
        { body.append(std::string_view{}) };
    };
    template<typename BodyT>
    concept rw_basic_body = ro_basic_body<BodyT> and wo_basic_body<BodyT>;


    template<typename BodyT>
    concept readable_body = ro_basic_body<BodyT> or ro_chunked_body<BodyT>;

    template<typename BodyT>
    concept writeable_body = wo_basic_body<BodyT> or wo_chunked_body<BodyT>;

    template<typename BodyT>
    concept is_body = readable_body<BodyT> or writeable_body<BodyT>;

    template <bool is_request>
    class static_parser_handler {
        using self_type = static_parser_handler<is_request>;

    public:
        static_parser_handler() = default;
        static_parser_handler(static_parser_handler &&other) noexcept
            : parser_{std::move(other.parser_)}
            , header_field_{std::move(other.header_field_)}
            , state_{std::move(other.state_)} {
            if (parser_) {
                parser_->data = this;
            }
        }
        static_parser_handler& operator=(static_parser_handler &&other) noexcept {
            parser_ = std::move(other.parser_);
            header_field_ = std::move(other.header_field_);
            state_ = std::move(other.state_);
            if (parser_) {
                parser_->data = this;
            }
            return *this;
        }
        static_parser_handler(const static_parser_handler &) noexcept = delete;
        static_parser_handler& operator=(const static_parser_handler &) noexcept = delete;

        bool has_body() const noexcept {
            return body_.size();
        }

        operator bool() const {
            return state_ == status::on_message_complete;
        }

        const void parse(const char *data, size_t len) {
            body_ = {};
            const auto count = execute_parser(data, len);
            if (count < len) {
                throw std::runtime_error{
                    std::string("parse error: ") + http_errno_description(detail::http_errno(parser_->http_errno))
                };
            }
//            if (!parser_->upgrade &&
//                parser_->status != detail::s_message_done)
//                return false;
        }

        const void parse(std::string_view input) {
            parse(input.data(), input.size());
        }

        auto method() const {
            return static_cast<http::method>(parser_->method);
        }
        auto status_code() const {
            return static_cast<http::status>(parser_->status_code);
        }

        template <typename MessageT>
        task<> load(MessageT &message) {
            static_assert(is_request == MessageT::is_request);
            if constexpr (is_request) {
                message.method = method();
                message.path = url_;
            } else {
                message.status = status_code();
            }
            if (!this->body_.empty()) {
                co_await message.write_body(body_);
            }
        }

        const auto &url() const {
            return url_;
        }

        auto &url() {
            return url_;
        }

        std::string to_string() const {
            fmt::memory_buffer out;
            std::string_view type;
            if constexpr (is_request) {
                fmt::format_to(out, "request {} {}",
                               detail::http_method_str(detail::http_method(parser_->method)),
                               url_);
            } else {
                fmt::format_to(out, "response {} ",
                               detail::http_status_str(detail::http_status(parser_->status_code)));
            }
            fmt::format_to(out, "{}", body_);
            return out.data();
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
            return *static_cast<self_type *>(parser->data);
        }

        static inline int on_message_begin(detail::http_parser *parser)  {
            auto &this_ = instance(parser);
            this_.state_ = status::on_message_begin;
            return 0;
        }

        static inline int on_url(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.url_ = uri::unescape({data, data + len});
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
            this_.header_field_ = {data, len};
            return 0;
        }

        static inline int on_header_value(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);

            this_.state_ = status::on_headers;
            this_.headers_.emplace(std::string{this_.header_field_}, std::string{data, data + len});
            return 0;
        }

        static inline int on_headers_complete(detail::http_parser *parser) {
            auto &this_ = instance(parser);
            this_.state_ = status::on_headers_complete;
            return 0;
        }

        static inline int on_body(detail::http_parser *parser, const char *data, size_t len) {
            auto &this_ = instance(parser);
            this_.body_ = {data, len};
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

        void init_parser() {
            if (!parser_) {
                parser_ = std::make_unique<detail::http_parser>();
                if constexpr (is_request)
                    http_parser_init(parser_.get(), detail::http_parser_type::HTTP_REQUEST);
                else http_parser_init(parser_.get(), detail::http_parser_type::HTTP_RESPONSE);
                parser_->data = this;
            }
        }

        auto execute_parser(const char *data, size_t len) {
            init_parser();
            return http_parser_execute(parser_.get(), &http_parser_settings_, data, len);
        }

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
        status state_{status::none};
        std::string_view header_field_;
        std::string url_;
        std::string_view body_;
        http::headers headers_;

        template<bool _is_response, is_body BodyT>
        friend struct abstract_message;
    };
}