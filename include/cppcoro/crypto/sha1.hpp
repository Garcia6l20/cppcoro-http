#pragma once

#include <mbedtls/sha1.h>

#include <string_view>

namespace cppcoro::crypto {
	class sha1 {
	public:
        sha1() noexcept {
			mbedtls_sha1_init(&ctx_);
            mbedtls_sha1_starts(&ctx_);
		}
		~sha1() noexcept {
            mbedtls_sha1_free(&ctx_);
		}

		sha1 &operator<<(std::string_view input) noexcept {
			mbedtls_sha1_update(&ctx_, reinterpret_cast<const uint8_t*>(input.data()), input.size());
			return *this;
		}

		std::string operator()() noexcept {
			uint8_t output[20];
			mbedtls_sha1_finish(&ctx_, output);
			return std::string{reinterpret_cast<char*>(&output[0]), 20};
		}

		template <std::convertible_to<std::string_view> ...ItemsT>
		static std::string hash(ItemsT &&...items) {
			sha1 hasher{};
            (hasher << ... << items);
			return hasher();
		}

	private:
		mbedtls_sha1_context ctx_;
	};
}
