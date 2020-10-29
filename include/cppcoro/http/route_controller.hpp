/**
 * @file cppcoro/http/route_controller.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/details/router.hpp>
#include <cppcoro/http/request.hpp>
#include <cppcoro/http/request_server.hpp>
#include <cppcoro/http/response.hpp>

#include <cppcoro/task.hpp>

#include <ctll.hpp>
#include <ctre.hpp>

namespace cppcoro::http
{
	namespace detail
	{
#define __CPPCORO_HTTP_MAKE_METHOD_CHECKER(__method)                      \
	template<typename ControllerT>                                        \
	concept is_##__method##_controller = requires(ControllerT controller) \
	{                                                                     \
		&ControllerT::on_##__method;                                      \
	};

		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(post)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(get)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(put)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(del)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(head)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(options)
		__CPPCORO_HTTP_MAKE_METHOD_CHECKER(patch)
#undef __CPPCORO_HTTP_MAKE_METHOD_CHECKER

		template<typename ControllerT>
		concept has_init_request_handler = requires()
		{
			&ControllerT::init_request;
		};

		struct abstract_route_controller
		{
			explicit abstract_route_controller(io_service& service) noexcept
				: service_{ service }
			{
			}
			virtual ~abstract_route_controller() = default;

			virtual task<detail::base_response&> process() = 0;
			virtual http::detail::base_request* _init_request(http::request_parser& url) = 0;
			virtual bool match(std::string_view url) = 0;

			io_service& service_;
			void* session_ = nullptr;
		};

	}  // namespace detail

	namespace detail
	{
		template<typename T>
		concept is_visitable = requires(T&& v)
		{
			{ std::get<0>(v) };
		};

		template<typename T>
		concept has_body_type = requires(T&& v)
		{
			typename T::body_type;
		};
	}  // namespace detail

	template<
		ctll::fixed_string route,
		http::is_config ConfigT,
		typename RequestT,
		template<typename>
		typename Derived>
	class route_controller : public detail::abstract_route_controller
	{
		using derived_type = Derived<ConfigT>;
		using session_type = typename ConfigT::session_type;
		using request_type = RequestT;
		using builder_type = ctre::regex_builder<route>;
		static constexpr inline auto match_ = ctre::regex_match_t<typename builder_type::type>();
		std::invoke_result_t<decltype(match_), std::string_view> match_result_;

		std::optional<request_type> request_;

		auto& self() { return static_cast<derived_type&>(*this); }

		using handler_type =
			std::function<cppcoro::task<detail::base_response&>(route_controller&)>;
		std::map<http::method, handler_type> handlers_;
		http::string_response error_response_;

		template<http::method method, typename HandlerT>
		void register_handler(HandlerT&& handler)
		{
			using traits = detail::function_traits<HandlerT>;
			using handler_trait = detail::view_handler_traits<
				cppcoro::task<detail::base_response>,
				detail::function_detail::parameters_tuple_all_enabled,
				HandlerT>;
			using response_type = typename handler_trait::await_result_type;
			std::shared_ptr<response_type> response;
			constexpr auto has_response_body = detail::has_body_type<response_type>;
			if constexpr (has_response_body)
			{
				if constexpr (std::constructible_from<
								  typename response_type::body_type,
								  io_service&>)
				{
					response = std::make_shared<response_type>(
						http::status::HTTP_STATUS_INTERNAL_SERVER_ERROR,
						typename response_type::body_type{ service_ });
				}
				else
				{
					response = std::make_shared<response_type>();
				}
			}
			else
			{
				response = std::make_shared<response_type>();
			}
			handlers_[method] =
				[data = std::make_shared<typename handler_trait::data_type>(),
				 response = response,
				 handler = std::forward<HandlerT>(handler)](
					route_controller& self) mutable -> cppcoro::task<detail::base_response&> {
				handler_trait::load_data(self.match_result_, *data);
				*response = co_await std::apply(
					handler, std::tuple_cat(std::make_tuple(&self.self()), *data));
				if constexpr (detail::is_visitable<response_type>)
				{
					detail::base_response* ptr = nullptr;
					std::visit([&ptr](auto& elem) mutable { ptr = &elem; }, *response);
					co_return* ptr;
				}
				else
				{
					co_return* response;
				}
			};
		};

		bool match(std::string_view url) final
		{
			match_result_ = std::move(match_(url));
			return bool(match_result_);
		}

		http::detail::base_request* _init_request(http::request_parser& parser) final
		{
			request_.emplace(make_request(parser));
			assert(match(request_->path));  // reload results
			if constexpr (detail::has_init_request_handler<derived_type>)
			{
				using traits = detail::function_traits<decltype(&derived_type::init_request)>;
				using data_type = typename traits::template parameters_tuple<
					detail::function_detail::parameters_tuple_disable<request_type>>::tuple_type;
				data_type data;
				detail::load_data(match_result_, data);
				std::apply(
					&derived_type::init_request,
					std::tuple_cat(
						std::make_tuple(static_cast<derived_type*>(this)),
						data,
						std::tuple<request_type&>(*request_)));
			}
			return &*request_;
		}

		auto make_request(http::request_parser& parser)
		{
			using request_body = typename request_type::body_type;
			if constexpr (std::constructible_from<request_body, io_service&>)
			{
				return request_type{ parser, request_body{ service_ } };
			}
			else
			{
				return request_type{ parser, request_body{} };
			}
		}

	protected:
		auto& request() { return *request_; }
		auto& session() { return *static_cast<session_type*>(session_); }
		auto& service() { return service_; }

	public:
		route_controller(const route_controller&) = delete;
		route_controller& operator=(const route_controller&) = delete;
		route_controller(route_controller&& other) = default;
		route_controller& operator=(route_controller&& other) noexcept = default;

		explicit route_controller(io_service& service)
			: detail::abstract_route_controller{ service }
		{
#define __CPPCORO_HTTP_MAKE_METHOD_CHECKER_IMPL(__method)                       \
	if constexpr (detail::is_##__method##_controller<derived_type>)             \
	{                                                                           \
		register_handler<http::method::__method>(&derived_type::on_##__method); \
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

		task<detail::base_response&> process() override
		{
			if (handlers_.contains(request_->method))
			{
				auto& result = co_await handlers_.at(request_->method)(*this);
				co_return result;
			}
			error_response_.status = http::status::HTTP_STATUS_METHOD_NOT_ALLOWED;
			co_return error_response_;
		}
	};

	template<http::is_config ConfigT, template<typename> typename... ControllersT>
	struct controller_server
		: http::request_server<ConfigT, controller_server<ConfigT, ControllersT...>>
	{
		using processor_type =
			http::request_server<ConfigT, controller_server<ConfigT, ControllersT...>>;

		using session_type = typename ConfigT::session_type;

		template<typename... ArgsT>
		controller_server(
			io_service& service, const net::ip_endpoint& endpoint, const ArgsT&... args) noexcept
			: processor_type{ service, endpoint }
			, controllers_{ std::make_unique<ControllersT<ConfigT>>(
				  ControllersT<ConfigT>{ this->ios_, args... })... }
		{
		}

		http::detail::base_request* prepare(http::request_parser& parser, session_type& session)
		{
			next_proc_ = nullptr;
			for (auto& controller : controllers_)
			{
				if (controller->match(parser.url()))
				{
					controller->session_ = &session;
					next_proc_ = controller.get();
					return controller->_init_request(parser);
				}
			}
			return nullptr;
		}

		cppcoro::task<http::detail::base_response&> process(http::detail::base_request& request)
		{
			if (!next_proc_)
			{
				error_response_ = string_response{
					http::status::HTTP_STATUS_NOT_FOUND,
				};
				co_return error_response_;
			}
			else
			{
				http::detail::base_response& response = co_await next_proc_->process();
				co_return response;
			}
		}

	private:
		string_response error_response_;
		detail::abstract_route_controller* next_proc_;
		std::array<std::unique_ptr<detail::abstract_route_controller>, sizeof...(ControllersT)>
			controllers_;
	};
}  // namespace cppcoro::http
