#include <ctype.h>
import std;
import star.format;
import star.string;
import star.byte_array;
import star.encode;

namespace Star {

std::size_t hexEncode(std::span<char const> data, std::span<char> output) {
  static char const hex[] = "0123456789abcdef";

  std::size_t len = std::min(data.size(), output.size() / 2);
  for (std::size_t i = 0; i < len; ++i) {
    output[i * 2] = hex[(data[i] & 0xf0) >> 4];
    output[i * 2 + 1] = hex[(data[i] & 0x0f)];
  }

  return len * 2;
}

std::expected<std::size_t, StarException> hexDecode(std::span<char const> src, std::span<char> output) {
  for (std::size_t i = 0; i < src.size() / 2; ++i) {
    if (i >= output.size())
      return i;

    std::uint8_t b1 = 0;
    char c1 = src[i * 2];
    if (c1 >= '0' && c1 <= '9')
      b1 = c1 - '0';
    else if (c1 >= 'A' && c1 <= 'F')
      b1 = c1 - 'A' + 10;
    else if (c1 >= 'a' && c1 <= 'f')
      b1 = c1 - 'a' + 10;

    std::uint8_t b2 = 0;
    char c2 = src[i * 2 + 1];
    if (c2 >= '0' && c2 <= '9')
      b2 = c2 - '0';
    else if (c2 >= 'A' && c2 <= 'F')
      b2 = c2 - 'A' + 10;
    else if (c2 >= 'a' && c2 <= 'f')
      b2 = c2 - 'a' + 10;

    output[i] = (b1 << 4) | b2;
  }

  return src.size() / 2;
}

std::expected<std::size_t, StarException> nibbleDecode(std::span<char const> src, std::span<char> output) {
  for (std::size_t i = 0; i < src.size(); ++i) {
    if (i >= output.size())
      return i;

    std::uint8_t b = 0;
    char c = src[i];
    if (c >= '0' && c <= '9')
      b = c - '0';
    else if (c >= 'A' && c <= 'F')
      b = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
      b = c - 'a' + 10;

    output[i] = b;
  }

  return src.size();
}

static constexpr std::string_view base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::size_t base64Encode(std::span<char const> data, std::span<char> destination) {
  auto output = destination.data();
  auto outLen = destination.size();
  if (outLen == 0)
    return 0;
  std::size_t written = 0;

  unsigned char ca3[3] = {0, 0, 0};
  unsigned char ca4[4] = {0, 0, 0, 0};
  const unsigned char* inPtr = reinterpret_cast<unsigned char const*>(data.data());
  int i = 0, j = 0;
  std::size_t in_len = data.size();

  while (in_len--) {
    ca3[i++] = *(inPtr++);
    if (i == 3) {
      ca4[0] = (ca3[0] & 0xfc) >> 2;
      ca4[1] = ((ca3[0] & 0x03) << 4) + ((ca3[1] & 0xf0) >> 4);
      ca4[2] = ((ca3[1] & 0x0f) << 2) + ((ca3[2] & 0xc0) >> 6);
      ca4[3] = ca3[2] & 0x3f;
      for (i = 0; (i < 4); i++) {
        --outLen;
        *output = base64_chars[ca4[i]];
        ++output;
        ++written;

        if (outLen == 0)
          return written;
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++)
      ca3[j] = '\0';
    ca4[0] = (ca3[0] & 0xfc) >> 2;
    ca4[1] = ((ca3[0] & 0x03) << 4) + ((ca3[1] & 0xf0) >> 4);
    ca4[2] = ((ca3[1] & 0x0f) << 2) + ((ca3[2] & 0xc0) >> 6);
    ca4[3] = ca3[2] & 0x3f;
    for (j = 0; (j < i + 1); j++) {
      --outLen;
      *output = base64_chars[ca4[j]];
      ++output;
      ++written;

      if (outLen == 0)
        return written;
    }
    while ((i++ < 3)) {
      --outLen;
      *output = '=';
      ++output;
      ++written;

      if (outLen == 0)
        return written;
    }
  }

  return written;
}

static inline bool is_base64(unsigned char c) {
  return (std::isalnum(c) || (c == '+') || (c == '/'));
}

std::expected<std::size_t, StarException> base64Decode(std::span<char const> src, std::span<char> destination) {
  auto output = destination.data();
  auto outLen = destination.size();
  if (outLen == 0)
    return 0;

  std::size_t written = 0;

  int i = 0, j = 0;
  std::size_t in_ = 0, in_len = src.size();
  unsigned char ca4[4], ca3[3];

  while (in_len-- && (src[in_] != '=') && is_base64(src[in_])) {
    ca4[i++] = src[in_++];
    if (i == 4) {
      for (i = 0; i < 4; i++)
        ca4[i] = base64_chars.find(ca4[i]);
      ca3[0] = (ca4[0] << 2) + ((ca4[1] & 0x30) >> 4);
      ca3[1] = ((ca4[1] & 0xf) << 4) + ((ca4[2] & 0x3c) >> 2);
      ca3[2] = ((ca4[2] & 0x3) << 6) + ca4[3];
      for (i = 0; (i < 3); i++) {
        --outLen;
        *output = ca3[i];
        ++output;
        ++written;

        if (outLen == 0)
          return written;
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++)
      ca4[j] = 0;
    for (j = 0; j < 4; j++)
      ca4[j] = base64_chars.find(ca4[j]);
    ca3[0] = (ca4[0] << 2) + ((ca4[1] & 0x30) >> 4);
    ca3[1] = ((ca4[1] & 0xf) << 4) + ((ca4[2] & 0x3c) >> 2);
    ca3[2] = ((ca4[2] & 0x3) << 6) + ca4[3];
    for (j = 0; (j < i - 1); j++) {
      --outLen;
      *output = ca3[j];
      ++output;
      ++written;

      if (outLen == 0)
        return written;
    }
  }

  return written;
}

String hexEncode(std::span<char const> data) {
  std::string res;
  res.resize_and_overwrite(data.size() * 2, [data](char* output, std::size_t size) noexcept {
    std::size_t encoded = hexEncode(data, {output, size});
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((encoded == size));
    }
    return encoded;
  });
  return res;
}

String base64Encode(std::span<char const> data) {
  std::string res;
  res.resize_and_overwrite(data.size() * 4 / 3 + 3, [data](char* output, std::size_t size) noexcept {
    std::size_t encoded = base64Encode(data, {output, size});
    if constexpr (Star::DebugEnabled) {
      Star::starAssertFailed((encoded <= size));
    }
    return encoded;
  });
  return res;
}

String hexEncode(ByteArray const& data) {
  return hexEncode(data.span());
}

std::expected<ByteArray, StarException> hexDecode(String const& encodedData) {
  ByteArray res(encodedData.utf8_size() / 2, 0);
  auto decoded = hexDecode(encodedData.utf8(), res.span());
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  return res;
}

String base64Encode(ByteArray const& data) {
  return base64Encode(data.span());
}

std::expected<ByteArray, StarException> base64Decode(String const& encodedData) {
  ByteArray res((encodedData.utf8_size() + 3) / 4 * 3, 0);
  auto decoded = base64Decode(encodedData.utf8(), res.span());
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  res.resize(*decoded);
  return res;
}

}
