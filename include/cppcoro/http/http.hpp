/**
 * @file cppcoro/http.hpp
 */
#pragma once

#include <map>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace cppcoro::http {

    namespace detail {
        #include <http_parser.h>
    }

    enum class method
    {
        get = detail::HTTP_GET,
        put = detail::HTTP_PUT,
        del = detail::HTTP_DELETE,
        post = detail::HTTP_POST,
        head = detail::HTTP_HEAD,
        options = detail::HTTP_OPTIONS,
        patch = detail::HTTP_PATCH,
        unknown
    };

    using status = detail::http_status;
    using headers = std::map<std::string, std::string>;

    namespace logging {
        static inline spdlog::level_t log_level = spdlog::level::warn;
        constexpr auto logger_name = "cppcoro::http";
        inline static auto logger = [] () -> std::shared_ptr<spdlog::logger> {
            if (auto logger = spdlog::get(logger_name);
                logger) {
                return logger;
            }
            return spdlog::stdout_color_mt(logger_name);
        }();
        template <typename IdT>
        auto logger_id(IdT &&id) {
            return fmt::format("{}::{}", logger_name, std::forward<IdT>(id));
        }
        template <typename IdT>
        auto get_logger(IdT &&id) {
            const auto _id = logger_id(std::forward<IdT>(id));
            if (auto _logger = spdlog::get(_id);
                _logger) {
                return _logger;
            }
            auto &sinks = logging::logger->sinks();
            auto this_logger = std::make_shared<spdlog::logger>(_id, begin(sinks), end(sinks));
            this_logger->set_level(spdlog::level::level_enum(log_level.load(std::memory_order_relaxed)));
            return this_logger;
        }
        template <typename IdT>
        auto drop_logger(IdT &&id) {
            spdlog::drop(logger_id(std::forward<IdT>(id)));
        }
    }
}
