/** @file cppcoro/net/ssl/key.hpp
 * @author Sylvain Garcia <garcia.6l20@gmail.com>
 */
#pragma once

#include <mbedtls/pk.h>

#include <cppcoro/filesystem.hpp>
#include <cppcoro/net/ssl/c_ptr.hpp>
#include <cppcoro/net/ssl/error.hpp>

#include <span>

namespace cppcoro::net::ssl
{
	enum class key_type
	{
		public_,
		private_
	};

	namespace detail {
		using mbedtls_pk_context_ptr = cppcoro::detail::c_shared_ptr<mbedtls_pk_context, mbedtls_pk_init, mbedtls_pk_free>;
	}

	template<key_type type_>
	class key final
	{
	public:
		explicit key() noexcept = default;

		template<typename T>
		explicit key(std::span<T> data, std::string_view passphrase = {})
			: key{}
		{
			if constexpr (type_ == key_type::private_)
			{
				if (auto error = mbedtls_pk_parse_key(
						ctx_.get(),
						reinterpret_cast<const unsigned char*>(data.data()),
						data.size_bytes(),
						reinterpret_cast<const unsigned char*>(
							passphrase.empty() ? nullptr : passphrase.data()),
						passphrase.size());
					error)
				{
					throw std::system_error{ error, ssl::error_category, "mbedtls_pk_parse_key" };
				}
			}
			else
			{
				if (auto error = mbedtls_pk_parse_public_key(
						ctx_.get(),
						reinterpret_cast<const unsigned char*>(data.data()),
						data.size_bytes());
					error)
				{
					throw std::system_error{ error,
											 ssl::error_category,
											 "mbedtls_pk_parse_public_key" };
				}
			}
		}

	private:
		detail::mbedtls_pk_context_ptr ctx_ = detail::mbedtls_pk_context_ptr::make();
	};
	using private_key = key<key_type::private_>;
	using public_key = key<key_type::public_>;
}  // namespace cppcoro::net::ssl
