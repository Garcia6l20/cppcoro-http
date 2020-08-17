/**
 * @file cppcoro/http/route_controller.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/http_response.hpp>
#include <cppcoro/http/http_request.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/http/details/router.hpp>
#include <cppcoro/http/request_processor.hpp>

#include <ctll.hpp>
#include <ctre.hpp>

namespace cppcoro::http {

    namespace detail {

#define __CPPCORO_HTTP_MAKE_METHOD_CHECKER(__method) \
        template <typename ControllerT> \
        concept is_ ## __method ## _controller = requires (ControllerT controller) { \
            &ControllerT::on_ ## __method; \
        };

        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(post)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(get)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(put)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(del)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(head)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(options)
        __CPPCORO_HTTP_MAKE_METHOD_CHECKER(patch)
#undef __CPPCORO_HTTP_MAKE_METHOD_CHECKER

        struct abstract_route_controller
        {
            virtual ~abstract_route_controller() = default;

            virtual task<http::response> process(http::request &request) = 0;

            void *session_ = nullptr;
        };

    }

    template<ctll::fixed_string route, typename SessionT, typename Derived>
    class route_controller : public detail::abstract_route_controller
    {
        using builder_type = ctre::regex_builder<route>;
        static constexpr inline auto match_ = ctre::regex_match_t<typename builder_type::type>();
        std::invoke_result_t<decltype(match_), std::string_view> match_result_;

        auto &self() {
            return *static_cast<Derived *>(this);
        }

        using handler_type = std::function<cppcoro::task<http::response>(route_controller&)>;
        std::map<http::method, handler_type> handlers_;

        template<http::method method, typename HandlerT>
        void register_handler(HandlerT &&handler) {
            using traits = detail::function_traits<HandlerT>;
            using handler_trait = detail::view_handler_traits<cppcoro::task<http::response>,
                detail::function_detail::parameters_tuple_all_enabled,
                HandlerT>;
            auto data = std::make_shared<typename handler_trait::data_type>();
            handlers_[method] = [data,
                handler = std::forward<HandlerT>(handler)](route_controller &self) mutable -> task<http::response> {
                handler_trait::load_data(self.match_result_, *data);
                co_return co_await std::apply(handler, std::tuple_cat(std::make_tuple(&self.self()), *data));
            };
        };

        bool match(const std::string_view &url) {
            match_result_ = std::move(match_(url));
            return bool(match_result_);
        }

    protected:

        auto &session() { return *static_cast<SessionT*>(session_); }

    public:
        route_controller(const route_controller&) = delete;
        route_controller& operator=(const route_controller&) = delete;
        route_controller(route_controller &&other) = default;
        route_controller& operator=(route_controller &&other) noexcept = default;

        route_controller() {
#define __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(__method) \
            if constexpr (detail::is_ ## __method ## _controller<Derived>) { \
                register_handler<http::method:: __method>(&Derived::on_ ## __method);\
            }
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(post)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(get)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(put)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(del)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(head)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(options)
            __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(patch)

#undef __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL
        }

        task<http::response> process(http::request &request) override {
            if (match(request.url)) {
                if (handlers_.contains(request.method)) {
                    auto result = co_await handlers_.at(request.method)(*this);
                    co_return std::move(result);
                }
                co_return http::response{http::status::HTTP_STATUS_METHOD_NOT_ALLOWED};
            }
            co_return http::response{http::status::HTTP_STATUS_NOT_FOUND};
        }
    };

    template<typename SessionType, typename...ControllersT>
    struct controller_server : http::request_processor<SessionType>
    {
        using session_type = SessionType;
        using http::request_processor<SessionType>::request_processor;

        cppcoro::task<http::response> process(http::request &request, session_type &session) override {
            for (auto &controller : controllers_) {
                controller->session_ = &session;
                http::response response = co_await controller->process(request);
                if (response.status != http::status::HTTP_STATUS_NOT_FOUND) {
                    co_return response;
                }
            }
            co_return http::response{
                http::status::HTTP_STATUS_NOT_FOUND
            };
        }

    private:
        std::array<std::unique_ptr<detail::abstract_route_controller>, sizeof...(ControllersT)> controllers_ = {
            std::make_unique<ControllersT>(ControllersT{})...
        };
    };
}
