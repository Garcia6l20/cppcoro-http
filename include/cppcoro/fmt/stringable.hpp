#pragma once

#include <fmt/format.h>

#include <cppcoro/net/ip_endpoint.hpp>

namespace fmt {

    namespace concepts {
        template<typename T>
        concept stringable = requires(const T &v) {
            { v.to_string() } -> std::convertible_to<std::string>;
        };
    }

    template <concepts::stringable StringableT>
    struct formatter<StringableT> : fmt::formatter<std::string> {
        template <typename FormatContext>
        auto format(const StringableT &obj, FormatContext& ctx) {
            return formatter<std::string>::format(obj.to_string(), ctx);
        }
    };
}
