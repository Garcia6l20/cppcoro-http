#pragma once

#include <cppcoro/detail/is_specialization.hpp>
#include <cppcoro/detail/tuple_utils.hpp>

namespace cppcoro
{
	template<typename... ArgsT>
	struct kw_args;

	template<typename, typename>
	struct kw_args_contains_impl;

	template<typename T, typename... ArgsT>
	struct kw_args_contains_impl<T, kw_args<ArgsT...>>
	{
		using kw_args_type = kw_args<ArgsT...>;
		static constexpr bool value =
			cppcoro::detail::tuple_contains<T, std::tuple<ArgsT...>>::value;
	};

	template<typename KwArgsT, typename T>
	concept kw_args_contains = kw_args_contains_impl<T, KwArgsT>::value;

	template<typename... ArgsT>
	struct kw_args : std::tuple<std::optional<ArgsT>...>
	{
		using type = kw_args<ArgsT...>;
		using tuple_type = std::tuple<std::optional<ArgsT>...>;
		//        kw_args_t() noexcept = default;

		template<typename... InitT>
		requires(not(... and detail::specialization_of<InitT, kw_args>))
			kw_args(InitT&&... init) noexcept
			: tuple_type{}
		{
			(std::get<std::optional<std::remove_reference_t<InitT>>>(*this).emplace(
				 std::forward<std::remove_reference_t<InitT>>(init)),
			 ...);
		}

		template<detail::specialization_of<kw_args> OtherT>
		kw_args(OtherT&& other) noexcept
			: tuple_type{}
		{
			//			auto emplace = [&]<typename ArgT>() {
			//				using OptionalT = std::optional<ArgT>;
			//				if constexpr (requires {
			//                    {std::get<OptionalT>(*this).emplace(*std::get<OptionalT>(other))}
			//                    -> std::same_as<OptionalT&>;
			//                  }) {
			//                    std::get<OptionalT>(*this).emplace(*std::get<OptionalT>(other));
			//				}
			//			};
			//			(emplace<InitT>(std::get<InitT>(std::forward<kw_args_t<InitT...>>(other))),
			//...);
			cppcoro::detail::for_each(
				std::forward<OtherT>(other), [&]<typename ItemT>(ItemT&& item) {
					if constexpr (detail::in_tuple<tuple_type, ItemT>)
					{
						emplace_one<ItemT>(std::forward<OtherT>(other));
					}
				});
		}

		template<class... OthersT>
		void emplace(kw_args<OthersT...>&& other) noexcept
		{
			(emplace_one<OthersT>(std::forward<kw_args<OthersT...>>(other)), ...);
		}

		template<typename T, class... OthersT>
		void emplace_one(kw_args<OthersT...>&& other) noexcept
		{
			using OptionalT = std::optional<T>;
			if constexpr (kw_args_contains<kw_args<OthersT...>, T> and kw_args_contains<type, T>)
			{
				if (other.template has_value<T>())
				{
					std::get<OptionalT>(*this).emplace(
						std::get<OptionalT>(std::forward<kw_args<OthersT...>>(other))
							.template value());
				}
			}
		}

		template<typename T>
		constexpr decltype(auto) has_value() noexcept
		{
			using OptionalT = std::optional<T>;
			if constexpr (kw_args_contains<type, T>)
			{
				return bool(std::get<OptionalT>(*this));
			}
			else
			{
				return false;
			}
		}

		template<typename T>
		decltype(auto) value() noexcept
		{
			return std::get<std::optional<T>>(*this).value();
		}

		template<typename T>
		decltype(auto) get() noexcept
		{
			return std::get<std::optional<T>>(*this);
		}

		template<typename T>
		decltype(auto) value_or(T&& or_value = {}) noexcept
		{
			using OptionalT = std::optional<T>;
			if constexpr (requires { { std::get<OptionalT>(std::declval<decltype(*this)>()) }; })
			{
				return std::get<OptionalT>(*this).value_or(std::forward<T>(or_value));
			}
			else
			{
				return std::forward<T>(or_value);
			}
		}
		//        template<cppcoro::detail::specialization_of<std::optional> OptionalT>
		//        decltype(auto) value_or(OptionalT&& or_value = {}) noexcept
		//        {
		//            if constexpr (requires {
		//                {std::get<OptionalT>(std::declval<decltype(*this)>())};
		//            })
		//            {
		//				if (not std::get<OptionalT>(*this)) {
		//                    return
		//                    std::get<OptionalT>(*this).value_or(std::forward<or_value>().value());
		//				} else {
		//                    return std::get<OptionalT>(*this).value_or(std::forward<T>(or_value));
		//				}
		//            } else {
		//                return std::forward<OptionalT>(or_value).value();
		//            }
		//        }
		template<typename... TypesT>
		void set_default_values(TypesT&&... values)
		{
			(set_default_values_one<TypesT>(
				 std::make_tuple(std::forward<std::decay_t<TypesT>>(values))),
			 ...);
		}
		template<typename T, class... TypesT>
		void set_default_values_one(std::tuple<TypesT...>&& tuple) noexcept
		{
			using OptionalT = std::optional<T>;
			if constexpr (requires { { std::get<OptionalT>(std::declval<decltype(*this)>()) }; })
			{
				if (not std::get<OptionalT>(*this))
				{
					std::get<OptionalT>(*this).emplace(
						std::get<T>(std::forward<std::tuple<TypesT...>>(tuple)));
				}
			}
		}
	};
}  // namespace cppcoro
