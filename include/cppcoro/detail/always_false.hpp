#pragma once

namespace cppcoro
{
	template<typename T>
	struct always_false : std::false_type {};

	template<typename T>
	static constexpr auto always_false_v = always_false<T>::value;
}  // namespace cppcoro
