import std;
import star.memory;
import star.unicode;
import star.format;
import star.encode;
import star.byte_array;
import star.string;

namespace Star {
std::expected<void, StarException> Utf8Cursor::decode() const {
  if (at_end()) return std::unexpected(UnicodeException("Cannot read past end of UTF-8 range"));
  if (m_width != 0) return {};
  auto width = utf8_decode_char(m_bytes.data() + m_offset, &m_point, m_bytes.size() - m_offset);
  if (!width) return std::unexpected(std::move(width.error()));
  m_width = *width;
  return {};
}
std::expected<Utf32Type, StarException> Utf8Cursor::read() const {
  auto result = decode();
  if (!result) return std::unexpected(std::move(result.error()));
  return m_point;
}
std::expected<void, StarException> Utf8Cursor::advance() {
  auto result = decode();
  if (!result) return std::unexpected(std::move(result.error()));
  m_offset += m_width;
  m_width = 0;
  return {};
}
std::expected<void, StarException> Utf8Cursor::retreat() {
  if (m_offset == 0) return std::unexpected(UnicodeException("Cannot move before beginning of UTF-8 range"));
  auto offset = m_offset - 1;
  while (offset != 0 && (static_cast<unsigned char>(m_bytes[offset]) & 0xc0) == 0x80) --offset;
  Utf32Type point;
  auto width = utf8_decode_char(m_bytes.data() + offset, &point, m_offset - offset);
  if (!width) return std::unexpected(std::move(width.error()));
  if (*width != m_offset - offset) return std::unexpected(UnicodeException("Invalid UTF-8 boundary"));
  m_offset = offset;
  m_width = *width;
  m_point = point;
  return {};
}

std::expected<std::size_t, StarException> utf8_decode_char(Utf8Type const* bytes, Utf32Type* output, std::size_t remain) {
  if (remain == 0 || (remain == NPos && *bytes == 0)) return 0;
  auto first = static_cast<unsigned char>(bytes[0]);
  unsigned count;
  std::uint32_t point;
  std::uint32_t minimum;
  if (first < 0x80) { count = 1; point = first; minimum = 0; }
  else if ((first & 0xe0) == 0xc0) { count = 2; point = first & 0x1f; minimum = 0x80; }
  else if ((first & 0xf0) == 0xe0) { count = 3; point = first & 0x0f; minimum = 0x800; }
  else if ((first & 0xf8) == 0xf0) { count = 4; point = first & 7; minimum = 0x10000; }
  else return std::unexpected(UnicodeException("Invalid UTF-8 code unit sequence"));
  if (remain != NPos && remain < count)
    return std::unexpected(UnicodeException("UTF-8 string missing trailing code units"));
  for (unsigned i = 1; i != count; ++i) {
    auto next = static_cast<unsigned char>(bytes[i]);
    if (remain == NPos && next == 0)
      return std::unexpected(UnicodeException("UTF-8 string missing trailing code units"));
    if ((next & 0xc0) != 0x80)
      return std::unexpected(UnicodeException("Invalid UTF-8 code unit sequence"));
    point = (point << 6) | (next & 0x3f);
  }
  if (point < minimum || point > 0x10ffff)
    return std::unexpected(UnicodeException("Invalid UTF-8 code point"));
  *output = static_cast<Utf32Type>(point);
  return count;
}

std::expected<std::size_t, StarException> utf8_length(Utf8Type const* bytes, std::size_t remain) {
  std::size_t length = 0;
  while (remain != 0) {
    Utf32Type point;
    auto decoded = utf8_decode_char(bytes, &point, remain);
    if (!decoded) return std::unexpected(std::move(decoded.error()));
    if (*decoded == 0) break;
    bytes += *decoded;
    if (remain != NPos) remain -= *decoded;
    ++length;
  }
  return length;
}

std::expected<std::size_t, StarException> utf8_encode_char(Utf8Type* bytes, Utf32Type point, std::size_t capacity) {
  if (point > 0x10ffff)
    return std::unexpected(UnicodeException::format("Invalid UTF-32 code point {} encountered while trying to encode UTF-8", static_cast<std::uint32_t>(point)));
  unsigned count = point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
  if (capacity < count)
    return std::unexpected(UnicodeException("Insufficient output capacity while encoding UTF-8"));
  if (count == 1) bytes[0] = static_cast<char>(point);
  else {
    for (unsigned i = count - 1; i != 0; --i) { bytes[i] = static_cast<char>(0x80 | (point & 0x3f)); point >>= 6; }
    bytes[0] = static_cast<char>((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | point);
  }
  return count;
}

bool is_utf16_lead_surrogate(Utf32Type point) { return point >= 0xd800 && point <= 0xdbff; }
bool is_utf16_trail_surrogate(Utf32Type point) { return point >= 0xdc00 && point <= 0xdfff; }
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
std::expected<Utf32Type, StarException> hex_string_to_utf32(std::string const& hex, std::optional<Utf32Type> previous) {
  auto bytes = hexDecode(hex);
  if (!bytes) return std::unexpected(std::move(bytes.error()));
  if (bytes->size() > sizeof(Utf32Type))
    return std::unexpected(UnicodeException("Codepoint size is too big in hexStringToUtf32"));
  std::uint32_t point = 0;
  for (std::size_t i = 0; i != bytes->size(); ++i)
    point = (point << 8) | static_cast<unsigned char>(bytes->span()[i]);
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
