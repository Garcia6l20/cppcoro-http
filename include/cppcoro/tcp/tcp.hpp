/**
 * @file cppcoro/tcp.hpp
 */
#pragma once

#include <cppcoro/io_service.hpp>
#include <cppcoro/net/socket.hpp>

#include <utility>

namespace cppcoro {
    namespace net {
        template<bool bind = true>
        auto create_tcp_socket(io_service &ios, const ip_endpoint &endpoint) {
            auto sock = socket{endpoint.is_ipv4() ? socket::create_tcpv4(ios) : socket::create_tcpv6(ios)};
            if constexpr (bind) {
                sock.bind(endpoint);
            }
            return sock;
        }
    }
    namespace tcp {
        class connection
        {
        public:
            connection(connection &&other) noexcept
                : sock_{std::move(other.sock_)}, ct_{std::move(other.ct_)} {}

            connection(const connection &) = delete;

            connection(net::socket socket, cancellation_token ct)
                : sock_{std::move(socket)},
                  ct_{std::move(ct)} {
            }

            [[nodiscard]] const net::ip_endpoint &peer_address() const {
                return sock_.remote_endpoint();
            }

            auto &socket() { return sock_; }

        protected:
            net::socket sock_;
            cancellation_token ct_;
        };

        class server
        {
        public:
            server(server &&other) noexcept: ios_{other.ios_}, endpoint_{std::move(other.endpoint_)},
                                             socket_{std::move(other.socket_)}, cs_{other.cs_} {}

            server(const server &) = delete;

            server(io_service &ios, const net::ip_endpoint &endpoint)
                : ios_{ios}, endpoint_{endpoint}, socket_{net::create_tcp_socket<true>(ios, endpoint_)} {
                socket_.listen();
            }

            task <connection> accept() {
                auto sock = net::create_tcp_socket<false>(ios_, endpoint_);
                co_await socket_.accept(sock, cs_.token());
                co_return connection{std::move(sock), cs_.token()};
            }

            void stop() {
                cs_.request_cancellation();
            }

        protected:
            io_service &ios_;
            net::ip_endpoint endpoint_;
            net::socket socket_;
            cancellation_source cs_;
        };
    }
}