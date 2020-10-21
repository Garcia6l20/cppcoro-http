/** @file cppcoro/net/ssl/socket.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <mbedtls/timing.h>

#include <cppcoro/net/ssl/certificate.hpp>
#include <cppcoro/net/ssl/context.hpp>
#include <cppcoro/net/ssl/key.hpp>

#include <cppcoro/net/socket.hpp>
#include <cppcoro/task.hpp>

#include <spdlog/spdlog.h>

namespace cppcoro::net::ssl
{
	enum class peer_verify_mode
	{
		none = MBEDTLS_SSL_VERIFY_NONE,
		required = MBEDTLS_SSL_VERIFY_REQUIRED,
		optional = MBEDTLS_SSL_VERIFY_OPTIONAL,
	};

	class socket : public net::socket
	{
	public:
		static socket create_tcpv4(io_service& ioSvc, ssl::certificate&&, ssl::private_key&&);
		static socket create_tcpv4(io_service& ioSvc);

		virtual ~socket() noexcept
		{
			mbedtls_ssl_free(&ssl_context_);
			mbedtls_ssl_config_free(&ssl_config_);
		}

		void peer_verify_mode(peer_verify_mode mode) noexcept
		{
			mbedtls_ssl_conf_authmode(&ssl_config_, int(mode));
		}

		void host_name(std::string_view host_name) noexcept
		{
			mbedtls_ssl_set_hostname(&ssl_context_, host_name.data());
		}

		task<> encrypt()
		{
			if (encrypted_)
				co_return;

			while (true)
			{
				auto result = mbedtls_ssl_handshake(&ssl_context_);
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
					&ssl_context_, reinterpret_cast<const uint8_t*>(data) + offset, size - offset);
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
					&ssl_context_, reinterpret_cast<uint8_t*>(data) + offset, size - offset);
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
			ssl::certificate&& certificate,
			ssl::private_key&& key)
			: net::socket{ std::move(sock) }
			, io_service_{ service }
			, certificate_{ std::forward<ssl::certificate>(certificate) }
			, key_{ std::forward<ssl::private_key>(key) }
		{
			init(MBEDTLS_SSL_IS_SERVER);
		}

		explicit socket(io_service& service, net::socket sock)
			: net::socket{ std::move(sock) }
			, io_service_{ service }
		{
			init(MBEDTLS_SSL_IS_CLIENT);
		}

		void init(int endpoint)
		{
			mbedtls_ssl_init(&ssl_context_);
			mbedtls_ssl_config_init(&ssl_config_);
			if (auto error = mbedtls_ssl_config_defaults(
					&ssl_config_,
					endpoint,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT);
				error != 0)
			{
				throw std::system_error{ error,
										 ssl::error_category,
										 "mbedtls_ssl_config_defaults" };
			}

			mbedtls_ssl_conf_ca_chain(&ssl_config_, &context.ca_certs().chain(), nullptr);

			peer_verify_mode(peer_verify_mode::optional);

			mbedtls_ssl_set_bio(
				&ssl_context_,
				this,
				&ssl::socket::_mbedtls_send,
				&ssl::socket::_mbedtls_recv,
				&ssl::socket::_mbedtls_recv_timeout);

			mbedtls_ssl_set_timer_cb(
				&ssl_context_,
				&timing_delay_context_,
				mbedtls_timing_set_delay,
				mbedtls_timing_get_delay);

			mbedtls_ssl_conf_rng(&ssl_config_, mbedtls_ctr_drbg_random, &context.drbg_context());
			mbedtls_ssl_conf_verify(&ssl_config_, &ssl::socket::_mbedtls_verify_cert, this);

#ifdef CPPCORO_SSL_DEBUG
			mbedtls_ssl_conf_dbg(&ssl_config_, &ssl::socket::_mbedtls_debug, this);
#endif

			if (certificate_)
			{
				if (auto error = mbedtls_ssl_conf_own_cert(
						&ssl_config_, &certificate_->chain(), &key_->ctx());
					error != 0)
				{
					throw std::system_error{ error,
											 ssl::error_category,
											 "mbedtls_ssl_conf_own_cert" };
				}
			}

			if (auto error = mbedtls_ssl_setup(&ssl_context_, &ssl_config_); error != 0)
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

		io_service& io_service_;
		std::optional<ssl::certificate> certificate_{};
		std::optional<ssl::private_key> key_{};
		mbedtls_ssl_context ssl_context_{};
		mbedtls_ssl_config ssl_config_{};
		mbedtls_timing_delay_context timing_delay_context_{};
		bool encrypted_ = false;
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
		ssl_buf<> to_receive_{};
		ssl_buf<true> to_send_{};
	};

	ssl::socket ssl::socket::create_tcpv4(
		io_service& ioSvc, ssl::certificate&& certificate, ssl::private_key&& key)
	{
		return socket(
			ioSvc,
			net::socket::create_tcpv4(ioSvc),
			std::forward<ssl::certificate>(certificate),
			std::forward<ssl::private_key>(key));
	}
	ssl::socket ssl::socket::create_tcpv4(io_service& ioSvc)
	{
		return socket(ioSvc, net::socket::create_tcpv4(ioSvc));
	}
}  // namespace cppcoro::net::ssl
