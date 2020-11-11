#pragma once

#include <cppcoro/fmt/span.hpp>
#include <cppcoro/http/concepts.hpp>
#include <cppcoro/http/http.hpp>
#include <cppcoro/http/request.hpp>
#include <cppcoro/http/response.hpp>
#include <cppcoro/net/message.hpp>
#include <cppcoro/net/tcp.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/when_all.hpp>
#include <cppcoro/detail/is_specialization.hpp>

#include <cppcoro/fmt/stringable.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cppcoro/ssl/socket.hpp>
#include <charconv>
#include <cstring>

namespace cppcoro::http
{
	template<net::is_connection_socket_provider>
	class client;

	template<is_config>
	class server;

	template<bool is_request>
	struct message_header
	{
		using parser_t = detail::static_parser_handler<is_request>;
		using method_or_status_t = typename parser_t::method_or_status_t;

		message_header(method_or_status_t& method_or_status, http::headers&& headers = {}) noexcept
			: _method_or_status{ method_or_status }
			, headers{ std::forward<http::headers>(headers) }
		{
		}

		message_header(
			method_or_status_t& method,
			std::string_view path,
			http::headers&& headers = {}) noexcept
			: _method_or_status{ method }
			, path{ path }
			, headers{ std::forward<http::headers>(headers) }
		{
		}

		message_header(parser_t& parser)
		{
			_method_or_status = parser.status_code_or_method();
			content_length = parser.content_length();  // before moving headers
			headers = std::move(parser.headers_);

			if constexpr (is_request)
			{
				path = parser.url();
			}
		}

		method_or_status_t _method_or_status;
		method_or_status_t& method{ _method_or_status };
		method_or_status_t& status{ _method_or_status };

		std::optional<size_t> content_length{};
		std::optional<std::string> path{};

		http::headers headers;

		bool chunked = false;

		inline auto _header_base()
		{
			if constexpr (is_request)
			{
				assert(this->path);
				return fmt::format(
					"{} {} HTTP/1.1\r\n"
					"UserAgent: cppcoro-http/0.0\r\n",
					http::detail::http_method_str(http::detail::http_method(this->method)),
					this->path.value());
			}
			else
			{
				return fmt::format(
					"HTTP/1.1 {} {}\r\n"
					"UserAgent: cppcoro-http/0.0\r\n",
					int(this->status),
					http::detail::http_status_str(http::detail::http_status(this->status)));
			}
		}

		inline std::string build()
		{
			for (auto& [k, v] : this->headers)
			{
				spdlog::debug("- {}: {}", k, v);
			}
			std::string output = _header_base();
			auto write_header = [&output](const std::string& field, const std::string& value) {
				output += fmt::format("{}: {}\r\n", field, value);
			};
			if (chunked)
			{
				this->headers.emplace("Transfer-Encoding", "chunked");
			}
			else if (content_length)
			{
				std::array<char, 64> content_length_str{};
				auto [ptr, error] = std::to_chars(
					begin(content_length_str), end(content_length_str), *content_length);
				assert(error == std::errc{});
				this->headers.emplace(
					"Content-Length", std::string_view{ content_length_str.data(), ptr });
			}
			for (auto& [field, value] : this->headers)
			{
				write_header(field, value);
			}
			output += "\r\n";
			return output;
		}
	};

	template<net::is_socket SocketT, net::message_direction direction, net::connection_mode mode>
	struct message;

