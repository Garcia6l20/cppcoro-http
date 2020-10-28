/**
 * @file cppcoro/http_server.hpp
 */
#pragma once

#include <cppcoro/http/concepts.hpp>
#include <cppcoro/http/http_connection.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/operation_cancelled.hpp>
#include <cppcoro/task.hpp>

#include <iostream>
#include <utility>
#include <vector>

namespace cppcoro::http
{
	template<is_config ConfigT>
	class server : protected tcp::server<typename ConfigT::socket_provider>
	{
	public:
        using connection_type = connection<
            server, ConfigT>;

		using base = tcp::server<typename ConfigT::socket_provider>;
		using base::server;
		using base::service;
		using base::stop;

		task<connection_type> listen()
		{
			auto conn_generator = this->accept();
			while (!this->cs_.is_cancellation_requested())
			{
				spdlog::debug("listening for new connection on {}", this->socket_.local_endpoint());
				connection_type conn{ *this, std::move(co_await this->accept()) };
				spdlog::debug("{} incoming connection: {}", conn.to_string(), conn.peer_address());
				co_return conn;
			}
			throw operation_cancelled{};
		}

	private:
		cppcoro::cancellation_source cancellation_source_;
	};
}  // namespace cppcoro::http
