module;
import std;
import star.format;

namespace Star {
struct UnicodeExceptionTag { static constexpr std::string_view name() { return "UnicodeException"; } };
using UnicodeException = StarError<UnicodeExceptionTag, StarException>;
using Utf8Type = char;
using Utf32Type = char32_t;
inline constexpr long STAR_UTF32_REPLACEMENT_CHAR = 0x000000b7L;

// Bounded inputs preserve embedded NUL; pointer-only inputs are NUL-terminated.
[[nodiscard]] std::expected<std::size_t, StarException> utf8_length(std::string_view);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_length(Utf8Type const*);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_decode_char(std::string_view, Utf32Type&);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_decode_char(Utf8Type const*, Utf32Type&);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_encode_char(std::span<Utf8Type>, Utf32Type);
[[nodiscard]] std::expected<Utf32Type, StarException> hex_string_to_utf32(std::string_view, std::optional<Utf32Type> previous = {});
[[nodiscard]] std::expected<std::string, StarException> hex_string_from_utf32(Utf32Type);
constexpr bool is_utf16_lead_surrogate(Utf32Type point) noexcept { return point >= 0xd800 && point <= 0xdbff; }
constexpr bool is_utf16_trail_surrogate(Utf32Type point) noexcept { return point >= 0xdc00 && point <= 0xdfff; }
[[nodiscard]] std::expected<Utf32Type, StarException> utf32_from_utf16_surrogate_pair(Utf32Type, Utf32Type);
[[nodiscard]] std::expected<std::pair<Utf32Type, std::optional<Utf32Type>>, StarException> utf32_to_utf16_surrogate_pair(Utf32Type);

namespace unicode_detail {
enum class decode_error : unsigned char {
  none,
  invalid_sequence,
  missing_trailing_units,
  invalid_codepoint,
  past_end
};

constexpr unsigned char byte_count(unsigned char first) noexcept {
  return first < 0x80 ? 1 : (first & 0xe0) == 0xc0 ? 2
      : (first & 0xf0) == 0xe0 ? 3 : (first & 0xf8) == 0xf0 ? 4 : 1;
}

// Compact validation shared by bulk traversal and public checked boundaries.
// Only a successful, nonempty decode modifies output.
constexpr std::expected<unsigned char, decode_error> decode_char(
    char const* bytes, Utf32Type& output, std::size_t remain, bool terminated = false) noexcept {
  if (remain == 0 || (terminated && *bytes == 0)) return 0;
  auto first = static_cast<unsigned char>(bytes[0]);
  unsigned char count;
  std::uint32_t point;
  std::uint32_t minimum;
  if (first < 0x80) { count = 1; point = first; minimum = 0; }
  else if ((first & 0xe0) == 0xc0) { count = 2; point = first & 0x1f; minimum = 0x80; }
  else if ((first & 0xf0) == 0xe0) { count = 3; point = first & 0x0f; minimum = 0x800; }
  else if ((first & 0xf8) == 0xf0) { count = 4; point = first & 7; minimum = 0x10000; }
  else return std::unexpected(decode_error::invalid_sequence);
  if (remain < count) return std::unexpected(decode_error::missing_trailing_units);
  for (unsigned i = 1; i != count; ++i) {
    auto next = static_cast<unsigned char>(bytes[i]);
    if (terminated && next == 0) return std::unexpected(decode_error::missing_trailing_units);
    if ((next & 0xc0) != 0x80) return std::unexpected(decode_error::invalid_sequence);
    point = (point << 6) | (next & 0x3f);
  }
  if (point < minimum || point > 0x10ffff)
    return std::unexpected(decode_error::invalid_codepoint);
  output = static_cast<Utf32Type>(point);
  return count;
}

// Construct owning diagnostics only on failure, with normal cause/trace policy.
StarException decode_exception(decode_error);

template <class Iterator>
concept contiguous_byte_iterator = std::contiguous_iterator<Iterator>
    && (!std::is_volatile_v<std::remove_reference_t<std::iter_reference_t<Iterator>>>)
    && (std::same_as<std::iter_value_t<Iterator>, char>
        || std::same_as<std::iter_value_t<Iterator>, signed char>
        || std::same_as<std::iter_value_t<Iterator>, unsigned char>
        || std::same_as<std::iter_value_t<Iterator>, char8_t>);
}

template <class Visitor>
concept codepoint_visitor = requires(Visitor& visitor) {
  std::invoke(visitor, char32_t{}, std::size_t{}, std::size_t{});
} && (std::same_as<std::invoke_result_t<Visitor&, char32_t, std::size_t, std::size_t>, void>
    || std::same_as<std::invoke_result_t<Visitor&, char32_t, std::size_t, std::size_t>,
        std::expected<void, StarException>>);

// The borrowed visitor is invoked as an lvalue with independent value arguments:
// (codepoint, byte offset, byte width). Stop at the first decode/callback error,
// without reading or validating any unvisited suffix.
template <class Visitor> requires codepoint_visitor<Visitor>
[[nodiscard]] std::expected<void, StarException> for_each_codepoint(std::string_view bytes, Visitor&& visitor) {
  std::size_t offset = 0;
  while (offset != bytes.size()) {
    Utf32Type point;
    auto width = unicode_detail::decode_char(bytes.data() + offset, point, bytes.size() - offset);
    if (!width) return std::unexpected(unicode_detail::decode_exception(width.error()));
    if constexpr (std::same_as<std::invoke_result_t<Visitor&, char32_t, std::size_t, std::size_t>, void>) {
      std::invoke(visitor, char32_t(point), std::size_t(offset), std::size_t(*width));
    } else {
      auto result = std::invoke(visitor, char32_t(point), std::size_t(offset), std::size_t(*width));
      if (!result) return result;
    }
    offset += *width;
  }
  return {};
}

// Lazy checked cursor for short-circuit algorithms over unvalidated bytes.
class Utf8Cursor {
public:
  explicit Utf8Cursor(std::string_view bytes) : m_bytes(bytes) {}
  template <class Owner>
    requires std::same_as<std::remove_cvref_t<Owner>, std::string> && (!std::is_lvalue_reference_v<Owner>)
  Utf8Cursor(Owner&&) = delete;
  static Utf8Cursor end(std::string_view bytes) {
    Utf8Cursor cursor(bytes);
    cursor.m_offset = bytes.size();
    return cursor;
  }
  template <class Owner>
    requires std::same_as<std::remove_cvref_t<Owner>, std::string> && (!std::is_lvalue_reference_v<Owner>)
  static Utf8Cursor end(Owner&&) = delete;
  bool at_end() const { return m_offset == m_bytes.size(); }
  std::size_t offset() const { return m_offset; }
  [[nodiscard]] std::expected<Utf32Type, StarException> read() const;
  [[nodiscard]] std::expected<void, StarException> advance();
  [[nodiscard]] std::expected<void, StarException> retreat();
private:
  [[nodiscard]] unicode_detail::decode_error decode() const;
  std::string_view m_bytes;
  std::size_t m_offset = 0;
  mutable std::size_t m_width = 0;
  mutable Utf32Type m_point = 0;
};

template <class Iterator>
concept utf8_byte_iterator = requires(Iterator position, Iterator const& other, char& byte) {
  Iterator(other);
  Iterator(std::move(position));
  ++position;
  --position;
  bool(position == other);
  bool(position != other);
  byte = *position;
  static_cast<unsigned char>(*position);
};

template <class Iterator, class Codepoint>
concept utf8_byte_writer = requires(Iterator position, Codepoint point, char byte) {
  Iterator(std::move(position));
  static_cast<Utf32Type>(point);
  *position++ = byte;
};

// Validation happens at creation, not through an error channel hidden in a
// standard iterator operation. Positions must remain within the validated range.
template <class BaseIterator, class U32Type = Utf32Type>
  requires utf8_byte_iterator<BaseIterator> && std::is_object_v<U32Type>
      && requires(std::uint32_t point) { static_cast<U32Type>(point); }
class U8ToU32Iterator {
public:
  using difference_type = std::ptrdiff_t;
  using value_type = U32Type;
  using pointer = void;
  using reference = U32Type;
  using iterator_concept = std::bidirectional_iterator_tag;
  // Legacy forward iterators require an lvalue reference; codepoints are values.
  using iterator_category = std::input_iterator_tag;
  U8ToU32Iterator() noexcept(std::is_nothrow_default_constructible_v<BaseIterator>)
    requires std::default_initializable<BaseIterator> : m_position{} {}
  [[nodiscard]] static std::expected<U8ToU32Iterator, StarException> create(BaseIterator first, BaseIterator last) {
    if constexpr (unicode_detail::contiguous_byte_iterator<BaseIterator>) {
      if (first != last) {
        auto validated = utf8_length(std::string_view(
            reinterpret_cast<char const*>(std::to_address(first)), static_cast<std::size_t>(last - first)));
        if (!validated) return std::unexpected(std::move(validated.error()));
      }
    } else {
      auto position = first;
      while (position != last) {
        char bytes[4];
        bytes[0] = *position;
        ++position;
        std::size_t available = 1;
        auto wanted = unicode_detail::byte_count(static_cast<unsigned char>(bytes[0]));
        while (available != wanted && position != last) {
          bytes[available++] = *position;
          ++position;
        }
        Utf32Type point;
        auto decoded = utf8_decode_char(std::string_view(bytes, available), point);
        if (!decoded) return std::unexpected(std::move(decoded.error()));
      }
    }
    return U8ToU32Iterator(first);
  }
  static U8ToU32Iterator end(BaseIterator last) { return U8ToU32Iterator(last); }
  BaseIterator base() const { return m_position; }
  reference operator*() const {
    auto next = m_position;
    auto first = static_cast<unsigned char>(*next);
    ++next;
    unsigned count = unicode_detail::byte_count(first);
    std::uint32_t point = first & (count == 1 ? 0x7fu : (0x7fu >> count));
    for (unsigned i = 1; i != count; ++i) {
      point = (point << 6) | (static_cast<unsigned char>(*next) & 0x3fu);
      ++next;
    }
    return static_cast<U32Type>(point);
  }
  U8ToU32Iterator& operator++() {
    auto count = unicode_detail::byte_count(static_cast<unsigned char>(*m_position));
    for (unsigned i = 0; i != count; ++i) ++m_position;
    return *this;
  }
  U8ToU32Iterator operator++(int) { auto old = *this; ++*this; return old; }
  U8ToU32Iterator& operator--() { do { --m_position; } while ((static_cast<unsigned char>(*m_position) & 0xc0u) == 0x80u); return *this; }
  U8ToU32Iterator operator--(int) { auto old = *this; --*this; return old; }
  bool operator==(U8ToU32Iterator const& other) const { return static_cast<bool>(m_position == other.m_position); }
private:
  explicit U8ToU32Iterator(BaseIterator position) : m_position(std::move(position)) {}
  BaseIterator m_position;
};

// An explicit checked writer, rather than a standard output iterator whose
// assignment result would be discarded by algorithms.
template <class BaseIterator, class U32Type = Utf32Type>
  requires utf8_byte_writer<BaseIterator, U32Type>
class Utf8OutputIterator {
public:
  explicit Utf8OutputIterator(BaseIterator position) : m_position(std::move(position)) {}
  [[nodiscard]] std::expected<void, StarException> push(U32Type point) {
    char bytes[4];
    auto encoded = utf8_encode_char(bytes, static_cast<Utf32Type>(point));
    if (!encoded) return std::unexpected(std::move(encoded.error()));
    for (std::size_t i = 0; i != *encoded; ++i) *m_position++ = bytes[i];
    return {};
  }
private:
  BaseIterator m_position;
};
}
export module star.unicode;
export namespace Star {
  using ::Star::UnicodeExceptionTag;
  using ::Star::UnicodeException;
  using ::Star::Utf8Type;
  using ::Star::Utf32Type;
  using ::Star::STAR_UTF32_REPLACEMENT_CHAR;
  using ::Star::utf8_length;
  using ::Star::utf8_decode_char;
  using ::Star::utf8_encode_char;
  using ::Star::hex_string_to_utf32;
  using ::Star::hex_string_from_utf32;
  using ::Star::is_utf16_lead_surrogate;
  using ::Star::is_utf16_trail_surrogate;
  using ::Star::utf32_from_utf16_surrogate_pair;
  using ::Star::utf32_to_utf16_surrogate_pair;
  using ::Star::U8ToU32Iterator;
  using ::Star::Utf8OutputIterator;
  using ::Star::Utf8Cursor;
  using ::Star::utf8_byte_iterator;
  using ::Star::utf8_byte_writer;
  using ::Star::codepoint_visitor;
  using ::Star::for_each_codepoint;
}
export namespace Star::unicode_detail {
  using ::Star::unicode_detail::decode_error;
  using ::Star::unicode_detail::decode_char;
  using ::Star::unicode_detail::decode_exception;
}
