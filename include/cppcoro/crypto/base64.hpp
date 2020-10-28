#pragma once

#include <cctype>

#include <string>
#include <string_view>

namespace cppcoro::crypto
{
	struct base64
	{
		static constexpr const std::string_view _chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
														 "abcdefghijklmnopqrstuvwxyz"
														 "0123456789+/";

		static inline bool is_base64(uint8_t c) { return (isalnum(c) || (c == '+') || (c == '/')); }

        static inline std::string encode(uint8_t const* bytes_to_encode, size_t in_len)
		{
			std::string ret;
			int i = 0;
			int j = 0;
			uint8_t char_array_3[3];
			uint8_t char_array_4[4];

			while (in_len--)
			{
				char_array_3[i++] = *(bytes_to_encode++);
				if (i == 3)
				{
					char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
					char_array_4[1] = static_cast<uint8_t>(
						((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4));
					char_array_4[2] = static_cast<uint8_t>(
						((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6));
					char_array_4[3] = char_array_3[2] & 0x3f;

					for (i = 0; (i < 4); i++)
						ret += _chars[char_array_4[i]];
					i = 0;
				}
			}

			if (i)
			{
				for (j = i; j < 3; j++)
					char_array_3[j] = '\0';

				char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
				char_array_4[1] = static_cast<uint8_t>(
					((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4));
				char_array_4[2] = static_cast<uint8_t>(
					((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6));

				for (j = 0; (j < i + 1); j++)
					ret += _chars[char_array_4[j]];

				while ((i++ < 3))
					ret += '=';
			}

			return ret;
		}

		template<typename IteratorT>
        static inline std::string encode(IteratorT begin, IteratorT end)
		{
			return encode(begin, std::distance(begin, end));
		}

        static inline std::string encode(std::string_view data) noexcept {
			return encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
		}

		static inline std::string decode(std::string const& encoded_string)
		{
			size_t in_len = encoded_string.size();
			int i = 0;
			int j = 0;
			size_t in_ = 0;
			unsigned char char_array_4[4], char_array_3[3];
			std::string ret;
			ret.resize(encoded_string.size() / 4);

			while (in_len-- && (encoded_string[in_] != '=') &&
				   is_base64(uint8_t(encoded_string[in_])))
			{
				char_array_4[i++] = uint8_t(encoded_string[in_]);
				in_++;
				if (i == 4)
				{
					for (i = 0; i < 4; i++)
						char_array_4[i] = uint8_t(_chars.find(char(char_array_4[i])));

					char_array_3[0] = static_cast<uint8_t>(
						(char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
					char_array_3[1] = static_cast<uint8_t>(
						((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));
					char_array_3[2] =
						static_cast<uint8_t>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

					for (i = 0; (i < 3); i++)
						ret += char(char_array_3[i]);
					i = 0;
				}
			}

			if (i)
			{
				for (j = 0; j < i; j++)
					char_array_4[j] = uint8_t(_chars.find(char(char_array_4[j])));

				char_array_3[0] =
					static_cast<uint8_t>((char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4));
				char_array_3[1] = static_cast<uint8_t>(
					((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2));

				for (j = 0; (j < i - 1); j++)
					ret += char(char_array_3[j]);
			}

			return ret;
		}
	};

}  // namespace cppcoro::crypto
