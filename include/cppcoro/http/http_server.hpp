/**
 * @file cppcoro/http_server.hpp
 */
#pragma once

#include <cppcoro/tcp/tcp.hpp>
#include <cppcoro/http/http_request.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <utility>
#include <vector>
#include <iostream>

namespace cppcoro::http {

    class server;

    class connection : public tcp::connection
    {
    public:
        connection(connection &&other) noexcept
            : tcp::connection{std::move(other)}, server_{other.server_}, req_{std::move(other.req_)},
              buffer_{std::move(other.buffer_)} {
        }

        connection(const connection &other) = delete;

        connection &operator=(connection &&other) = delete;

        connection &operator=(const connection &other) = delete;

        explicit connection(server &server, tcp::connection connection)
            : tcp::connection(std::move(connection)), server_{server}, req_{std::make_unique<request>()},
              buffer_(2048, 0) {
        }

        task<request *> next() {
            while (true) {
                auto ret = co_await sock_.recv(buffer_.data(), buffer_.size(), ct_);
                bool done = ret <= 0;
                if (!done) {
                    req_->parse(buffer_.data(), ret);
                    if (*req_) {
                        co_return req_.get();
                    }
                } else {
                    co_return nullptr;
                }
            }
        }

        task<> send(http::response& response) {
            auto header = response.build_header();
            std::cout << "output header: " << header << '\n';
            auto size = co_await sock_.send(header.data(), header.size(), ct_);
            assert(size == header.size());
            std::cout << "output body: " << response.body << '\n';
            if (!response.body.empty()) {
                size = co_await sock_.send(response.body.data(), response.body.size(), ct_);
                assert(size == response.body.size());
                size = co_await sock_.send("\r\n", 2, ct_);
                assert(size == 2);
            }
        }

    private:
        std::vector<char> buffer_;
        server &server_;
        std::unique_ptr<request> req_;
    };

    class server : protected tcp::server
    {
    public:
        using tcp::server::server;
        using tcp::server::stop;

        task<connection> listen() {
            auto conn_generator = accept();
            while (!cs_.is_cancellation_requested()) {
                connection conn{*this, std::move(co_await accept())};
                co_return conn;
            }
            throw operation_cancelled{};
        }

    private:
        cppcoro::cancellation_source cancellation_source_;
    };
} // namespace cpporo::http
