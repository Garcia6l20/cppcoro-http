#pragma once

#include <cppcoro/tcp/tcp.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/http/http_request.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>

#include <charconv>
#include <cstring>

namespace cppcoro::http {

    class client;

    class server;

    template<typename ParentT>
    class connection : public tcp::connection
    {
    public:
        static constexpr bool is_response_parser() {
            if constexpr (std::is_same_v<ParentT, server>) {
                return false;
            } else return true;
        }
        static constexpr auto parser_type() {
            if constexpr (std::is_same_v<ParentT, server>) {
                return detail::http_parser_type::HTTP_REQUEST;
            } else return detail::http_parser_type::HTTP_RESPONSE;
        }

        using receive_type = typename std::conditional<is_response_parser(), response, request>::type;
        using send_type = typename std::conditional<is_response_parser(), request, response>::type;

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
            while (true) {
                //std::fill(begin(buffer_), end(buffer_), '\0');
                auto ret = co_await sock_.recv(buffer_.data(), buffer_.size(), ct_);
                bool done = ret <= 0;
                if (!done) {
                    if(result.parse(buffer_.data(), ret)) {
                        if (result.headers.contains("Content-Length")) {
                            int content_len = 0;
                            auto value = result.headers.at("Content-Length");
                            std::from_chars(value.data(), value.data() + value.size(), content_len);
                            auto body_len = ::strnlen(result.body.data(), result.body.size());
                            if (content_len != body_len) {
                                buffer_.resize(content_len - body_len);
                                fmt::print("- waiting for {} bytes\n", content_len - body_len);
                                continue;
                            }
                        }
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

        task<> send(send_type &&to_send) {
            auto header = to_send.build_header();
            auto size = co_await sock_.send(header.data(), header.size(), ct_);
            assert(size == header.size());
            if (!to_send.body.empty()) {
                size = co_await sock_.send(to_send.body.data(), to_send.body.size(), ct_);
                assert(size == to_send.body.size());
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
            co_await send(std::move(request));
            auto resp = co_await next();
            //fmt::print("{}\n", resp->body);
            co_return resp;
        }

        std::vector<char> buffer_;
        ParentT &parent_;
        // std::unique_ptr<receive_type> input_;
    };
}