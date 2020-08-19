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
    protected:
        http::string_response error_response_;
    public:
        using session_type = SessionT;
        using server::server;

        task<> serve() {
            auto handle_conn = [this](http::server::connection_type conn) mutable -> task<> {
                session_type session{};
                while (true) {
                    try {
                        // wait next connection request
                        auto req = co_await conn.next();
                        if (!req)
                            break; // connection closed
                        // wait next router response
                        co_await conn.send(co_await static_cast<ProcessorT*>(this)->process(*req, session));
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
