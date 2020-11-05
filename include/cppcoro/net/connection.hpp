#pragma once

#include <cppcoro/net/make_socket.hpp>

namespace cppcoro::net
{
	enum class connection_mode {
		client, server
	};
}