	// tx message
	template<net::is_socket SocketT, net::connection_mode mode>
	struct message<SocketT, net::message_direction::outgoing, mode>
		: protected net::message<SocketT, net::message_direction::outgoing>
	{
		using base = net::message<SocketT, net::message_direction::outgoing>;
		using base::base;
		//		using base::receive;
		//		using base::send;

		static constexpr bool is_request = mode == net::connection_mode::client;
		static constexpr bool is_response = mode == net::connection_mode::server;

		using method_or_status_t = std::conditional_t<is_request, http::method, http::status>;
		using parser_t = detail::static_parser_handler<is_request>;

		using header_type = message_header<is_request>;

		header_type make_header(method_or_status_t method_or_status, http::headers&& headers = {})
		{
			return header_type{ method_or_status, std::forward<http::headers>(headers) };
		}

		task<size_t> send(header_type&& header)
		{
			header_data_ = std::forward<header_type>(header).build();
			return base::send(std::as_writable_bytes(std::span{ header_data_ }));
		}

		task<size_t> send() { return send(this->bytes_); }

		template<typename T, size_t extent = std::dynamic_extent>
		task<size_t> send(std::span<T, extent> data)
		{
			if (chunked)
			{
				auto size_str = fmt::format("{:x}\r\n", data.size_bytes());
				co_await base::send(std::as_bytes(std::span{ size_str }));
				if (data.size_bytes())
				{
					co_await base::send(std::as_bytes(data));
				}
				co_return co_await base::send(
					std::as_bytes(std::span{ std::string_view{ "\r\n" } }));
			}
			else
			{
				co_return co_await base::send(std::as_bytes(data));
			}
		}

		void init(header_type hdr)
		{
			if (hdr.headers.contains("Transfer-Encoding"))
			{
				auto it = hdr.headers.find("Transfer-Encoding");
				chunked = it->second == "chunked";
			}
		}

		task<> begin_message(
			method_or_status_t method,
			std::string_view path,
			size_t size,
			http::headers&& hdrs = {}) requires(is_request)
		{
			header_type hdr{ method, path, std::forward<http::headers>(hdrs) };
			hdr.content_length = size;
			init(hdr);
			co_await send(std::move(hdr));
		}

		task<> begin_message(
			method_or_status_t method,
			std::string_view path,
			http::headers&& hdrs = {}) requires(is_request)
		{
			header_type hdr{ method, path, std::forward<http::headers>(hdrs) };
			init(hdr);
			co_await send(std::move(hdr));
		}

		task<> begin_message(
			method_or_status_t status, size_t size, http::headers&& hdrs = {}) requires(is_response)
		{
			header_type hdr{ status, std::forward<http::headers>(hdrs) };
			hdr.content_length = size;
			init(hdr);
			co_await send(std::move(hdr));
		}

		task<>
		begin_message(method_or_status_t status, http::headers&& hdrs = {}) requires(is_response)
		{
			header_type hdr{ status, std::forward<http::headers>(hdrs) };
			init(hdr);
			co_await send(std::move(hdr));
		}

		bool chunked = false;

	private:
		parser_t parser_;
		std::string header_data_;
	};
	static_assert(
		net::has_begin_message<
			message<net::socket, net::message_direction::outgoing, net::connection_mode::client>,
			http::method,
			std::string_view,
			size_t>);

