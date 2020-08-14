#pragma once

#include <variant>
#include <cppcoro/details/router.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/http/http_response.hpp>
#include <cppcoro/http/http_request.hpp>

#include <cppcoro/task.hpp>

#include <concepts>
#include <ctre/functions.hpp>
#include <ctll/fixed_string.hpp>

namespace cppcoro::http {

    struct _dumb_route_context
    {
    };

    class router
    {
    public:
        using response_variant = std::variant<
            http::status,
            std::string,
            std::tuple<http::status, std::string>>;

        using return_type = response_variant;

        router() = default;

        struct abs_route
        {
            virtual bool match(const std::string_view &) = 0;

            virtual cppcoro::task<return_type> do_complete() = 0;
        };

        template<auto regex_literal>
        struct route : abs_route
        {
            using builder_type = ctre::regex_builder<regex_literal>;
            static constexpr inline auto match_ = ctre::regex_match_t<typename builder_type::type>();
            std::invoke_result_t<decltype(match_), std::string_view> result_;

            bool match(const std::string_view &url) override {
                result_ = std::move(match_(url));
                return bool(result_);
            }

            using handler_type = std::function<task<return_type>()>;
            using chunk_handler_type = std::function<void(std::string_view)>;
            using data_type = std::shared_ptr<void>;

            static void escape(std::string &input) {
                std::regex special_chars{R"([-[\]{}()*+?.,\^$|#\s])"};
                std::regex_replace(input, special_chars, R"(\$&)");
            }

            route() = default;

            route(route &&) noexcept = default;

            route &operator=(route &&) noexcept = default;

            route(const route &) = delete;

            route &operator=(const route &) = delete;

            template<typename HandlerT>
            route &init(HandlerT handler) {
                if (_complete_handler) {
                    throw std::logic_error("init must be called() before complete()");
                }
//                using traits = details::view_handler_traits<init_return_type, RouterContextT, HandlerT>;
//                auto data = std::make_shared<typename traits::data_type>();
//                _init_handler = [this, data, handler](const std::smatch& match, RouterContextT& ctx) {
//                    traits::load_data(match, *data);
//                    using invoker_type = typename traits::template invoker<decltype(handler)>;
//                    if constexpr (std::is_void_v<typename invoker_type::return_type>) {
//                        invoker_type{}(handler, *data, ctx);
//                        return init_return_type{ string_body{}, string_body::value_type{} };
//                    } else {
//                        auto res = invoker_type{}(handler, *data, ctx);
//                        return init_return_type{ body_traits::template body_of<decltype(res)>(), std::move(res) };
//                    }
//                };
                return *this;
            }

            template<typename TaskT>
            route &complete(TaskT &&handler) {
                using traits = http::details::view_handler_traits<return_type, TaskT>;
                using handler_return_type = typename traits::return_type::value_type;

//                static_assert(traits::arity == 2);
//                static_assert(std::same_as<typename traits::arg<0>::type, const std::string&>);
//                static_assert(std::same_as<handler_return_type, std::tuple<http::status, std::string>>);

                static_assert(std::convertible_to<handler_return_type, return_type>);

                auto data = std::make_shared<typename traits::data_type>();
                _complete_handler = [this, data, handler = std::forward<TaskT>(handler)]() -> task<return_type> {
                    traits::load_data(result_, *data);
                    return_type result = co_await typename traits::invoker_type{}(handler, *data);
                    co_return result;
                };

//                if (!_init_handler) {
//                    // create default parser assigner
//                    _assign_parser = [](parser_type& var, request_parser<empty_body>&req) {
//                        var.template emplace<request_parser<UpstreamBodyType>>(std::move(req));
//                    };
//                }
//
//                _complete_handler = [this, data, handler](const std::smatch& match, RouterContextT& ctx) -> return_type {
//                    traits::load_data(match, *data);
//                    using invoker_type = typename traits::template invoker<decltype(handler)>;
//                    auto result = std::move(invoker_type{}(handler, *data, ctx));
//                    using ResT = std::decay_t<decltype(result)>;
//                    if constexpr (std::disjunction_v<std::is_same<std::remove_cvref_t<ResT>, response<BodyTypes>>...>) {
//                    return std::move(result);
//                } else if constexpr (std::is_same_v<std::remove_cvref_t<ResT>, std::string>) {
//                    response<string_body> reply{
//                        std::piecewise_construct,
//                        std::make_tuple(result),
//                        std::make_tuple(status::ok, XDEV_NET_HTTP_DEFAULT_VERSION)
//                    };
//                    reply.set(field::content_type, "text/plain");
//                    reply.content_length(reply.body().size());
//                    return reply;
//                } else if constexpr (std::is_same_v<std::remove_cvref_t<ResT>, std::tuple<std::string, std::string>>) {
//                    response<string_body> reply{
//                        std::piecewise_construct,
//                        std::make_tuple(std::get<1>(result)),
//                        std::make_tuple(status::ok, XDEV_NET_HTTP_DEFAULT_VERSION)
//                    };
//                    reply.set(field::content_type, std::get<0>(result));
//                    reply.content_length(reply.body().size());
//                    return reply;
//                } else if constexpr (std::is_same_v<std::remove_cvref_t<ResT>, std::tuple<http::status, std::string, std::string>>) {
//                    response<string_body> reply{
//                        std::piecewise_construct,
//                        std::make_tuple(std::get<2>(result)),
//                        std::make_tuple(std::get<0>(result), XDEV_NET_HTTP_DEFAULT_VERSION)
//                    };
//                    reply.set(field::content_type, std::get<1>(result));
//                    reply.content_length(reply.body().size());
//                    return reply;
//                } else if constexpr (std::disjunction_v<std::is_same<std::remove_cvref_t<ResT>, typename BodyTypes::value_type>...>) {
//                    response<typename body_traits::template body_of_value_t<ResT>> reply {
//                        std::piecewise_construct,
//                        std::make_tuple(std::move(result)),
//                        std::make_tuple(status::ok, XDEV_NET_HTTP_DEFAULT_VERSION)
//                    };
//                    return std::move(reply);
//                } else static_assert (always_false_v<ResT>, "Unhandled complete return type");
//                };
                return *this;
            }

