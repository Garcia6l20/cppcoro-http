/**
 *
 */
#pragma once

#include <type_traits>

namespace cppcoro::detail
{
	// template specialization detection

	template<class T, template<class...> class Template>
	struct is_specialization : std::false_type
	{
	};

    template<template<class...> class Template, class... Args>
    struct is_specialization<Template<Args...>, Template> : std::true_type
    {
    };


	template<class T, template<class...> class Template>
	concept specialization_of = is_specialization<std::decay_t<T>, Template>::value;
}  // namespace cppcoro::detail
