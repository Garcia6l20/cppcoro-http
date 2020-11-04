/** @file cppcoro/http/url_encode.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <charconv>
#include <ctre.hpp>
#include <fmt/format.h>
#include <optional>
#include <string_view>
#include <cppcoro/net/ip_endpoint.hpp>

namespace cppcoro::net
{
	class uri
	{
        std::string uri_;
	public:
        std::string_view scheme{};
        std::string_view host{};
        std::string_view port{};
        std::string_view path{};
        std::string_view parameters{};

		uri(std::string input) noexcept;

		[[nodiscard]] std::optional<net::ip_endpoint> endpoint() const {
			return net::ip_endpoint::from_string(fmt::format("{}:{}", host, port));
		}

		[[nodiscard]] bool uses_ssl() const noexcept {
            static std::vector ssl_schemes{"https", "wss"};
			return std::ranges::find(ssl_schemes, scheme) != ssl_schemes.end();
		}

		static auto escape(std::string_view input)
		{
			std::string output{};
			output.reserve(input.size());
			for (size_t ii = 0; ii < input.size(); ++ii)
			{
				if (not ctre::match<R"([a-zA-Z0-9])">(std::string_view{ &input[ii], 1 }))
				{
					output.reserve(output.capacity() + 3);
					output.append(fmt::format(FMT_STRING("%{:02X}"), uint8_t(input[ii])));
				}
				else
				{
					output.append(std::string_view{ &input[ii], 1 });
				}
			}
			return output;
		}
        static auto unescape(std::string_view input)
		{
            std::string output{};
			output.reserve(input.size());
			for (size_t ii = 0; ii < input.size(); ++ii)
			{
				if (ctre::match<R"(%[a-zA-Z0-9]{2})">(std::string_view{ &input[ii], 3 }))
				{
					uint8_t c{};
					std::from_chars(&input[ii + 1], &input[ii + 3], c, 16);
					output.append(std::string_view{ reinterpret_cast<char*>(&c), 1 });
					ii += 2;
				}
				else
				{
					output.append(std::string_view{ &input[ii], 1 });
				}
			}
			return output;
		}
	};
}  // namespace cppcoro::http
