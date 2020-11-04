/**
 * @file cppcoro/http_request.hpp
 */
#pragma once

#include <cppcoro/http/message.hpp>

namespace cppcoro::http
{
	struct string_request : detail::abstract_message<false, std::span<char>>
	{
		using base = detail::abstract_message<false, std::span<char>>;
		using base::base;
        using base::operator=;

		string_request(
			http::method method,
			std::string&& path,
			std::string&& string,
			http::headers&& headers = {}) noexcept
			: base::abstract_message{ method,
									  std::forward<std::string>(path),
									  std::forward<http::headers>(headers) }
			, _data{ std::forward<std::string>(string) }
		{
			body = std::as_writable_bytes(std::span{ _data });
		}

	private:
		std::string _data;
	};
}  // namespace cppcoro::http
