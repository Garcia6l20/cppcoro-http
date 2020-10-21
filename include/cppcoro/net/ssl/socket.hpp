/** @file cppcoro/net/ssl/socket.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <mbedtls/timing.h>

#include <cppcoro/net/ssl/certificate.hpp>
#include <cppcoro/net/ssl/context.hpp>
#include <cppcoro/net/ssl/key.hpp>
#include <cppcoro/net/ssl/handshake_operation.hpp>

#include <cppcoro/net/socket.hpp>

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

        handshake_operation encrypt() noexcept {
			return {io_service_.io_queue(), ssl_context_};
		}

	private:
		socket(io_service &service, net::socket&& sock, ssl::certificate&& certificate, ssl::private_key&& key)
			: net::socket{ std::forward<net::socket>(sock) }
			, io_service_{service}
			, certificate_{ std::forward<ssl::certificate>(certificate) }
			, key_{ std::forward<ssl::private_key>(key) }
		{
			init(MBEDTLS_SSL_IS_SERVER);
		}

		explicit socket(io_service &service, net::socket&& sock)
			: net::socket{ std::forward<net::socket>(sock) }
            , io_service_{service}
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

			certificate_.load(context.ca_certs().chain());
			mbedtls_ssl_conf_ca_chain(&ssl_config_, &certificate_.chain(), nullptr);

			peer_verify_mode(peer_verify_mode::optional);

			mbedtls_ssl_set_bio(
				&ssl_context_,
				this,
				&ssl::socket::mbedtls_send,
				&ssl::socket::mbedtls_recv,
				&ssl::socket::mbedtls_recv_timeout);

			mbedtls_ssl_set_timer_cb(
				&ssl_context_,
				&timing_delay_context_,
				mbedtls_timing_set_delay,
				mbedtls_timing_get_delay);
		}

		static int mbedtls_send(void* ctx, const uint8_t* buf, size_t len) noexcept {}
		static int mbedtls_recv(void* ctx, uint8_t* buf, size_t len) noexcept {}
		static int
		mbedtls_recv_timeout(void* ctx, uint8_t* buf, size_t len, uint32_t timeout) noexcept
		{

		}

		io_service &io_service_;
		ssl::certificate certificate_{};
		ssl::private_key key_{};
		mbedtls_ssl_context ssl_context_{};
		mbedtls_ssl_config ssl_config_{};
		mbedtls_timing_delay_context timing_delay_context_{};
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
