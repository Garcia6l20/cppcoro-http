#pragma once

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <string>
#include <vector>

#include <ctll.hpp>

namespace cppcoro::http
{
	template<typename T>
	struct route_parameter;

	template<>
	struct route_parameter<std::string>
	{
		static constexpr int group_count() { return 0; }
		static std::string load(const std::string_view& input)
		{
			return { input.data(), input.data() + input.size() };
		}

		static constexpr auto pattern = ctll::fixed_string{ R"(.+(?=/)|.+)" };
	};

	template<>
	struct route_parameter<std::string_view>
	{
		static constexpr int group_count() { return 0; }
		static std::string_view load(const std::string_view& input)
		{
			return { input.data(), input.size() };
		}

		static constexpr auto pattern = ctll::fixed_string{ R"(.+(?=/)|.+)" };
	};

	template<>
	struct route_parameter<std::filesystem::path>
	{
		static constexpr int group_count() { return 0; }
		static std::filesystem::path load(const std::string_view& input)
		{
			return { input.data() };
		}
		static constexpr auto pattern = ctll::fixed_string{ R"(.+)" };
	};

	template<>
	struct route_parameter<int>
	{
		static constexpr int group_count() { return 0; }
		static int load(const std::string_view& input)
		{
			int result = 0;
			std::from_chars(input.data(), input.data() + input.size(), result);
			return result;
		}
		static constexpr auto pattern = ctll::fixed_string{ R"(\d+)" };
	};

	template<>
	struct route_parameter<double>
	{
		static constexpr int group_count() { return 0; }
		static double load(const std::string_view& input)
		{
			double result = 0;
#if not CPPCORO_COMPILER_GCC or CPPCORO_COMPILER_GCC > 10'02'00
            std::from_chars(input.data(), input.data() + input.size(), result);
#else
            char* pend;
            result = std::strtod(input.data(), &pend);
#endif
			return result;
		}
		static constexpr auto pattern = ctll::fixed_string{ R"(\d+\.?\d*)" };
	};

	template<>
	struct route_parameter<bool>
	{
		static bool load(const std::string_view& input)
		{
			static const std::vector<std::string> true_values = { "yes", "on", "true" };
			return std::any_of(true_values.begin(), true_values.end(), [&input](auto&& elem) {
				return std::equal(
					elem.begin(),
					elem.end(),
					input.begin(),
					input.end(),
					[](char left, char right) { return tolower(left) == tolower(right); });
			});
		}
		static constexpr auto pattern = ctll::fixed_string{ R"(\w+)" };
	};

}  // namespace cppcoro::http
