/**
 * @file cppcoro/http/htt_message.hpp
 * @author Garcia Sylvain <garcia.6l20@gmail.com>
 */
#pragma once

#include <cppcoro/http/details/static_parser_handler.hpp>
#include <cppcoro/detail/is_specialization.hpp>

#include <fmt/format.h>
#include <span>

namespace cppcoro::http
{
	namespace detail
	{
		enum
		{
			max_body_size = 1024
		};

		struct base_message
		{
			base_message() = default;

			explicit base_message(http::headers&& headers)
				: headers{ std::forward<http::headers>(headers) }
			{
			}

			base_message(base_message const& other) = delete;

			base_message& operator=(base_message const& other) = delete;

			base_message(base_message&& other) = default;

			base_message& operator=(base_message&& other) = default;

			std::optional<std::reference_wrapper<std::string>>
			header(const std::string& key) noexcept
			{
				if (auto it = headers.find(key); it != headers.end())
				{
					return it->second;
				}
				return {};
			}

			http::headers headers;
			byte_span body;

//			virtual bool is_chunked() = 0;
//			virtual std::string build_header() = 0;
			//			virtual task<std::span<std::byte, std::dynamic_extent>> read_body(size_t
			//max_size = max_body_size) = 0; 			virtual task<size_t> write_body(std::span<std::byte,
			//std::dynamic_extent> data) = 0;
		};

		struct base_request : base_message
		{
			static constexpr bool is_request = true;
			static constexpr bool is_response = false;
			using base_message::base_message;

			base_request(base_request&& other) = default;
			base_request& operator=(base_request&& other) = default;

			base_request(http::method method, std::string&& path, http::headers&& headers = {})
				: base_message{ std::forward<http::headers>(headers) }
				, method{ method }
				, path{ std::forward<std::string>(path) }
			{
			}

			http::method method;
			std::string path;

			[[nodiscard]] auto method_str() const
			{
				return http_method_str(static_cast<detail::http_method>(method));
			}

			std::string to_string() const { return fmt::format("{} {}", method_str(), path); }
		};

		struct base_response : base_message
		{
			static constexpr bool is_response = true;
			static constexpr bool is_request = false;
			using base_message::base_message;

			base_response(base_response&& other) = default;
			base_response& operator=(base_response&& other) = default;

			base_response(http::status status, http::headers&& headers = {})
				: base_message{ std::forward<http::headers>(headers) }
				, status{ status }
			{
			}

			http::status status;

			[[nodiscard]] auto status_str() const
			{
				return http_status_str(static_cast<detail::http_status>(status));
			}

			std::string to_string() const { return status_str(); }
		};

		template<bool _is_response, is_body BodyT>
		struct abstract_message : std::conditional_t<_is_response, base_response, base_request>
		{
			using base_type = std::conditional_t<_is_response, base_response, base_request>;
			static constexpr bool is_response = _is_response;
			static constexpr bool is_request = !_is_response;
			using parser_type = http::detail::static_parser_handler<is_request>;

			using base_type::base_type;

			std::optional<async_generator<std::string_view>> chunk_generator_;
			std::optional<async_generator<std::string_view>::iterator> chunk_generator_it_;

			abstract_message(abstract_message::parser_type& parser)
				: base_response{ parser.template status_code_or_method<is_request>(),
								 std::move(parser.headers_), std::move(parser.url()), std::move(parser.header_) }
			{
			}

			abstract_message(
				auto status_or_method,
				http::headers&& headers = {})
				: base_response{ status_or_method, std::forward<http::headers>(headers) }
			{
			}

			abstract_message(
				http::method method,
				std::string&& path,
				http::headers&& headers = {}) requires(is_request)
				: base_request{ method,
								std::forward<std::string>(path),
								std::forward<http::headers>(headers) }
			{
			}

			auto& operator=(parser_type& parser) noexcept requires(is_request)
			{
				this->method = parser.method();
				this->path = std::move(parser.url());
				this->headers = std::move(parser.headers_);
				return *this;
			}