            template<typename HandlerT>
            route &chunk(HandlerT handler) {
//                using traits = details::view_handler_traits<void, RouterContextT, HandlerT>;
//                _chunk_handler = [this, handler] (std::string_view data, RouterContextT& ctx) {
//                    if constexpr (traits::has_context_last) {
//                        handler(data, ctx);
//                    } else {
//                        handler(data);
//                    }
//                };
                return *this;
            }

            bool match(const std::string &url) {
                std::smatch match;
                return _match();
            }

            cppcoro::task<return_type> do_complete() override {
                auto ret = co_await _complete_handler();
                co_return ret;
            }
//
//            void do_init(const std::smatch& match, RouterContextT& ctx, parser_type& var, request_parser<empty_body>&req) {
//                if (!_init_handler) {
//                    _assign_parser(var, req);
//                    return;
//                } else {
//                    using parser_variant = typename body_traits::parser_variant;
//                    using body_value_variant = typename body_traits::body_value_variant;
//                    auto init_tup = _init_handler(match, ctx);
//                    std::visit([this,&var,&req, &init_tup](auto&body) {
//                        using BodyT = std::decay_t<decltype(body)>;
//                        std::visit(overloaded {
//                            [] (auto&body_value) {
//                                using BodyValueT = std::decay_t<decltype(body_value)>;
//                                throw std::runtime_error("Body type mismatch: " +
//                                                         boost::typeindex::type_id<request_parser<BodyT>>().pretty_name() + " != " +
//                                                                                                         boost::typeindex::type_id<BodyValueT>().pretty_name());
//                            },
//                            [this,&var,&req, &body] (typename BodyT::value_type& body_value) {
//                                var.template emplace<request_parser<BodyT>>(std::move(req));
//                                std::get<request_parser<BodyT>>(var).get().body() = std::move(body_value);
//                            }
//                        }, std::get<1>(init_tup));
//                    }, std::get<0>(init_tup));
//                }
//            }

            bool do_chunk(std::string_view data, const std::smatch & /*match*/) {
                if (!_chunk_handler) {
                    return false;
                }
                _chunk_handler(data);
                return true;
            }

        private:
            std::function<bool()> _match;
            std::function<std::string_view(int)> _get_match_arg;

//            request_parser_assign_func _assign_parser;
//            std::regex _regex;
            handler_type _complete_handler;
//            init_handler_type _init_handler;
            chunk_handler_type _chunk_handler;
        };

        template<ctll::fixed_string RegexT>
        auto &add_route() {
            using route_type = route<RegexT>;
            return *static_cast<route_type *>(routes_.emplace_back(std::make_unique<route_type>()).get());
        }

        std::optional<abs_route*> route_for(const std::string &url) {
            for (auto &route: routes_) {
                if (route->match(url))
                    return route.get();
            }
            return {};
        }

        cppcoro::task<http::response> process(const http::request &request) {
            auto route = route_for(request.url);
            if (route) {
                co_return std::visit(details::overloaded{
                    [](std::tuple<http::status, std::string> &&stat_str) {
                        auto &[status, body] = stat_str;
                        return http::response{
                            status, {}, std::forward<std::string>(body)
                        };
                    }, [](http::status &&status) {
                        return http::response{
                            status
                        };
                    }, [](std::string &&body) {
                        return http::response{
                            http::status::HTTP_STATUS_OK, {}, std::forward<std::string>(body)
                        };
                    }
                }, co_await route.value()->do_complete());
            } else {
                co_return http::response {http::status::HTTP_STATUS_NOT_FOUND};
            }
        }

    private:

        std::vector<std::unique_ptr<abs_route>> routes_;
    };

//    using simple_router = base_router<_dumb_route_context, string_body, file_body, dynamic_body>;

} // namespace xdev
