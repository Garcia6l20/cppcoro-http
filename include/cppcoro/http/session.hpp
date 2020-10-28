/** @file cppcoro/http/session.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/net/concepts.hpp>
#include <nlohmann/json.hpp>

namespace cppcoro::http {
	template <typename HttpServerT>
	class session {
	public:
		session(HttpServerT &server) noexcept :
		    server_{server} {
		}

		template<typename T>
        void add_cookie(std::string_view key, T &&value) {
            cookies_[key.data()] = std::forward<T>(value);
		}

		template<typename T>
		T cookie(std::string_view key) {
			return cookies_[key.data()].get<T>();
		}

	private:
		friend HttpServerT;

		void set_cookies(std::string_view raw_cookies) {
			using namespace nlohmann;
            cookies_ = json::parse(raw_cookies);
		}

        std::string get_cookies() {
            using namespace nlohmann;
			return cookies_.dump();
        }

        nlohmann::json cookies_{};

		HttpServerT& server_;
	};
}
