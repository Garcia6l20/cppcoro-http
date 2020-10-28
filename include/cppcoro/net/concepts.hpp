/** @file cppcoro/net/ssl/context.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/cancellation_token.hpp>
#include <cppcoro/io_service.hpp>
#include <cppcoro/concepts.hpp>
#include <cppcoro/net/ip_endpoint.hpp>

namespace cppcoro {
	class io_service;
}

namespace cppcoro::net
{
	// clang-format off

	template<typename T>
	concept is_socket = requires(T v)
	{
        { v.listen() };
        { v.bind(ip_endpoint{}) };
        { v.connect(ip_endpoint{}) } -> awaitable;
        { v.accept(std::declval<T&>()) } -> awaitable;
		{ v.recv(std::declval<void*>(), size_t{}) } -> awaitable<size_t>;
		{ v.send(std::declval<const void*>(), size_t{}) } -> awaitable<size_t>;
	};

    template<typename T>
    concept is_cancelable_socket = is_socket<T> and requires(T v)
    {
        { v.connect(ip_endpoint{}, cancellation_token{}) } -> awaitable;
        { v.accept(std::declval<T&>(), cancellation_token{}) } -> awaitable;
//        { v.recv(std::declval<void*>(), size_t{}, cancellation_token{}) } -> awaitable<size_t>;
//        { v.send(std::declval<const void*>(), size_t{}, cancellation_token{}) } -> awaitable<size_t>;
    };

    template <typename T>
    concept is_connection_socket_provider = requires(T v) {
        { v.create_connection_sock(std::declval<io_service&>()) } -> net::is_cancelable_socket;
    };

    template <typename T>
    concept is_socket_provider = is_connection_socket_provider<T> and requires(T v) {
        { v.create_listening_sock(std::declval<io_service&>()) } -> net::is_cancelable_socket;
    };
	// clang-format on

}  // namespace cppcoro::net
