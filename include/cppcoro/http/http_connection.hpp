#pragma once

#include <cppcoro/tcp/tcp.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/http/http_request.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>

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
                return details::http_parser_type::HTTP_REQUEST;
            } else return details::http_parser_type::HTTP_RESPONSE;
        }

        using receive_type = typename std::conditional<is_response_parser(), response, request>::type;
        using send_type = typename std::conditional<is_response_parser(), request, response>::type;

        connection(connection &&other) noexcept
            : tcp::connection{std::move(other)}, parent_{other.parent_}, input_{std::move(other.input_)},
              buffer_{std::move(other.buffer_)} {
        }

        connection(const connection &other) = delete;

        connection &operator=(connection &&other) = delete;

        connection &operator=(const connection &other) = delete;

        explicit connection(server &server, tcp::connection connection)
            : tcp::connection(std::move(connection)), parent_{server}, input_{std::make_unique<request>()},
              buffer_(2048, 0) {
        }

        explicit connection(client &client, tcp::connection connection)
            : tcp::connection(std::move(connection)), parent_{client}, input_{std::make_unique<response>()},
              buffer_(2048, 0) {
        }

        task<receive_type *> next() {
            while (true) {
                auto ret = co_await sock_.recv(buffer_.data(), buffer_.size(), ct_);
                bool done = ret <= 0;
                if (!done) {
                    input_->parse(buffer_.data(), ret);
                    if (*input_) {
                        co_return input_.get();
                    }
                } else {
                    co_return nullptr;
                }
            }
        }

        task<> send(send_type &to_send) {
            auto header = to_send.build_header();
            auto size = co_await sock_.send(header.data(), header.size(), ct_);
            assert(size == header.size());
            if (!to_send.body.empty()) {
                size = co_await sock_.send(to_send.body.data(), to_send.body.size(), ct_);
                assert(size == to_send.body.size());
                size = co_await sock_.send("\r\n", 2, ct_);
                assert(size == 2);
            }
        }

        task<receive_type *> post(std::string &&path, std::string &&data) requires(is_response_parser()) {
            send_type request{
                http::method::post,
                std::forward<std::string>(path),
                std::forward<std::string>(data),
                {}
            };
            auto header = request.build_header();
            auto size = co_await sock_.send(header.data(), header.size(), ct_);
            assert(size == header.size());
            if (!request.body.empty()) {
                size = co_await sock_.send(request.body.data(), request.body.size(), ct_);
                assert(size == request.body.size());
                size = co_await sock_.send("\r\n", 2, ct_);
                assert(size == 2);
            }
            auto resp = co_await next();
            co_return resp;
        }

    private:
        std::vector<char> buffer_;
        ParentT &parent_;
        std::unique_ptr<receive_type> input_;
    };
}