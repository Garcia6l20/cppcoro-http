/** @file cppcoro/http/session.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/concepts.hpp>
#include <nlohmann/json.hpp>

#include <ranges>

namespace cppcoro::http {
	template <is_config ConfigT>
	class session {
	public:
		session(http::server<ConfigT> &server) noexcept :
		    server_{server} {
		}

		template<typename T>
        void add_cookie(std::string_view key, T &&value) {
            cookies_[key.data()] = std::forward<T>(value);
		}

		template<typename T>
		T cookie(std::string_view key, std::optional<T> default_value = {}) {
			if (default_value and not cookies_.contains(key)) {
				cookies_[key.data()] = std::move(*default_value);
			}
			return cookies_[key.data()].get<T>();
		}

		void extract_cookies(const http::headers& headers) {
			using namespace nlohmann;
			for (auto &[_, raw_cookie] : headers | std::views::filter([] (auto const& elem) {
											 return elem.first == "Cookie";
										 })) {
				auto rng = std::views::split(raw_cookie, '=') | std::views::transform([](auto &&rng) {
							   return std::string_view(&*rng.begin(), std::ranges::distance(rng));
						   });
				std::string k{*next(begin(rng), 0)};
                std::string v{*next(begin(rng), 1)};
//                std::string v = *next(begin(rng), 1);
				cookies_[k] = v;
			}
		}

        void load_cookies(http::headers& headers) {
            using namespace nlohmann;
			for (auto &[k, v] : cookies_.items()) {
				headers.emplace("Set-Cookie", fmt::format(FMT_STRING("{}={}"), k, v));
			}
        }

        nlohmann::json cookies_{};

        http::server<ConfigT>& server_;
	};
}