			//            explicit abstract_message(base_type &&base) noexcept :
			//            base_type(std::move(base))  {} abstract_message& operator=(base_type
			//            &&base) noexcept {
			//                static_cast<base_type>(*this) = std::move(base);
			//            }
//
//			bool is_chunked() final
//			{
//				if constexpr (ro_chunked_body<body_type> or wo_chunked_body<body_type>)
//				{
//					return true;
//				}
//				else
//				{
//					return false;
//				}
//			}

			//			task<std::span<std::byte, std::dynamic_extent>> read_body(size_t max_size =
			//max_body_size) final
			//			{
			//				if constexpr (ro_basic_body<BodyT>)
			//				{
			//					co_return std::span{ body_access };
			//				}
			//				else if constexpr (ro_chunked_body<BodyT>)
			//				{
			//					if (not chunk_generator_)
			//					{
			//						chunk_generator_ = body_access.read(max_size);
			//						chunk_generator_it_ = co_await chunk_generator_->begin();
			//						if (*chunk_generator_it_ != chunk_generator_->end())
			//						{
			//							co_return** chunk_generator_it_;
			//						}
			//					}
			//					else if (
			//						chunk_generator_it_ and
			//						co_await ++*chunk_generator_it_ != chunk_generator_->end())
			//					{
			//						co_return** chunk_generator_it_;
			//					}
			//					co_return {};
			//				}
			//			}

			//			task<size_t> write_body(std::span<std::byte, std::dynamic_extent> data)
			//final
			//			{
			//				if constexpr (wo_basic_body<BodyT>)
			//				{
			//					auto size =
			//						std::min(data.size(),
			//std::as_writable_bytes(this->body_access).size()); 					std::memmove(
			//						std::as_writable_bytes(this->body_access).data(), data.data(),
			//size); 					co_return size;
			//				}
			//				else if constexpr (wo_chunked_body<BodyT>)
			//				{
			//					co_return co_await this->body_access.write(data);
			//				}
			//			}

			inline std::string build_header()
			{
				for (auto& [k, v] : this->headers)
				{
					spdlog::debug("- {}: {}", k, v);
				}
				std::string output = _header_base();
				auto write_header = [&output](const std::string& field, const std::string& value) {
					output += fmt::format("{}: {}\r\n", field, value);
				};
				if constexpr (ro_basic_body<BodyT>)
				{
					auto sz = this->body.size();
					if (auto node = this->headers.extract("Content-Length"); !node.empty())
					{
						node.mapped() = std::to_string(sz);
						this->headers.insert(std::move(node));
					}
					else
					{
						this->headers.emplace("Content-Length", std::to_string(sz));
					}
				}
				else if constexpr (ro_chunked_body<BodyT>)
				{
					this->headers.emplace("Transfer-Encoding", "chunked");
				}
				for (auto& [field, value] : this->headers)
				{
					write_header(field, value);
				}
				output += "\r\n";
				return output;
			}

		private:
			inline auto _header_base()
			{
				if constexpr (is_response)
				{
					return fmt::format(
						"HTTP/1.1 {} {}\r\n"
						"UserAgent: cppcoro-http/0.0\r\n",
						int(this->status),
						http_status_str(this->status));
				}
				else
				{
					return fmt::format(
						"{} {} HTTP/1.1\r\n"
						"UserAgent: cppcoro-http/0.0\r\n",
						this->method_str(),
						this->path);
				}
			}
		};

        template<bool response, typename T>
        struct abstract_span_message : detail::abstract_message<response, T> {
            abstract_span_message(auto status_or_method, T span, http::headers &&headers = {}) noexcept
			    : detail::abstract_message<response, T>{status_or_method} {
				this->body = std::as_writable_bytes(span);
			}
		};

	}  // namespace detail


	template<detail::is_body BodyT>
	using abstract_request = detail::abstract_message<false, BodyT>;

	template<detail::is_body BodyT>
	using abstract_response = detail::abstract_message<true, BodyT>;

	template<detail::is_body BodyT>
	using abstract_request = detail::abstract_message<false, BodyT>;

	struct request_parser : detail::static_parser_handler<true>
	{
		using detail::static_parser_handler<true>::static_parser_handler;
	};

	struct response_parser : detail::static_parser_handler<false>
	{
		using detail::static_parser_handler<false>::static_parser_handler;
	};
}  // namespace cppcoro::http