	// rx message
	template<net::is_socket SocketT, net::connection_mode mode>
	struct message<SocketT, net::message_direction::incoming, mode>
		: protected net::message<SocketT, net::message_direction::incoming>
	{
		using base = net::message<SocketT, net::message_direction::incoming>;
		using base::base;

		static constexpr bool is_request = mode == net::connection_mode::server;
		static constexpr bool is_response = mode == net::connection_mode::client;

		using method_or_status_t = std::conditional_t<is_request, http::method, http::status>;
		using parser_t = detail::static_parser_handler<is_request>;

		using header_type = message_header<is_request>;

		header_type make_header(method_or_status_t method_or_status, http::headers&& headers = {})
		{
			return header_type{ method_or_status, std::forward<http::headers>(headers) };
		}

		task<> begin_message() { co_await receive_header(); }

		task<net::readable_bytes> receive()
		{
			if (parser_.has_body())
			{
				co_return parser_.body();;
			}
			else if (parser_ or (not chunked and not remaining_length))
			{
				co_return net::readable_bytes{};
			}
			else if (chunked)
			{
				auto bytes_received = co_await base::receive();
				parser_.parse(std::span{ this->bytes_.data(), bytes_received });
				if (parser_.has_body())
				{
					co_return parser_.body();
				}
				else
				{
					co_return net::readable_bytes{};
				}
			}
			else
			{
				auto size = std::min(remaining_length, this->bytes_.size_bytes());
//				spdlog::debug(
//					">>>>>>>>>> http::{}_connection::receiving {} bytes...",
//					mode == net::connection_mode::client ? "client" : "server",
//					size);
				auto bytes_received = co_await base::receive(size);
				parser_.parse(std::span{ this->bytes_.data(), bytes_received });
				remaining_length -= bytes_received;
//				spdlog::debug(
//					">>>>>>>>>> http::{}_connection::receive: {}/{}/{}",
//					mode == net::connection_mode::client ? "client" : "server",
//					size,
//					remaining_length,
//					*content_length);
				if (parser_.has_body())
				{
					co_return parser_.body();
				}
				else
				{
					co_return net::readable_bytes{};
				}
			}
		}

		auto& operator[](const std::string& key) { return parser_[key]; }

		std::optional<size_t> content_length{};
		bool chunked = false;
		std::string path{};

	private:
		task<> receive_header()
		{
//			spdlog::debug(
//				">>>>>>>>>> http::{}_connection::receive_header...",
//				mode == net::connection_mode::client ? "client" : "server");
			size_t bytes_received;
			do
			{
				bytes_received = co_await base::receive();
				parser_.parse(std::span{ this->bytes_.data(), bytes_received });
			} while (not parser_.header_done());

			content_length = parser_.content_length();
			if (parser_.chunked())
			{
				chunked = true;
			}
			else if (content_length)
			{
				remaining_length = *content_length;
			}
			path = parser_.url();
            remaining_length -= parser_.body_size();
		}

		size_t remaining_length = 0;
		parser_t parser_{};
	};

	template<
		net::is_socket SocketT,
		net::connection_mode connection_mode,
		typename ResponseT = string_response,
		typename RequestT = string_request>
	class connection : public tcp::connection<SocketT, connection_mode>
	{
	public:
		using base = tcp::connection<SocketT, connection_mode>;

		template<net::message_direction dir = net::message_direction::incoming>
		using message_type = http::message<SocketT, dir, connection_mode>;

	private:
		std::shared_ptr<spdlog::logger> logger_ = [this]() mutable {
			auto logger = logging::get_logger(*this);
			logger->flush_on(spdlog::level::debug);
			return logger;
		}();

	public:
		using socket_type = SocketT;
		auto& logger() { return *logger_; }

		std::string to_string() const
		{
			return fmt::format("connection::{}", this->peer_address());
		}
		static constexpr bool is_server = base::connection_mode == net::connection_mode::server;

		static constexpr bool is_client = not is_server;

		using receive_type = std::conditional_t<is_client, ResponseT, RequestT>;
		using base_receive_type =
			std::conditional_t<is_client, http::detail::base_response, http::detail::base_request>;
		using send_type = std::conditional_t<is_client, RequestT, ResponseT>;
		using base_send_type =
			std::conditional_t<is_client, http::detail::base_request, http::detail::base_response>;
		using parser_type = std::conditional_t<is_client, response_parser, request_parser>;

		connection(connection&& other) noexcept
			: base{ std::move(other) }
			, logger_{ std::move(other.logger_) }
		{
		}

		virtual ~connection() noexcept
		{
			if (logger_)
			{
				logger_->info("connection closed");
				logging::drop_logger(*this);
			}
		}

		connection(const connection& other) = delete;

		connection& operator=(connection&& other) = delete;

		connection& operator=(const connection& other) = delete;

		explicit connection(socket_type socket, cancellation_token ct)
			: base(std::move(socket), std::move(ct))
		{
			logger_->info("new {} connection", is_server ? "server" : "client");
		}

		template<typename ConnectionT>
		ConnectionT upgrade() noexcept
		{
			return ConnectionT{ std::move(this->sock_), std::move(this->ct_) };
		}
	};

	template<net::is_socket SocketT>
	using server_connection = connection<SocketT, net::connection_mode::server>;

	template<net::is_socket SocketT>
	using client_connection = connection<SocketT, net::connection_mode::client>;

}  // namespace cppcoro::http