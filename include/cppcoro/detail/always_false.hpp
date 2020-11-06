#pragma once

namespace cppcoro
{
	template<typename T>
	struct always_false
	{
		bool value = false;
	};
	template<typename T>
	constexpr auto always_false_v = always_false<T>::value;
}  // namespace cppcoro
