#pragma once

#include <cppcoro/tcp/tcp.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/http/http_request.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>

#include <charconv>
#include <cstring>

namespace cppcoro::http {

    class client;

    class server;

    template<typename ParentT, typename ResponseT = string_response, typename RequestT = string_request>
    class connection : public tcp::connection
    {
    public:
        static constexpr bool is_response_parser() {
            if constexpr (std::is_same_v<ParentT, server>) {
                return false;
            } else return true;
        }
        using receive_type = std::conditional_t<is_response_parser(), ResponseT, RequestT>;
        using send_type = std::conditional_t<is_response_parser(), RequestT, ResponseT>;
        using parser_type = std::conditional_t<is_response_parser(), response_parser, request_parser>;

        connection(connection &&other) noexcept
            : tcp::connection{std::move(other)}, parent_{other.parent_}, /*input_{std::move(other.input_)},*/
              buffer_{std::move(other.buffer_)} {
        }

        connection(const connection &other) = delete;

        connection &operator=(connection &&other) = delete;

        connection &operator=(const connection &other) = delete;

        explicit connection(server &server, tcp::connection connection)
            : tcp::connection(std::move(connection)), parent_{server}, /*input_{std::make_unique<request>()},*/
              buffer_(2048, 0) {
        }

        explicit connection(client &client, tcp::connection connection)
            : tcp::connection(std::move(connection)), parent_{client}, /*input_{std::make_unique<response>()},*/
              buffer_(2048, 0) {
        }

        task<std::optional<receive_type>> next() {
            receive_type result;
            parser_type parser;
            while (true) {
                //std::fill(begin(buffer_), end(buffer_), '\0');
                auto ret = co_await sock_.recv(buffer_.data(), buffer_.size(), ct_);
                bool done = ret <= 0;
                if (!done) {
                    parser.parse(buffer_.data(), ret);
                    if(parser.has_body() && not parser) {
                        // chunk
                        co_await parser.load(result);
                    }
                    if(parser) {
                        co_await parser.load(result);
                        co_return result;
                    }
                } else {
                    co_return std::nullopt;
                }
            }
        }


        auto post(std::string &&path, std::string &&data = "") requires(is_response_parser()) {
            return _send<http::method::post>(std::forward<std::string>(path), std::forward<std::string>(path));
        }
        auto get(std::string &&path = "/", std::string &&data = "") requires(is_response_parser()) {
            return _send<http::method::get>(std::forward<std::string>(path), std::forward<std::string>(path));
        }

        task<> send(http::detail::base_message &to_send) {
            auto header = to_send.build_header();
//            auto [size, body] = co_await when_all(
//                sock_.send(header.data(), header.size(), ct_),
//                to_send.read_body()
//            )
            auto size = co_await sock_.send(header.data(), header.size(), ct_);
            assert(size == header.size());
            if (to_send.is_chunked()) {
                std::string_view body;
                while (true) {
                    body = co_await to_send.read_body();
                    if (body.empty()) {
                        auto size_str = fmt::format("{}\r\n\r\n", 0);
                        co_await sock_.send(size_str.data(), size_str.size(), ct_);
                        break;
                    } else {
                        auto size_str = fmt::format("{:x}\r\n", body.size());
                        co_await sock_.send(size_str.data(), size_str.size(), ct_);
                        size = co_await sock_.send(body.data(), body.size(), ct_);
                        assert(size == body.size());
                        co_await sock_.send("\r\n", 2, ct_);
                    }
                }
            } else {
                auto body = co_await to_send.read_body();
                if (!body.empty()) {
                    size = co_await sock_.send(body.data(), body.size(), ct_);
                    assert(size == body.size());
                }
            }
        }

    private:


        template <http::method _method>
        task<std::optional<receive_type>> _send(std::string &&path, std::string &&data = "") requires(is_response_parser()) {
            send_type request{
                _method,
                std::forward<std::string>(path),
                std::forward<std::string>(data),
                {}
            };
            co_await send(request);
            auto resp = co_await next();
            //fmt::print("{}\n", resp->body);
            co_return resp;
        }

        std::vector<char> buffer_;
        ParentT &parent_;
        // std::unique_ptr<receive_type> input_;
    };
}