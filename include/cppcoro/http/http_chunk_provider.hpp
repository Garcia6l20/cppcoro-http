/**
 * @file cppcoro/http/http_chunk_provider.hpp
 * @author Garcia Sylvain <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/io_service.hpp>
#include <cppcoro/async_generator.hpp>
#include <cppcoro/read_only_file.hpp>
#include <cppcoro/write_only_file.hpp>

#include <cppcoro/http/http_message.hpp>

namespace cppcoro::http {

    /**
     * @brief Abstract chunk base.
     *
     * Implements base requirements for chunk providers/processors (default service ctor, move ctor & assignment op).
     */
    class abstract_chunk_base
    {
        std::reference_wrapper<io_service> service_;
    public:
        // needed for default construction
        abstract_chunk_base(io_service &service) noexcept: service_{service} {}

        abstract_chunk_base(abstract_chunk_base &&other) noexcept = default;

        abstract_chunk_base &operator=(abstract_chunk_base &&other) noexcept = default;

    protected:
        auto &service() { return service_; }
    };

    /**
     * @brief Read only file chunk provider.
     *
     * Chunk provider implementation for read_only_file access.
     */
    struct read_only_file_chunk_provider : http::abstract_chunk_base
    {
        using abstract_chunk_base::abstract_chunk_base;

        std::string path_;

        read_only_file_chunk_provider(io_service &service, std::string_view path) noexcept:
            abstract_chunk_base{service}, path_{path} {}

        async_generator<std::string_view> read(size_t chunk_size) {
            if (!path_.empty()) {
                auto f = read_only_file::open(service(), path_);
                std::string buffer;
                buffer.resize(chunk_size);
                uint64_t offset = 0;
                auto to_send = f.size();
                size_t res;
                do {
                    res = co_await f.read(offset, buffer.data(), chunk_size);
                    to_send -= res;
                    offset += res;
                    co_yield std::string_view{buffer.data(), res};
                } while (to_send);
            }
        }
    };

    static_assert(std::constructible_from<read_only_file_chunk_provider, io_service &>);
    static_assert(http::detail::ro_chunked_body<read_only_file_chunk_provider>);

    using read_only_file_chunked_response = http::abstract_response<read_only_file_chunk_provider>;
    using read_only_file_chunked_request = http::abstract_request<read_only_file_chunk_provider>;

    /**
     * @brief Write only file chunk processor.
     *
     * Chunk processor implementation for write_only_file access.
     */
    struct write_only_file_processor : abstract_chunk_base
    {
        using abstract_chunk_base::abstract_chunk_base;

        void init(std::string_view path) {
            file_.emplace(write_only_file::open(service(), path, file_open_mode::create_always));
        }

        std::optional<write_only_file> file_;
        size_t offset = 0;

        task<size_t> write(std::string_view chunk) {
            assert(file_);
            auto size = co_await file_->write(offset, chunk.data(), chunk.size());
            offset += size;
            co_return size;
        }
    };


    static_assert(std::constructible_from<write_only_file_processor, io_service &>);
    static_assert(detail::wo_chunked_body<write_only_file_processor>);

    using write_only_file_chunked_response = abstract_response<write_only_file_processor>;
    using write_only_file_chunked_request = abstract_request<write_only_file_processor>;
}
