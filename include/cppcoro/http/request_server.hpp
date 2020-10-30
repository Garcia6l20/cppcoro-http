/**
 * @file cppcoro/http/request_server.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/async_scope.hpp>
#include <cppcoro/http/concepts.hpp>
#include <cppcoro/http/server.hpp>

namespace cppcoro::http
{
	template<http::is_config ConfigT, typename ProcessorT>
	class request_server : public http::server<ConfigT>
	{
		using base = http::server<ConfigT>;

	public:
		using base::base;
		using session_type = typename ConfigT::session_type;

        session_type create_session() noexcept
		{
			if constexpr (is_server_session<session_type, decltype(*this)>)
			{
				return session_type{ *this };
			}
			else
			{
				return session_type{};
			}
		}

		task<> serve()
		{
			async_scope scope;
			std::exception_ptr exception_ptr;
			try
			{
				while (true)
				{
					auto conn = co_await this->listen();
					scope.spawn(handle_connection(std::move(conn)));
				}
			}
			catch (...)
			{
				exception_ptr = std::current_exception();
			}
			co_await scope.join();
			if (exception_ptr)
			{
				std::rethrow_exception(exception_ptr);
			}
		}

	private:
        task<> handle_connection(typename base::connection_type conn) {
            session_type session = create_session();
            http::string_request default_request;
            auto init_request = [&](http::request_parser& parser)
                -> http::detail::base_request& {
              auto* request =
                  static_cast<ProcessorT*>(this)->prepare(parser, session);
              if (!request)
              {
                  return default_request;
              }
              return *request;
            };
            while (true)
            {
                try
                {
                    // wait next connection request
                    auto req = co_await conn.next(init_request);
                    if (!req)
                        break;  // connection closed

                    if constexpr (is_cookies_session<session_type>)
                    {
                        session.extract_cookies(req->headers);
                    }

                    detail::base_response& resp =
                        co_await static_cast<ProcessorT*>(this)->process(*req);

                    if constexpr (is_cookies_session<session_type>)
                    {
                        session.load_cookies(resp.headers);
                    }

                    // process and send the response
                    co_await conn.send(resp);
                }
                catch (std::system_error& err)
                {
                    if (err.code() == std::errc::connection_reset)
                    {
                        break;  // connection reset by peer
                    }
                    else
                    {
                        throw err;
                    }
                }
                catch (operation_cancelled&)
                {
                    break;
                }
            }
        }
	};
}  // namespace cppcoro::http
