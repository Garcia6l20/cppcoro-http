/**
 * @file cppcoro/http/request_server.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/http_server.hpp>
#include <cppcoro/async_scope.hpp>

namespace cppcoro::http {

    template<typename SocketProviderT, typename SessionT, typename ProcessorT>
	class request_server : public http::server<SocketProviderT>
    {
		using base = http::server<SocketProviderT>;

	public:
		using base::base;
        using session_type = SessionT;

        task<> serve() {
            async_scope scope;
            std::exception_ptr exception_ptr;
            try {
                while (true) {
                    auto conn = co_await this->listen();
                    scope.spawn([](base *srv, typename base::connection_type conn) mutable -> task<> {
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
            } catch (...) {
                exception_ptr = std::current_exception();
			}
            co_await scope.join();
			if (exception_ptr) {
				std::rethrow_exception(exception_ptr);
			}
        }
    };
}
