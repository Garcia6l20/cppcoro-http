/**
 * @file cppcoro/http/route_controller.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/async_scope.hpp>

namespace cppcoro::http {

    template<typename SessionT>
    class request_processor : public server
    {
    public:
        using server::server;

        task<> serve() {
            auto handle_conn = [this](http::server::connection_type conn) mutable -> task<> {
                SessionT session{};
                while (true) {
                    try {
                        // wait next connection request
                        auto req = co_await conn.next();
                        if (!req)
                            break; // connection closed
                        // wait next router response
                        co_await conn.send(co_await this->process(*req, session));
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

    protected:
        virtual task <http::response> process(http::request &request, SessionT &session) = 0;
    };
}
