#include <cppcoro/http/http_request.hpp>

namespace cppcoro::http {
    int request::on_message_begin(details::http_parser *parser)  {
        auto &this_ = instance(parser);
        this_.state_ = status::on_message_begin;
        return 0;
    }

    int request::on_url(details::http_parser *parser, const char *data, size_t len) {
        auto &this_ = instance(parser);
        this_.url = {data, data + len};
        this_.state_ = status::on_url;
        return 0;
    }

    int request::on_status(details::http_parser *parser, const char *data, size_t len) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_status;
        return 0;
    }

    int request::on_header_field(details::http_parser *parser, const char *data, size_t len) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_headers;
        this_.header_field_ = {data, data + len};
        return 0;
    }

    int request::on_header_value(details::http_parser *parser, const char *data, size_t len) {
        auto &this_ = instance(parser);

        this_.state_ = status::on_headers;
        this_.headers[std::move(this_.header_field_)] = {data, data + len};
        return 0;
    }

    int request::on_headers_complete(details::http_parser *parser) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_headers_complete;
        return 0;
    }

    int request::on_body(details::http_parser *parser, const char *data, size_t len) {
        auto &this_ = instance(parser);
        this_.body = {data, data + len};
        this_.state_ = status::on_body;
        return 0;
    }

    int request::on_message_complete(details::http_parser *parser) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_message_complete;
        return 0;
    }

    int request::on_chunk_header(details::http_parser *parser) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_chunk_header;
        return 0;
    }

    int request::on_chunk_complete(details::http_parser *parser) {
        auto &this_ = instance(parser);
        this_.state_ = status::on_chunk_header_compete;
        return 0;
    }
}
