#pragma once

#include <fmt/format.h>

#include <span>


template <typename T, size_t extent>
struct fmt::formatter<std::span<T, extent>> : fmt::formatter<uint8_t>
{
    // Presentation format: 'x' - lower case alphanumeric numbers, 'X' - upper case alphanumeric numbers.
    char presentation = 'x';

    // Parses format specifications of the form ['f' | 'e'].
    constexpr auto parse(format_parse_context& ctx) {
        // auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) // c++11
        // [ctx.begin(), ctx.end()) is a character range that contains a part of
        // the format string starting from the format specifications to be parsed,
        // e.g. in
        //
        //   fmt::format("{:f} - point of interest", point{1, 2});
        //
        // the range will contain "f} - point of interest". The formatter should
        // parse specifiers until '}' or the end of the range. In this example
        // the formatter should parse the 'f' specifier and return an iterator
        // pointing to '}'.

        // Parse the presentation format and store it in the formatter:
        auto it = ctx.begin(), end = ctx.end();
        if (it != end && (*it == 'x' || *it == 'X')) presentation = *it++;

        // Check if reached the end of the range:
        if (it != end && *it != '}')
            throw format_error("invalid format");

        // Return an iterator past the end of the parsed range:
        return it;
    }

    // Formats the point p using the parsed format specification (presentation)
    // stored in this formatter.
    template <typename FormatContext>
    auto format(const std::span<T, extent>& p, FormatContext& ctx) {
        // auto format(const point &p, FormatContext &ctx) -> decltype(ctx.out()) // c++11
        // ctx.out() is an output iterator to write to.
		if (p.size() == 0)
		    return ctx.out();

		decltype(fmt::formatter<uint8_t>::format(std::declval<uint8_t&>(), std::declval<FormatContext&>())) output;
		auto item_size = p.size_bytes() / p.size();
		for (size_t ii = 0; ii < p.size(); ++ii) {
			for (size_t jj = 0; jj < item_size; ++jj) {
                output = fmt::formatter<uint8_t>::format(*reinterpret_cast<const uint8_t*>(std::as_bytes(p).data() + ii + jj), ctx);
			}
		}
        return output;
    }
};
