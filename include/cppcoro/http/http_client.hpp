/**
 * @file cppcoro/http_server.hpp
 */
#pragma once

#include <cppcoro/tcp/tcp.hpp>
#include <cppcoro/http/http_connection.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/operation_cancelled.hpp>

#include <utility>
#include <vector>
#include <iostream>

namespace cppcoro::http {

    class client : protected tcp::client
    {
    public:
        using tcp::client::client;
        using tcp::client::stop;
        using connection_type = connection<client>;

        task<connection_type> connect(net::ip_endpoint &&endpoint) {
            connection_type conn{*this, std::move(co_await tcp::client::connect(std::forward<net::ip_endpoint>(endpoint)))};
            co_return conn;
        }

    private:
        cppcoro::cancellation_source cancellation_source_;
    };
} // namespace cpporo::http
