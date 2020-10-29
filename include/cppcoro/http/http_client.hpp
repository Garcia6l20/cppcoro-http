/**
 * @file cppcoro/http_server.hpp
 */
#pragma once

#include <cppcoro/http/http_connection.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/net/concepts.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/task.hpp>

#include <iostream>
#include <utility>
#include <vector>

namespace cppcoro::http
{
	template<net::is_connection_socket_provider SocketProviderT = tcp::ipv4_socket_provider>
	class client : protected tcp::client<SocketProviderT>
	{
		using base = tcp::client<SocketProviderT>;

	public:
		using base::client;
		using base::service;
		using base::stop;
		using connection_type = connection<client, SocketProviderT>;

		task<connection_type> connect(net::ip_endpoint const& endpoint)
		{
			connection_type conn{ *this, std::move(co_await base::connect(endpoint)) };
			co_return conn;
		}

	private:
		cppcoro::cancellation_source cancellation_source_;
	};
}  // namespace cppcoro::http
