/**
 * @file cppcoro/http/route_controller.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/async_scope.hpp>

namespace cppcoro::http {

    template<typename SessionT, typename ProcessorT>
    class request_processor : public server
    {
    public:
        using session_type = SessionT;
        using server::server;

        task<> serve() {
            async_scope scope;
            try {
                while (true) {
                    auto conn = co_await listen();
                    scope.spawn([](server *srv, http::server::connection_type conn) mutable -> task<> {
                        session_type session{};
                        http::string_request default_request;
                        auto init_request = [&](const http::request_parser &parser) -> http::detail::base_request& {
                            auto *request = static_cast<ProcessorT*>(srv)->prepare(parser, session);
                            if (!request) {
                                return default_request;
                            }
                            return *request;
                        };
                        while (true) {
                            try {
                                // wait next connection request
                                auto req = co_await conn.next(init_request);
                                if (!req)
                                    break; // connection closed
                                // process and send the response
                                co_await conn.send(co_await static_cast<ProcessorT*>(srv)->process(*req));
                            } catch (std::system_error &err) {
                                if (err.code() == std::errc::connection_reset) {
                                    break; // connection reset by peer
                                } else {
                                    throw err;
                                }
                            } catch (operation_cancelled &) {
                                break;
                            }
                        }
                    }(this, std::move(conn)));
                }
            } catch (operation_cancelled &) {}
            co_await scope.join();
        }
    };
}
