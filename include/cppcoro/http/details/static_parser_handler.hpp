#pragma once

#include <cppcoro/async_generator.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/net/uri.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/detail/c_ptr.hpp>

#include <fmt/format.h>

#include <concepts>
#include <memory>
#include <span>

namespace cppcoro::net
{
	enum class message_direction;
	enum class connection_mode;
}  // namespace cppcoro::net

namespace cppcoro::http
{
	using byte_span = std::span<std::byte, std::dynamic_extent>;

	template<bool is_request>
	struct message_header;

	template<net::is_socket, net::message_direction, net::connection_mode>
	struct message;
}  // namespace cppcoro::http

namespace cppcoro::http::detail
{
	template<detail::http_parser_type type, typename T>
	void init_parser(detail::http_parser* parser, T* owner)
	{
		detail::http_parser_init(parser, type);
		parser->data = owner;
	}

	template<detail::http_parser_type type, typename OwnerT>
	using c_parser_ptr =
		cppcoro::detail::c_unique_ptr<detail::http_parser, init_parser<type, OwnerT>>;

	// clang-format off
	template<typename BodyT>
	concept ro_chunked_body = requires(BodyT&& body)
	{
		{ body.read(size_t(0)) } -> std::same_as<async_generator<std::string_view>>;
	};
	template<typename BodyT>
	concept wo_chunked_body = requires(BodyT&& body)
	{
		{ body.write(std::string_view{}) } ->std::same_as<task<size_t>>;
	};

	template<typename BodyT>
	concept chunked_body = ro_chunked_body<BodyT>and wo_chunked_body<BodyT>;

	template<typename BodyT>
	concept ro_basic_body = requires(BodyT& body)
	{
		{ std::as_bytes(body) };
	};

    template<typename T>
    concept is_const = std::is_const_v<T>;

	template<typename BodyT>
	concept wo_basic_body = requires(BodyT&& body)
	{
        { std::as_writable_bytes(body) };
	} or std::same_as<BodyT, std::string>;
	template<typename BodyT>
	concept basic_body = ro_basic_body<BodyT>and wo_basic_body<BodyT>;

	template<typename BodyT>
	concept readable_body = ro_basic_body<BodyT> or ro_chunked_body<BodyT>;

	template<typename BodyT>
	concept writeable_body = wo_basic_body<BodyT> or wo_chunked_body<BodyT>;

	template<typename BodyT>
	concept is_body = readable_body<BodyT> or writeable_body<BodyT>;

	// clang-format on

	template<bool is_request>
	class static_parser_handler
	{
		using self_type = static_parser_handler<is_request>;

		static constexpr auto c_parser_type = []() {
			if constexpr (is_request)
			{
				return detail::http_parser_type::HTTP_REQUEST;
			}
			else
			{
				return detail::http_parser_type::HTTP_RESPONSE;
			}
		}();

		using parser_ptr = c_parser_ptr<c_parser_type, static_parser_handler>;

	public:
		using method_or_status_t = std::conditional_t<is_request, http::method, http::status>;

		static_parser_handler() = default;
		static_parser_handler(static_parser_handler&& other) noexcept
			: parser_{ std::move(other.parser_) }
			, header_field_{ std::move(other.header_field_) }
			, url_{ std::move(other.url_) }
			, body_{ std::move(other.body_) }
			, state_{ std::move(other.state_) }
			, headers_{ std::move(other.headers_) }
		{
			parser_->data = this;
		}
		//        status state_{ status::none };
		//        std::string header_field_;
		//        std::string url_;
		//        byte_span body_;
		//        http::headers headers_;
		static_parser_handler& operator=(static_parser_handler&& other) noexcept
		{
			parser_ = std::move(other.parser_);
			header_field_ = std::move(other.header_field_);
			url_ = std::move(other.url_);
			body_ = std::move(other.body_);
            state_ = std::move(other.state_);
			headers_ = std::move(other.headers_);
			parser_->data = this;
			return *this;
		}
		static_parser_handler(const static_parser_handler&) noexcept = delete;
		static_parser_handler& operator=(const static_parser_handler&) noexcept = delete;

		std::optional<size_t> content_length() const noexcept
		{
			//			return parser_->content_length ==
			//					std::numeric_limits<decltype(parser_->content_length)>::max()
			//				? std::optional<size_t>{}
			//				: parser_->content_length;
			if (headers_.contains("Content-Length"))
			{
				auto it = headers_.find("Content-Length");
				std::span view{ it->second };
				size_t sz = 0;
				auto [ptr, error] =
					std::from_chars(view.data(), view.data() + view.size_bytes(), sz);
				assert(error == std::errc{});
				return sz;
			}
			else
			{
				return {};
			}
		}

		bool header_done() const noexcept { return state_ >= status::on_headers_complete; }
		bool has_body() const noexcept { return body_.size(); }
		auto body()
		{
			auto body = body_;
			body_ = {};
			return body;
		}

		bool chunked() const { return state_ != status::on_message_complete && has_body(); }

		operator bool() const { return state_ == status::on_message_complete; }

