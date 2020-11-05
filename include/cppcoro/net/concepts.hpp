/** @file cppcoro/net/ssl/context.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/concepts.hpp>

namespace cppcoro
{
	class io_service;
    class cancellation_token;
}

namespace cppcoro::net
{
	template <typename T>
	struct connection_message;

    class ip_endpoint;

	enum class connection_mode;

	// clang-format off

	template<typename T>
	concept is_socket = requires(T v)
	{
        { v.listen() };
        { v.bind(std::declval<ip_endpoint>()) };
        { v.connect(std::declval<ip_endpoint>()) } -> awaitable;
        { v.accept(std::declval<T&>()) } -> awaitable;
		{ v.recv(std::declval<void*>(), size_t{}) } -> awaitable<size_t>;
		{ v.send(std::declval<const void*>(), size_t{}) } -> awaitable<size_t>;
	};

    template<typename T>
    concept is_cancelable_socket = is_socket<T> and requires(T v)
    {
        { v.connect(std::declval<ip_endpoint>(), std::declval<cancellation_token>()) } -> awaitable;
        { v.accept(std::declval<T&>(), std::declval<cancellation_token>()) } -> awaitable;
//        { v.recv(std::declval<void*>(), size_t{}, cancellation_token{}) } -> awaitable<size_t>;
//        { v.send(std::declval<const void*>(), size_t{}, cancellation_token{}) } -> awaitable<size_t>;
    };

    template<typename T>
    concept is_connection = requires(T obj)
    {
        typename T::socket_type;
        typename T::template message_type<>;
        { T::connection_mode } -> std::convertible_to<net::connection_mode>;
        { obj.socket() } -> is_socket;
        { obj.token() } -> std::same_as<cancellation_token>;
    };

    template <typename T>
    concept is_connection_socket_provider = requires(T) {
        typename T::connection_socket_type;
        (net::is_cancelable_socket<typename T::connection_socket_type>);
        { T::create_connection_sock(std::declval<io_service&>()) } -> std::same_as<typename T::connection_socket_type>;
    };

    template <typename T>
    concept is_socket_provider = is_connection_socket_provider<T> and requires(T v) {
        typename T::listening_socket_type;
        (net::is_cancelable_socket<typename T::listening_socket_type>);
        { T::create_listening_sock(std::declval<io_service&>()) } -> std::same_as<typename T::listening_socket_type>;
    };
	// clang-format on

}  // namespace cppcoro::net
