/** @file cppcoro/net/ssl/socket.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <mbedtls/timing.h>

#include <cppcoro/net/ssl/certificate.hpp>
#include <cppcoro/net/ssl/context.hpp>
#include <cppcoro/net/ssl/key.hpp>

#include <cppcoro/net/ip_endpoint.hpp>
#include <cppcoro/net/socket.hpp>
#include <cppcoro/task.hpp>

#include <cppcoro/detail/c_ptr.hpp>

#include <utility>

#include <spdlog/spdlog.h>

namespace cppcoro::net::ssl
{
	enum class peer_verify_mode
	{
		none = MBEDTLS_SSL_VERIFY_NONE,
		required = MBEDTLS_SSL_VERIFY_REQUIRED,
		optional = MBEDTLS_SSL_VERIFY_OPTIONAL,
	};

	namespace detail
	{
		using mbedtls_ssl_context_ptr =
			cppcoro::detail::c_unique_ptr<mbedtls_ssl_context, mbedtls_ssl_init, mbedtls_ssl_free>;
		using mbedtls_ssl_config_ptr = cppcoro::detail::
			c_unique_ptr<mbedtls_ssl_config, mbedtls_ssl_config_init, mbedtls_ssl_config_free>;
	}  // namespace detail

	class socket : public net::socket
	{
	private:
		enum class mode
		{
			server,
			client
		};

		template<mode mode_, bool tcp_v6 = false>
		static socket create(
			io_service& ioSvc,
			std::optional<ssl::certificate> certificate,
			std::optional<ssl::private_key> pk);

	public:

		socket(socket&& other) noexcept
			: net::socket(std::move(other))
			, io_service_{other.io_service_}
			, certificate_{std::move(other.certificate_)}
            , key_{std::move(other.key_)}
			, ssl_context_{std::move(other.ssl_context_)}
			, ssl_config_{std::move(other.ssl_config_)}
			, timing_delay_context_{other.timing_delay_context_}
		    , encrypted_{other.encrypted_}
            , to_receive_{other.to_receive_}
            , to_send_{other.to_send_}
		{
			// update callbacks

			mbedtls_ssl_set_bio(
				ssl_context_.get(),
				this,
				&ssl::socket::_mbedtls_send,
				&ssl::socket::_mbedtls_recv,
				&ssl::socket::_mbedtls_recv_timeout);

            mbedtls_ssl_conf_verify(ssl_config_.get(), &ssl::socket::_mbedtls_verify_cert, this);

#ifdef CPPCORO_SSL_DEBUG
            mbedtls_ssl_conf_dbg(&ssl_config_, &ssl::socket::_mbedtls_debug, this);
#endif
		}

		static socket
		create_server(io_service& io_service, ssl::certificate&& certificate, ssl::private_key&& pk)
		{
			return create<mode::server>(
				io_service,
				std::forward<decltype(certificate)>(certificate),
				std::forward<decltype(pk)>(pk));
		}
		static socket create_client(
			io_service& io_service,
			std::optional<ssl::certificate> certificate = {},
			std::optional<ssl::private_key> pk = {})
		{
			return create<mode::client>(io_service, std::move(certificate), std::move(pk));
		}
		static socket create_server_v6(
			io_service& io_service, ssl::certificate&& certificate, ssl::private_key&& pk)
		{
			return create<mode::server, true>(
				io_service,
				std::forward<decltype(certificate)>(certificate),
				std::forward<decltype(pk)>(pk));
		}
		static socket create_client_v6(
			io_service& io_service,
			std::optional<ssl::certificate> certificate = {},
			std::optional<ssl::private_key> pk = {})
		{
			return create<mode::client, true>(io_service, std::move(certificate), std::move(pk));
		}

		virtual ~socket() noexcept = default;

		void peer_verify_mode(peer_verify_mode mode) noexcept
		{
			mbedtls_ssl_conf_authmode(ssl_config_.get(), int(mode));
		}

		void host_name(std::string_view host_name) noexcept
		{
			mbedtls_ssl_set_hostname(ssl_context_.get(), host_name.data());
		}

		task<> encrypt()
		{
			if (encrypted_)
				co_return;

			while (true)
			{
				auto result = mbedtls_ssl_handshake(ssl_context_.get());
				if (result == 0)
				{
					break;
				}
				else if (result == MBEDTLS_ERR_SSL_WANT_READ)
				{
					assert(to_receive_);  // ensure buffer/len properly setup
					to_receive_.actual_len =
						co_await net::socket::recv(to_receive_.buf, to_receive_.len);
				}
				else if (result == MBEDTLS_ERR_SSL_WANT_WRITE)
				{
					assert(to_send_);  // ensure buffer/len properly setup
					to_send_.actual_len = co_await net::socket::send(to_send_.buf, to_send_.len);
				}
				else if (result == MBEDTLS_ERR_SSL_TIMEOUT)
				{
					co_await io_service_.schedule();  // reschedule ?
				}
				else
				{
					throw std::system_error(result, ssl::error_category, "mbedtls_ssl_handshake");
				}
			}
			encrypted_ = true;
		}

		task<size_t> send(const void* data, size_t size)
		{
			size_t offset = 0;
			while (true)
			{
				int result = mbedtls_ssl_write(
					ssl_context_.get(),
					reinterpret_cast<const uint8_t*>(data) + offset,
					size - offset);
				if (result == MBEDTLS_ERR_SSL_WANT_WRITE)
				{
					assert(to_send_);  // ensure buffer/len properly setup
					to_send_.actual_len = co_await net::socket::send(to_send_.buf, to_send_.len);
				}
				else if (result < 0)
				{
					throw std::system_error(result, ssl::error_category, "mbedtls_ssl_write");
				}
				else
				{
					offset += result;
					if (offset >= size)
					{
						co_return size;
					}
				}
			}
		}

		task<size_t> recv(void* data, size_t size)
		{
			size_t offset = 0;
			while (true)
			{
				int result = mbedtls_ssl_read(
					ssl_context_.get(), reinterpret_cast<uint8_t*>(data) + offset, size - offset);
				if (result == MBEDTLS_ERR_SSL_WANT_READ)
				{
					assert(to_receive_);  // ensure buffer/len properly setup
					to_receive_.actual_len =
						co_await net::socket::recv(to_receive_.buf, to_receive_.len);
				}
				else if (result < 0)
				{
					throw std::system_error(result, ssl::error_category, "mbedtls_ssl_read");
				}
				else
				{
					offset += result;
					if (offset >= size || result == 0)
					{
						co_return offset;
					}
				}
			}
		}

	private:
		using net::socket::recv;
		using net::socket::recv_from;
		using net::socket::send;
		using net::socket::send_to;

		socket(
			io_service& service,
			net::socket sock,
			mode mode_,
			std::optional<ssl::certificate> cert,
			std::optional<ssl::private_key> key)
			: net::socket{ std::move(sock) }
			, io_service_{ service }
			, certificate_{ std::move(cert) }
			, key_{ std::move(key) }
		{
			if (auto error = mbedtls_ssl_config_defaults(
					ssl_config_.get(),
					mode_ == mode::server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT);
				error != 0)
			{
				throw std::system_error{ error,
										 ssl::error_category,
										 "mbedtls_ssl_config_defaults" };
			}

			mbedtls_ssl_conf_ca_chain(ssl_config_.get(), &context.ca_certs().chain(), nullptr);

			peer_verify_mode(peer_verify_mode::optional);

			mbedtls_ssl_set_bio(
				ssl_context_.get(),
				this,
				&ssl::socket::_mbedtls_send,
				&ssl::socket::_mbedtls_recv,
				&ssl::socket::_mbedtls_recv_timeout);

//			mbedtls_ssl_set_timer_cb(
//				ssl_context_.get(),
//				&timing_delay_context_,
//				mbedtls_timing_set_delay,
//				mbedtls_timing_get_delay);

			mbedtls_ssl_conf_rng(
				ssl_config_.get(), mbedtls_ctr_drbg_random, &context.drbg_context());
			mbedtls_ssl_conf_verify(ssl_config_.get(), &ssl::socket::_mbedtls_verify_cert, this);

#ifdef CPPCORO_SSL_DEBUG
			mbedtls_ssl_conf_dbg(&ssl_config_, &ssl::socket::_mbedtls_debug, this);
#endif

			if (certificate_)
			{
				assert(key_);
				if (auto error = mbedtls_ssl_conf_own_cert(
						ssl_config_.get(), &certificate_->chain(), &key_->ctx());
					error != 0)
				{
					throw std::system_error{ error,
											 ssl::error_category,
											 "mbedtls_ssl_conf_own_cert" };
				}
			}

			if (auto error = mbedtls_ssl_setup(ssl_context_.get(), ssl_config_.get()); error != 0)
			{
				throw std::system_error{ error, ssl::error_category, "mbedtls_ssl_setup" };
			}
		}

#ifdef CPPCORO_SSL_DEBUG
		static void _mbedtls_debug(
			void* /* ctx */, int level, const char* file, int line, const char* str) noexcept
		{
			static constexpr std::string_view fmt = "{}:{}: {}";
			switch (level)
			{
				case 0:  // shall not happen - no debug
				case 1:  // error
					spdlog::error(fmt, file, line, str);
					break;
				case 3:  // informational
					spdlog::info(fmt, file, line, str);
					break;
				case 2:  // state change
				case 4:  // verbose
				default:
					spdlog::debug(fmt, file, line, str);
					break;
			}
		}
