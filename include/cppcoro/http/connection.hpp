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

	template<typename MethodOrStatusT, bool is_request>
	struct message_header
	{
		using parser_t = detail::static_parser_handler<is_request>;

		message_header(MethodOrStatusT& method_or_status, http::headers&& headers = {}) noexcept
			: _method_or_status{ method_or_status }
			, headers{ std::forward<http::headers>(headers) }
		{
		}

		message_header(parser_t& parser)
		{
			_method_or_status = parser.status_code_or_method();
			headers = std::move(parser.headers_);
			if constexpr (is_request)
			{
				path = parser.url();
			}
		}

		MethodOrStatusT _method_or_status;
		MethodOrStatusT& method{ _method_or_status };
		MethodOrStatusT& status{ _method_or_status };

		std::optional<size_t> content_length{};
		std::optional<std::string> path{};

		http::headers headers;

		[[nodiscard]] bool chunked() const noexcept { return content_length.has_value(); }

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
			if (not chunked())
			{
				std::array<char, 64> content_length_str{};
				auto [ptr, error] = std::to_chars(
					begin(content_length_str), end(content_length_str), *content_length);
				assert(error == std::errc{});
				this->headers.emplace(
					"Content-Length", std::string_view{ content_length_str.data(), ptr });
			}
			else
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
	};

	template<net::is_socket SocketT, net::message_direction direction, net::connection_mode mode>
	struct message : net::message<SocketT, direction>
	{
		using base = net::message<SocketT, direction>;
		using base::base;
		using base::receive;
		using base::send;

		static constexpr bool is_request = (mode == net::connection_mode::server and
											direction == net::message_direction::incoming) or
			(mode == net::connection_mode::client and
			 direction == net::message_direction::outgoing);

		static constexpr bool is_response = (mode == net::connection_mode::server and
											 direction == net::message_direction::outgoing) or
			(mode == net::connection_mode::client and
			 direction == net::message_direction::incoming);

		using method_or_status_t = std::conditional_t<is_request, http::method, http::status>;
		using parser_t = detail::static_parser_handler<is_request>;

		using header_type = message_header<method_or_status_t, is_request>;

		header_type make_header(method_or_status_t method_or_status, http::headers&& headers = {})
		{
			return header_type{ method_or_status, std::forward<http::headers>(headers) };
		}

		task<header_type> receive_header()
		{
			auto bytes_received = co_await this->receive();
			parser_.init_parser();  // TODO remove this ugly thing
			parser_.parse(std::span{ this->bytes_.data(), bytes_received });
			co_return header_type{ parser_ };
		}

		task<size_t> send(header_type&& header)
		{
			std::string data = std::forward<header_type>(header).build();
			return this->_send(std::as_writable_bytes(std::span{ data }));
		}

	private:
		parser_t parser_;
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
			, /*input_{std::move(other.input_)},*/
			buffer_{ std::move(other.buffer_) }
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
			, buffer_(2048, 0)
		{
			logger_->info("new {} connection", is_server ? "server" : "client");
		}

		static constexpr parser_type make_parser() { return parser_type{}; }

		template<typename T, size_t extent = std::dynamic_extent>
		task<bool> receive(parser_type& parser, std::span<T, extent> data)
		{
			using message_t = http::detail::abstract_span_message<is_client, decltype(data)>;
			logger_->debug("waiting for incoming message...");
			// std::fill(begin(buffer_), end(buffer_), '\0');
			auto ret = co_await this->sock_.recv(
				std::as_writable_bytes(data).data(),
				std::as_writable_bytes(data).size(),
				this->ct_);
			logger_->debug("got something: {}", ret);
			bool done = ret <= 0;
			co_return done;
		}

		task<> receive(base_receive_type& receive)
		{
			parser_type parser;

			while (true)
			{
				logger_->debug("waiting for incoming message...");
				// std::fill(begin(buffer_), end(buffer_), '\0');
				auto ret = co_await this->sock_.recv(buffer_.data(), buffer_.size(), this->ct_);
				logger_->debug("got something: {}", ret);
				bool done = ret <= 0;
				if (!done)
				{
					parser.parse(buffer_.data(), ret);
					receive = parser;
					if (parser.has_body() && not parser)
					{
						// chunk
						assert(false);
					}
					if (parser)
					{
						co_await parser.load(receive);
						logger_->debug("message: {}", receive);
						co_return;
					}
				}
				else
				{
					co_return;
				}
			}
		}

#if 0
		task<receive_type*> next(std::function<base_receive_type&(parser_type&)> init)
		{
			base_receive_type* result = nullptr;
			parser_type parser;
			auto init_result = [&] {
				result = &init(parser);
				if (!result)
				{
					logger_->warn("unable to get valid message handler for {}", parser);
				}
			};
			while (true)
			{
				logger_->debug("waiting for incoming message...");
				// std::fill(begin(buffer_), end(buffer_), '\0');
				auto ret = co_await this->sock_.recv(buffer_.data(), buffer_.size(), this->ct_);
				logger_->debug("got something: {}", ret);
				bool done = ret <= 0;
				if (!done)
				{
					parser.parse(buffer_.data(), ret);
					if (!result)
						init_result();
					if (parser.has_body() && not parser)
					{
						// chunk
						if (result)
						{
							co_await parser.load(*result);
							logger_->debug("chunked message: {}", *result);
						}
						else
						{
							co_return nullptr;
						}
					}
					if (parser)
					{
						if (!result)
							init_result();
						if (result)
						{
							//co_await parser.load(*result);
							logger_->debug("message: {}", *result);
							co_return std::move(static_cast<receive_type*>(result));
						}
						else
						{
							co_return nullptr;
						}
					}
				}
				else
				{
					co_return nullptr;
				}
			}
		}

		auto _send() {

		}

		auto
		post(std::string&& path, std::string&& data = "", http::headers&& headers = {}) requires(
			is_client())
		{
			return _send<http::method::post>(
				std::forward<std::string>(path),
				std::forward<std::string>(data),
				std::forward<http::headers>(headers));
		}

		auto
		get(std::string&& path = "/",
			std::string&& data = "",
			http::headers&& headers = {}) requires(is_client())
		{
			return _send<http::method::get>(
				std::forward<std::string>(path),
				std::forward<std::string>(data),
				std::forward<http::headers>(headers));
		}

		template<typename T, size_t extent>
		task<> send(auto status_or_method, std::span<T, extent> data, http::headers&& headers = {})
		{
			using message_t = http::detail::abstract_span_message<is_server(), std::decay_t<decltype(data)>>;
			message_t message{ status_or_method, std::span{ data }, std::forward<http::headers>(headers) };
			auto header = message.build_header();
			auto size = co_await this->sock_.send(header.data(), header.size(), this->ct_);
			assert(size == header.size());
			if (!data.empty())
			{
				logger_->debug("body: {}", data);
				auto size = co_await this->sock_.send(
					std::as_bytes(data).data(), std::as_bytes(data).size(), this->ct_);
				assert(size == std::as_bytes(data).size());
			}
		}

		task<> send(std::derived_from<http::detail::base_message> auto& to_send)
		{
			auto header = to_send.build_header();
			try
			{
				if (to_send.is_chunked())
				{
					byte_span body;
					auto size = co_await this->sock_.send(header.data(), header.size(), this->ct_);
					assert(size == header.size());
					body = co_await to_send.read_body();
					while (!body.empty())
					{
						auto size_str = fmt::format("{:x}\r\n", body.size());
						co_await this->sock_.send(size_str.data(), size_str.size(), this->ct_);
						logger_->debug("chunked body: {}", body);
						size = co_await this->sock_.send(body.data(), body.size(), this->ct_);
						if (size != body.size())
						{
							logger_->error("body not sent ({}/{})", size, body.size());
						}
						else
						{
							co_await this->sock_.send("\r\n", 2, this->ct_);
						}
						body = co_await to_send.read_body();
					}
					auto size_str = fmt::format("{}\r\n\r\n", 0);
					co_await this->sock_.send(size_str.data(), size_str.size(), this->ct_);
				}
				else
				{
					auto size = co_await this->sock_.send(header.data(), header.size(), this->ct_);
					assert(size == header.size());
					if (!to_send.body.empty())
					{
						logger_->debug("body: {}", to_send.body);
						auto size = co_await this->sock_.send(to_send.body.data(), to_send.body.size(), this->ct_);
						assert(size == to_send.body.size());
					}
//				}
			}
			catch (std::system_error& error)
			{
				if (error.code() == std::errc::connection_reset)
				{
					throw error;  // Connection reset by peer
				}
				logger_->error("system_error caught: {}", error.what());
				if constexpr (is_server())
				{
					string_response error_message{ http::status::HTTP_STATUS_INTERNAL_SERVER_ERROR,
												   std::string{ error.what() },
												   {} };
					if (error.code() == std::errc::no_such_file_or_directory)
					{
						error_message.status = http::status::HTTP_STATUS_NOT_FOUND;
					}
					header = error_message.build_header();
					auto size = co_await this->sock_.send(header.data(), header.size(), this->ct_);
					assert(size == header.size());
					auto body = co_await to_send.read_body();
					size = co_await this->sock_.send(body.data(), body.size(), this->ct_);
					assert(size == body.size());
				}
				else
				{
					throw;
				}
			}
		}
#endif

		template<typename ConnectionT>
		ConnectionT upgrade() noexcept
		{
			return ConnectionT{ std::move(this->sock_) };
		}

	private:
#if 0
		template<http::method _method>
		task<std::optional<receive_type>>
		_send(std::string&& path, std::string&& data, http::headers&& headers) requires(is_client())
		{
			send_type request{ _method,
							   std::forward<std::string>(path),
							   std::forward<std::string>(data),
							   std::forward<http::headers>(headers) };
			co_await send(request);
			receive_type response{ http::status::HTTP_STATUS_NOT_FOUND };
			auto resp = co_await next([&](http::response_parser& parser) -> receive_type& {
				response = parser;
				return response;
			});
			if (resp)
			{
				co_return std::optional<receive_type>{ std::move(*resp) };
			}
			co_return std::optional<receive_type>{};
		}
#endif

		std::vector<char> buffer_;
		// std::unique_ptr<receive_type> input_;
	};

	template<net::is_socket SocketT>
	using server_connection = connection<SocketT, net::connection_mode::server>;

	template<net::is_socket SocketT>
	using client_connection = connection<SocketT, net::connection_mode::client>;

}  // namespace cppcoro::http