		template<typename T, size_t extent = std::dynamic_extent>
		bool parse(std::span<T, extent> data)
		{
			body_ = {};
			const auto count = execute_parser(
				reinterpret_cast<const char*>(std::as_bytes(data).data()), data.size_bytes());
			if (count < data.size_bytes())
			{
				throw std::runtime_error{ fmt::format(
					FMT_STRING("parse error: {}"),
					http_errno_description(detail::http_errno(parser_->http_errno))) };
			}
			return *this;
		}

		bool parse(std::string_view input) { return parse(input.data(), input.size()); }

		auto method() const { return static_cast<http::method>(parser_->method); }
		auto status_code() const { return static_cast<http::status>(parser_->status_code); }

		method_or_status_t status_code_or_method() const
		{
			if constexpr (is_request)
			{
				return method();
			}
			else
			{
				return status_code();
			}
		}

		//		template<typename MessageT>
		//		task<> load(MessageT& message)
		//		{
		//			static_assert(is_request == MessageT::is_request);
		//			if constexpr (is_request)
		//			{
		//				message.method = method();
		//				message.path = url_;
		//			}
		//			else
		//			{
		//				message.status = status_code();
		//			}
		//			if (!this->body_.empty())
		//			{
		//				co_await message.write_body(body_);
		//			}
		//		}

		const auto& url() const { return url_; }

		auto& url() { return url_; }

		std::string to_string() const
		{
			fmt::memory_buffer out;
			std::string_view type;
			if constexpr (is_request)
			{
				fmt::format_to(
					out,
					"request {} {}",
					detail::http_method_str(detail::http_method(parser_->method)),
					url_);
			}
			else
			{
				fmt::format_to(
					out,
					"response {} ",
					detail::http_status_str(detail::http_status(parser_->status_code)));
			}
			fmt::format_to(out, "{}", body_);
			return out.data();
		}

	protected:
		enum class status
		{
			none,
			on_message_begin,
			on_url,
			on_status,
			on_header_field,
			on_header_value,
			on_headers_complete,
			on_body,
			on_message_complete,
			on_chunk_header,
			on_chunk_header_compete,
		};

		inline static auto& instance(detail::http_parser* parser)
		{
			return *static_cast<self_type*>(parser->data);
		}

		static inline int on_message_begin(detail::http_parser* parser)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_message_begin;
			return 0;
		}

		static inline int on_url(detail::http_parser* parser, const char* data, size_t len)
		{
			auto& this_ = instance(parser);
			this_.url_ = net::uri::unescape({ data, len });
			this_.state_ = status::on_url;
			return 0;
		}

		static inline int on_status(detail::http_parser* parser, const char* data, size_t len)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_status;
			return 0;
		}

		static inline int on_header_field(detail::http_parser* parser, const char* data, size_t len)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_header_field;
			this_.header_field_ = { data, len };
			return 0;
		}

		static inline int on_header_value(detail::http_parser* parser, const char* data, size_t len)
		{
			auto& this_ = instance(parser);
			if (this_.state_ == status::on_header_field)
			{
				this_.headers_.emplace(this_.header_field_, std::string{ data, data + len });
			}
			else
			{
				// header has been cut
				auto it = this_.headers_.find(this_.header_field_);
				assert(it != this_.headers_.end());
				it->second.append(std::string_view{ data, data + len });
			}
			this_.state_ = status::on_header_value;

			return 0;
		}

		static inline int on_headers_complete(detail::http_parser* parser)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_headers_complete;
			return 0;
		}

		static inline int on_body(detail::http_parser* parser, const char* data, size_t len)
		{
			auto& this_ = instance(parser);
			this_.body_ = std::as_writable_bytes(std::span{ const_cast<char*>(data), len });
			this_.state_ = status::on_body;
			return 0;
		}

		static inline int on_message_complete(detail::http_parser* parser)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_message_complete;
			return 0;
		}

		static inline int on_chunk_header(detail::http_parser* parser)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_chunk_header;
			return 0;
		}

		static inline int on_chunk_complete(detail::http_parser* parser)
		{
			auto& this_ = instance(parser);
			this_.state_ = status::on_chunk_header_compete;
			return 0;
		}

		auto execute_parser(const char* data, size_t len)
		{
			return http_parser_execute(parser_.get(), &http_parser_settings_, data, len);
		}

	private:
		parser_ptr parser_ = parser_ptr::make(this);
		inline static detail::http_parser_settings http_parser_settings_ = {
			on_message_begin,    on_url,  on_status,           on_header_field, on_header_value,
			on_headers_complete, on_body, on_message_complete, on_chunk_header, on_chunk_complete,
		};
		status state_{ status::none };
		std::string header_field_;
		std::string url_;
		byte_span body_;
		http::headers headers_;

		template<bool _is_response, is_body BodyT>
		friend struct abstract_message;

		template<net::is_socket, net::message_direction, net::connection_mode>
		friend struct cppcoro::http::message;

		template<typename, bool>
		friend struct cppcoro::http::message_header;
	};
}  // namespace cppcoro::http::detail