#endif

		static int
		_mbedtls_verify_cert(void* ctx, mbedtls_x509_crt* crt, int depth, uint32_t* flags) noexcept
		{
			// TODO
			return 0;
		}

		static int _mbedtls_send(void* ctx, const uint8_t* buf, size_t len) noexcept
		{
			auto& self = *static_cast<ssl::socket*>(ctx);
			if (self.to_send_ == std::tuple{ len, buf })
			{
				len = self.to_send_.actual_len;
				self.to_send_ = {};
				return len;
			}
			else
			{
				self.to_send_ = { len, buf };
				return MBEDTLS_ERR_SSL_WANT_WRITE;
			}
		}
		static int _mbedtls_recv(void* ctx, uint8_t* buf, size_t len) noexcept
		{
			abort();
			return MBEDTLS_ERR_SSL_WANT_READ;
		}
		static int
		_mbedtls_recv_timeout(void* ctx, uint8_t* buf, size_t len, uint32_t timeout) noexcept
		{
			auto& self = *static_cast<ssl::socket*>(ctx);
			if (self.to_receive_ == std::tuple{ len, buf })
			{
				len = self.to_receive_.actual_len;
				self.to_receive_ = {};
				return len;
			}
			else
			{
				self.to_receive_ = { len, buf };
				assert(timeout == 0);
				return MBEDTLS_ERR_SSL_WANT_READ;
			}
		}

        template<bool const_ = false>
        struct ssl_buf
        {
            size_t len = 0;
            std::conditional_t<const_, const uint8_t*, uint8_t*> buf = nullptr;
            size_t actual_len = 0;

            operator bool() const noexcept { return len != 0 && buf != nullptr; }
            bool operator==(std::tuple<size_t, const uint8_t*> other) const noexcept
            {
                return len == std::get<0>(other) && buf == std::get<1>(other);
            }
        };
		io_service& io_service_;
		std::optional<ssl::certificate> certificate_{};
		std::optional<ssl::private_key> key_{};
		detail::mbedtls_ssl_context_ptr ssl_context_ = detail::mbedtls_ssl_context_ptr::make();
		detail::mbedtls_ssl_config_ptr ssl_config_ = detail::mbedtls_ssl_config_ptr::make();
		mbedtls_timing_delay_context timing_delay_context_{};
		bool encrypted_ = false;
		ssl_buf<> to_receive_{};
		ssl_buf<true> to_send_{};
	};

	template<ssl::socket::mode mode_, bool tcp_v6>
	ssl::socket ssl::socket::create(
		cppcoro::io_service& io_service,
		std::optional<ssl::certificate> cert,
		std::optional<ssl::private_key> pk)
	{
		if constexpr (mode_ == mode::server)
		{
			assert(cert && pk);
		}
		std::optional<net::socket> sock;
		if constexpr (tcp_v6)
		{
			sock = socket::create_tcpv6(io_service);
		}
		else
		{
			sock = socket::create_tcpv4(io_service);
		}
		assert(sock);
		return socket(io_service, std::move(*sock), mode_, std::move(cert), std::move(pk));
	}
}  // namespace cppcoro::net::ssl
