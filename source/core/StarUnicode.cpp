import std;
import star.format;
import star.unicode;

namespace Star {
namespace unicode_detail {

// Only public checked boundaries turn these codes into owning diagnostics.
StarException decode_exception(decode_error error) {
  switch (error) {
    case decode_error::invalid_sequence: return UnicodeException("Invalid UTF-8 code unit sequence");
    case decode_error::missing_trailing_units: return UnicodeException("UTF-8 string missing trailing code units");
    case decode_error::invalid_codepoint: return UnicodeException("Invalid UTF-8 code point");
    case decode_error::past_end: return UnicodeException("Cannot read past end of UTF-8 range");
    case decode_error::none: std::unreachable();
  }
  std::unreachable();
}
}

unicode_detail::decode_error Utf8Cursor::decode() const {
  if (at_end()) return unicode_detail::decode_error::past_end;
  if (m_width != 0) return unicode_detail::decode_error::none;
  auto width = unicode_detail::decode_char(m_bytes.data() + m_offset, m_point, m_bytes.size() - m_offset);
  if (!width) return width.error();
  m_width = *width;
  return unicode_detail::decode_error::none;
}
std::expected<Utf32Type, StarException> Utf8Cursor::read() const {
  auto error = decode();
  if (error != unicode_detail::decode_error::none)
    return std::unexpected(unicode_detail::decode_exception(error));
  return m_point;
}
std::expected<void, StarException> Utf8Cursor::advance() {
  auto error = decode();
  if (error != unicode_detail::decode_error::none)
    return std::unexpected(unicode_detail::decode_exception(error));
  m_offset += m_width;
  m_width = 0;
  return {};
}
std::expected<void, StarException> Utf8Cursor::retreat() {
  if (m_offset == 0) return std::unexpected(UnicodeException("Cannot move before beginning of UTF-8 range"));
  auto offset = m_offset - 1;
  auto lower = m_offset > 4 ? m_offset - 4 : 0;
  while (offset != lower && (static_cast<unsigned char>(m_bytes[offset]) & 0xc0) == 0x80) --offset;
  // A valid predecessor needs at most four bytes. Malformed continuation tails
  // retain the old unbounded search and its precise decoder error precedence.
  if ((static_cast<unsigned char>(m_bytes[offset]) & 0xc0) == 0x80) {
    while (offset != 0 && (static_cast<unsigned char>(m_bytes[offset]) & 0xc0) == 0x80) --offset;
  }
  Utf32Type point;
  auto width = unicode_detail::decode_char(m_bytes.data() + offset, point, m_offset - offset);
  if (!width) return std::unexpected(unicode_detail::decode_exception(width.error()));
  if (*width != m_offset - offset) return std::unexpected(UnicodeException("Invalid UTF-8 boundary"));
  m_offset = offset;
  m_width = *width;
  m_point = point;
  return {};
}

std::expected<std::size_t, StarException> utf8_decode_char(std::string_view bytes, Utf32Type& output) {
  auto decoded = unicode_detail::decode_char(bytes.data(), output, bytes.size());
  if (!decoded) return std::unexpected(unicode_detail::decode_exception(decoded.error()));
  return *decoded;
}

std::expected<std::size_t, StarException> utf8_decode_char(Utf8Type const* bytes, Utf32Type& output) {
  auto decoded = unicode_detail::decode_char(bytes, output, 4, true);
  if (!decoded) return std::unexpected(unicode_detail::decode_exception(decoded.error()));
  return *decoded;
}

std::expected<std::size_t, StarException> utf8_length(std::string_view bytes) {
  std::size_t offset = 0;
  std::size_t length = 0;
  while (offset != bytes.size()) {
    // memcpy permits unaligned loads, and every load stays inside the view.
    while (bytes.size() - offset >= sizeof(std::uint64_t)) {
      std::uint64_t word;
      std::memcpy(&word, bytes.data() + offset, sizeof(word));
      if ((word & 0x8080808080808080ULL) != 0) break;
      offset += sizeof(word);
      length += sizeof(word);
    }
    while (offset != bytes.size() && static_cast<unsigned char>(bytes[offset]) < 0x80) {
      ++offset;
      ++length;
    }
    if (offset == bytes.size()) break;
    Utf32Type point;
    auto decoded = unicode_detail::decode_char(bytes.data() + offset, point, bytes.size() - offset);
    if (!decoded) return std::unexpected(unicode_detail::decode_exception(decoded.error()));
    offset += *decoded;
    ++length;
  }
  return length;
}

std::expected<std::size_t, StarException> utf8_length(Utf8Type const* bytes) {
  std::size_t length = 0;
  // Never inspect bytes beyond a terminator, even inside a potential sequence.
  while (*bytes != 0) {
    if (static_cast<unsigned char>(*bytes) < 0x80) {
      ++bytes;
    } else {
      Utf32Type point;
      auto decoded = unicode_detail::decode_char(bytes, point, 4, true);
      if (!decoded) return std::unexpected(unicode_detail::decode_exception(decoded.error()));
      bytes += *decoded;
    }
    ++length;
  }
  return length;
}

std::expected<std::size_t, StarException> utf8_encode_char(std::span<Utf8Type> bytes, Utf32Type point) {
  if (point > 0x10ffff)
    return std::unexpected(UnicodeException::format("Invalid UTF-32 code point {} encountered while trying to encode UTF-8", static_cast<std::uint32_t>(point)));
  unsigned count = point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
  if (bytes.size() < count)
    return std::unexpected(UnicodeException("Insufficient output capacity while encoding UTF-8"));
  if (count == 1) bytes[0] = static_cast<char>(point);
  else {
    for (unsigned i = count - 1; i != 0; --i) { bytes[i] = static_cast<char>(0x80 | (point & 0x3f)); point >>= 6; }
    bytes[0] = static_cast<char>((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | point);
  }
  return count;
}

std::expected<Utf32Type, StarException> utf32_from_utf16_surrogate_pair(Utf32Type lead, Utf32Type trail) {
  if (!is_utf16_lead_surrogate(lead)) return std::unexpected(UnicodeException("Invalid lead surrogate passed to utf32FromUtf16SurrogatePair"));
  if (!is_utf16_trail_surrogate(trail)) return std::unexpected(UnicodeException("Invalid trail surrogate passed to utf32FromUtf16SurrogatePair"));
  return static_cast<Utf32Type>(((lead - 0xd800) << 10) + (trail - 0xdc00) + 0x10000);
}
std::expected<std::pair<Utf32Type, std::optional<Utf32Type>>, StarException> utf32_to_utf16_surrogate_pair(Utf32Type point) {
  if (point > 0x10ffff) return std::unexpected(UnicodeException("Invalid codepoint passed to utf32ToUtf16SurrogatePair"));
  if (point < 0x10000) return std::pair<Utf32Type, std::optional<Utf32Type>>{point, {}};
  point -= 0x10000;
  return std::pair<Utf32Type, std::optional<Utf32Type>>{static_cast<Utf32Type>((point >> 10) + 0xd800), static_cast<Utf32Type>((point & 0x3ff) + 0xdc00)};
}
std::expected<Utf32Type, StarException> hex_string_to_utf32(std::string_view hex, std::optional<Utf32Type> previous) {
  auto count = hex.size() / 2;
  if (count > sizeof(Utf32Type))
    return std::unexpected(UnicodeException("Codepoint size is too big in hexStringToUtf32"));
  auto nibble = [](char byte) -> std::uint32_t {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return 0;
  };
  std::uint32_t point = 0;
  for (std::size_t i = 0; i != count; ++i)
    point = (point << 8) | (nibble(hex[2 * i]) << 4) | nibble(hex[2 * i + 1]);
  if (previous && is_utf16_lead_surrogate(*previous)) return utf32_from_utf16_surrogate_pair(*previous, static_cast<Utf32Type>(point));
  return static_cast<Utf32Type>(point);
}
std::expected<std::string, StarException> hex_string_from_utf32(Utf32Type point) {
  auto pair = utf32_to_utf16_surrogate_pair(point);
  if (!pair) return std::unexpected(std::move(pair.error()));
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  auto append = [&](Utf32Type unit) {
    for (int shift = 12; shift >= 0; shift -= 4) result.push_back(digits[(unit >> shift) & 0xf]);
  };
  append(pair->first);
  if (pair->second) append(*pair->second);
  return result;
}
}
