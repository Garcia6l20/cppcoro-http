#pragma once

#include <cppcoro/net/transport.hpp>
#include <cppcoro/net/ip_endpoint.hpp>

#include <cppcoro/tcp/transport.hpp>

#include <variant>

namespace cppcoro::net {

	template <typename ...TypesT>
	struct polymorphic_value : std::variant<TypesT...> {
		using std::variant<TypesT...>::variant;
	};

	class server {
	public:
		server(tcp_transport&&, ip_endpoint&& endpoint) {

		}

	private:
		polymorphic_value<
	};
}