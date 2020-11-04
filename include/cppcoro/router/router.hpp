#pragma once

#include <ctre.hpp>
#include <tuple>

#include <cppcoro/router/route_parameter.hpp>
#include <cppcoro/detail/function_traits.hpp>
#include <cppcoro/detail/tuple_utils.hpp>
#include <variant>

namespace cppcoro::router
{
	template<typename T>
	struct context
	{
		using type = T;
		operator bool() { return elem_ != nullptr; }
		decltype(auto) operator*() { return *elem_; }
		decltype(auto) operator->() { return elem_; }
		auto& operator=(T& elem)
		{
			elem_ = &elem;
			return *this;
		}
		T* elem_ = nullptr;
	};

	namespace detail
	{
		template<auto route_, typename FnT>
		class handler
		{
		public:
			constexpr explicit handler(FnT&& fn) noexcept
				: fn_{ std::forward<decltype(fn)>(fn) }
			{
			}
			static constexpr auto route = route_;
			using fn_type = FnT;

		private:
			using fn_trait = cppcoro::detail::function_traits<fn_type>;

			struct no_context_predicate
			{
				template<typename T>
				static constexpr bool value =
					cppcoro::detail::specialization_of<T, router::context>;
			};

			//			using no_context_predicate =
			//cppcoro::detail::parameters_tuple_disable<ContextT>;

			template<typename Predicate = cppcoro::detail::parameters_tuple_all_enabled>
			using parameters_trait = typename fn_trait::template parameters_tuple<Predicate>;
			using parameters_tuple_type = typename parameters_trait<>::tuple_type;
			parameters_tuple_type parameters_data_ = parameters_trait<>::make();

			FnT fn_;

			using builder_type = ctre::regex_builder<route>;
			static constexpr inline auto match_ =
				ctre::regex_match_t<typename builder_type::type>();
			std::invoke_result_t<decltype(match_), std::string_view> match_result_;

			template<
				int type_idx,
				int match_idx,
				typename MatcherT,
				typename ContextT,
				typename ArgsT>
			void _load_data(
				ContextT& context, parameters_tuple_type& data, const MatcherT& match, ArgsT&& args)
			{
				if constexpr (type_idx < fn_trait::arity)
				{
					using ParamT = typename fn_trait::template arg<type_idx>::clean_type;
					if constexpr (cppcoro::detail::specialization_of<ParamT, router::context>)
					{
						if constexpr (cppcoro::detail::in_tuple<ArgsT, typename ParamT::type&>)
						{
							std::get<type_idx>(data) = std::get<typename ParamT::type&>(args);
						}
						else
						{
							std::get<type_idx>(data) = std::get<typename ParamT::type&>(context);
						}
						_load_data<type_idx + 1, match_idx>(
							context, data, match, std::forward<ArgsT>(args));
					}
					else
					{
						std::get<type_idx>(data) =
							route_parameter<ParamT>::load(match.template get<match_idx>());
						_load_data<
							type_idx + 1,
							match_idx + route_parameter<ParamT>::group_count() + 1>(
							context, data, match, std::forward<ArgsT>(args));
					}
				}
			}

			template<typename MatcherT, typename ContextT, typename ArgsT>
			bool load_data(
				ContextT& context, const MatcherT& match, parameters_tuple_type& data, ArgsT&& args)
			{
				_load_data<0, 1>(context, data, match, std::forward<ArgsT>(args));
				return true;
			}

		public:
			constexpr bool matches(std::string_view url)
			{
				match_result_ = std::move(match_(url));
				return bool(match_result_);
			}

			using result_t = typename fn_trait::return_type;

			template<typename ContextT, typename ArgsT>
			std::optional<result_t>
			operator()(ContextT& context, std::string_view path, ArgsT&& args)
			{
				if (matches(path))
				{
					load_data(
						context, match_result_, parameters_data_, std::forward<ArgsT>(args));
					return std::apply(fn_, parameters_data_);
				}
				else
				{
					return {};
				}
			}

			struct handler_view
			{
				static constexpr auto route = route_;
				using fn_type = FnT;
				fn_type fn_;
			};
		};

	}  // namespace detail

	template<ctll::fixed_string route, typename FnT>
	constexpr auto on(FnT&& fn)
	{
		return detail::handler<route, FnT>{ std::forward<FnT>(fn) };
	}

	template<
		cppcoro::detail::is_tuple ContextT = std::tuple<>,
		typename... HandlersT>
	class router
	{
		ContextT context_{};
		using handlers_t = std::tuple<HandlersT...>;

	public:
		constexpr explicit router(HandlersT&&... handlers) noexcept
			: handlers_{ std::forward<HandlersT>(handlers)... }
		{
		}

		constexpr explicit router(ContextT context, HandlersT&&... handlers) noexcept
			: context_{ std::move(context) }
			, handlers_{ std::forward<HandlersT>(handlers)... }
		{
		}

		using result_t = std::variant<
			typename detail::handler<HandlersT::route, typename HandlersT::fn_type>::result_t...>;

		template<typename... HandlerArgsT>
		constexpr auto operator()(std::string_view path, HandlerArgsT&&... args)
		{
			result_t output;
			cppcoro::detail::for_each(handlers_, [&](auto&& handler) {
				if (auto result = handler(context_, path, std::make_tuple(std::forward<HandlerArgsT>(args)...)); result)
				{
					output = result.value();
					return cppcoro::detail::break_;
				}
				return cppcoro::detail::continue_;
			});
			return output;
		}

	protected:
		handlers_t handlers_;
	};
	// template <typename TupleT, typename...HandlersT>

}  // namespace cppcoro::router
