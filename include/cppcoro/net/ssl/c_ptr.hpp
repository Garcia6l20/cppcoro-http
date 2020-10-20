/** @file cppcoro/net/ssl/c_ptr.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <memory>

namespace cppcoro::detail {

    template <typename T, auto free_func>
    struct c_deleter {
        void operator()(T *ptr) noexcept {
            free_func(ptr);
            delete ptr;
        }
    };

	template <typename T, auto init_func, auto free_func>
	struct c_shared_ptr : std::shared_ptr<T> {
		template <typename...ArgsT>
        explicit c_shared_ptr(T *ptr, ArgsT...args) noexcept : std::shared_ptr<T>{ptr, c_deleter<T, free_func>{}} {
			init_func(ptr, std::forward<ArgsT>(args)...);
		}

        template <typename...ArgsT>
        static auto make(ArgsT &&...args) noexcept {
            return c_shared_ptr{new T, std::forward<ArgsT>(args)...};
        }
	};

    template <typename T, auto init_func, auto free_func>
    struct c_unique_ptr : std::unique_ptr<T, c_deleter<T, free_func>> {
        template <typename...ArgsT>
        explicit c_unique_ptr(T *ptr, ArgsT...args) noexcept : std::unique_ptr<T, c_deleter<T, free_func>>{ptr} {
            init_func(ptr, std::forward<ArgsT>(args)...);
        }

        template <typename...ArgsT>
        static auto make(ArgsT &&...args) noexcept {
            return c_unique_ptr{new T, std::forward<ArgsT>(args)...};
        }
    };
}
