/** @file cppcoro/http/url_encode.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <charconv>
#include <ctre.hpp>
#include <fmt/format.h>
#include <string_view>

namespace cppcoro::http
{
	namespace uri
	{
		auto escape(std::string_view input)
		{
			std::string output{};
			output.reserve(input.size());
			for (size_t ii = 0; ii < input.size(); ++ii)
			{
				if (not ctre::match<R"([a-zA-Z0-9])">(std::string_view{ &input[ii], 1 }))
				{
					output.reserve(output.capacity() + 3);
					output.append(fmt::format(FMT_STRING("%{:02X}"), input[ii]));
				}
				else
				{
					output.append(std::string_view{ &input[ii], 1 });
				}
			}
			return output;
		}
		auto unescape(std::string_view input)
		{
            std::string output{};
			output.reserve(input.size());
			for (size_t ii = 0; ii < input.size(); ++ii)
			{
				if (ctre::match<R"(%[a-zA-Z0-9]{2})">(std::string_view{ &input[ii], 3 }))
				{
					char c{};
					std::from_chars(&input[ii + 1], &input[ii + 3], c, 16);
					output.append(std::string_view{ &c, 1 });
					ii += 2;
				}
				else
				{
					output.append(std::string_view{ &input[ii], 1 });
				}
			}
			return output;
		}
	}  // namespace uri
}  // namespace cppcoro::http
