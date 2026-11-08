module;
import star.memory;
import std;
import star.format;

namespace Star {
struct UnicodeExceptionTag { static constexpr std::string_view name() { return "UnicodeException"; } };
using UnicodeException = StarError<UnicodeExceptionTag, StarException>;
using Utf8Type = char;
using Utf32Type = char32_t;
inline constexpr long STAR_UTF32_REPLACEMENT_CHAR = 0x000000b7L;

// NPos denotes a null-terminated sequence; an explicit size preserves embedded NUL.
[[nodiscard]] std::expected<std::size_t, StarException> utf8_length(Utf8Type const*, std::size_t size = NPos);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_decode_char(Utf8Type const*, Utf32Type*, std::size_t len = NPos);
[[nodiscard]] std::expected<std::size_t, StarException> utf8_encode_char(Utf8Type*, Utf32Type, std::size_t len = 6);
[[nodiscard]] std::expected<Utf32Type, StarException> hex_string_to_utf32(std::string const&, std::optional<Utf32Type> previousCodepoint = {});
[[nodiscard]] std::expected<std::string, StarException> hex_string_from_utf32(Utf32Type);
bool is_utf16_lead_surrogate(Utf32Type);
bool is_utf16_trail_surrogate(Utf32Type);
[[nodiscard]] std::expected<Utf32Type, StarException> utf32_from_utf16_surrogate_pair(Utf32Type, Utf32Type);
[[nodiscard]] std::expected<std::pair<Utf32Type, std::optional<Utf32Type>>, StarException> utf32_to_utf16_surrogate_pair(Utf32Type);

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
  [[nodiscard]] std::expected<void, StarException> decode() const;
  std::string_view m_bytes;
  std::size_t m_offset = 0;
  mutable std::size_t m_width = 0;
  mutable Utf32Type m_point = 0;
};

template <class Iterator>
concept utf8_byte_iterator = std::bidirectional_iterator<Iterator>
    && std::convertible_to<std::iter_reference_t<Iterator>, char>
    && requires(Iterator position) {
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
  U8ToU32Iterator() = default;
  [[nodiscard]] static std::expected<U8ToU32Iterator, StarException> create(BaseIterator first, BaseIterator last) {
    if constexpr (std::contiguous_iterator<BaseIterator> && std::same_as<std::iter_value_t<BaseIterator>, char>) {
      auto validated = utf8_length(std::to_address(first), static_cast<std::size_t>(last - first));
      if (!validated) return std::unexpected(std::move(validated.error()));
    } else {
      auto position = first;
      while (position != last) {
        char bytes[4];
        std::size_t available = 0;
        auto next = position;
        while (available != 4 && next != last) bytes[available++] = *next++;
        Utf32Type point;
        auto decoded = utf8_decode_char(bytes, &point, available);
        if (!decoded) return std::unexpected(std::move(decoded.error()));
        std::advance(position, *decoded);
      }
    }
    return U8ToU32Iterator(first);
  }
  static U8ToU32Iterator end(BaseIterator last) { return U8ToU32Iterator(last); }
  BaseIterator base() const { return m_position; }
  reference operator*() const {
    auto next = m_position;
    auto first = static_cast<unsigned char>(*next++);
    unsigned count = byte_count(first);
    std::uint32_t point = first & (count == 1 ? 0x7fu : (0x7fu >> count));
    for (unsigned i = 1; i != count; ++i)
      point = (point << 6) | (static_cast<unsigned char>(*next++) & 0x3fu);
    return static_cast<U32Type>(point);
  }
  U8ToU32Iterator& operator++() { std::advance(m_position, byte_count(static_cast<unsigned char>(*m_position))); return *this; }
  U8ToU32Iterator operator++(int) { auto old = *this; ++*this; return old; }
  U8ToU32Iterator& operator--() { do { --m_position; } while ((static_cast<unsigned char>(*m_position) & 0xc0u) == 0x80u); return *this; }
  U8ToU32Iterator operator--(int) { auto old = *this; --*this; return old; }
  bool operator==(U8ToU32Iterator const& other) const { return m_position == other.m_position; }
private:
  explicit U8ToU32Iterator(BaseIterator position) : m_position(std::move(position)) {}
  static unsigned byte_count(unsigned char c) { return c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4; }
  BaseIterator m_position{};
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
    auto encoded = utf8_encode_char(bytes, static_cast<Utf32Type>(point), sizeof(bytes));
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
}
