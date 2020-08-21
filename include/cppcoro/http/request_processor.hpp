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
            auto handle_conn = [this](http::server::connection_type conn) mutable -> task<> {
                session_type session{};
                http::string_request default_request;
                auto init_request = [&](const http::request_parser &parser) -> http::detail::base_request& {
                    auto *request = static_cast<ProcessorT*>(this)->prepare(parser, session);
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
                        co_await conn.send(co_await static_cast<ProcessorT*>(this)->process(*req));
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
            };
            async_scope scope;
            try {
                while (true) {
                    scope.spawn(handle_conn(co_await listen()));
                }
            } catch (operation_cancelled &) {}
            co_await scope.join();
        }
    };
}
