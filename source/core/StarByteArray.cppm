module;
import std;
import star.text_formatter;
import star.hash;

import star.format;

namespace Star {

// The callback is borrowed and invoked as an lvalue; byte assignment permits
// normal integral promotions without requiring a copyable or pure callback.
template <class Combiner>
concept byte_combiner = requires(Combiner& combine, char& output) {
  output = combine(char{}, char{});
};

// Class to hold an array of bytes.  Contains an internal buffer that may be
// larger than what is reported by size(), to avoid repeated allocations when a
// ByteArray grows.
class ByteArray {
public:
  using value_type = char;
  using iterator = char*;
  using const_iterator = char const*;

  // Constructs bytes from text without or with a trailing NUL byte.
  static ByteArray from_string(std::string_view str);
  static ByteArray from_string_with_null(std::string_view str);
  static ByteArray with_reserve(std::size_t capacity);

  ByteArray();
  ByteArray(std::size_t dataSize, char c);
  ByteArray(std::span<char const> data);
  ByteArray(ByteArray const& b);
  ByteArray(ByteArray&& b) noexcept;
  ~ByteArray();

  ByteArray& operator=(ByteArray const& b);
  ByteArray& operator=(ByteArray&& b) noexcept;

  char const* data() const & noexcept;
  char* data() & noexcept;
  char const* data() const && = delete;
  std::span<char const> span() const & noexcept { return {m_data, m_size}; }
  std::span<char> span() & noexcept { return {m_data, m_size}; }
  std::span<char const> span() const && = delete;

  std::size_t size() const;
  // Maximum size before realloc
  std::size_t capacity() const;
  static constexpr std::size_t max_size() noexcept {
    return static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  }
  // Is zero size
  bool empty() const;

  // Sets size to 0.
  void clear();
  // Clears and resets buffer to empty.
  void reset();

  void reserve(std::size_t capacity);
  void shrink_to_fit();

  void resize(std::size_t size);
  // resize, filling new space with given byte if it exists.
  void resize(std::size_t size, char f);

  // fill array with byte.
  void fill(char c);
  // fill array and resize to new size.
  void fill(std::size_t size, char c);

  void append(ByteArray const& b);
  void append(std::span<char const> data);
  void push_back(char byte);

  void copy_to(std::span<char> data) const;

  // Copy from ByteArray starting at pos, clamped to the destination range.
  void copy_to(std::span<char> data, std::size_t pos) const;
  // Aliased sources retain their offset across growth; overlapping writes work.
  void write_from(std::span<char const> data, std::size_t pos);

  ByteArray substr(std::size_t pos, std::size_t count) const;

  void remove_prefix(std::size_t count);
  void remove_suffix(std::size_t count);

  // returns location of first character that is different than the given
  // ByteArray.
  std::size_t mismatch_index(ByteArray const& b) const;
  // returns -1 if this < b, 0 if this == b, 1 if this > b
  int compare(ByteArray const& b) const;

  template <byte_combiner Combiner>
  ByteArray combine(Combiner&& combine, ByteArray const& rhs, bool extend = false) const;

  ByteArray bit_and(ByteArray const& rhs, bool extend = false) const;
  ByteArray bit_or(ByteArray const& rhs, bool extend = false) const;
  ByteArray bit_xor(ByteArray const& rhs, bool extend = false) const;

  iterator begin() & noexcept;
  iterator end() & noexcept;
  const_iterator begin() const & noexcept;
  const_iterator end() const & noexcept;
  const_iterator begin() const && = delete;
  const_iterator end() const && = delete;

  void insert(std::size_t pos, char byte);
  iterator insert(const_iterator pos, char byte) &;
  iterator insert(const_iterator pos, char byte) && = delete;

  char& operator[](std::size_t i);
  char operator[](std::size_t i) const;
  [[nodiscard]] std::expected<std::reference_wrapper<char const>, StarException> at(std::size_t i) const &;
  std::expected<std::reference_wrapper<char const>, StarException> at(std::size_t) const && = delete;

  bool operator<(ByteArray const& b) const;
  bool operator==(ByteArray const& b) const;
  bool operator!=(ByteArray const& b) const;

private:
  static std::size_t checked_size_sum(std::size_t pos, std::size_t count) {
    if (pos > max_size() || count > max_size() - pos)
      Star::fatal_error("ByteArray size exceeds max_size", false);
    return pos + count;
  }

  char* m_data;
  std::size_t m_capacity;
  std::size_t m_size;
};

template <>
struct hash<ByteArray> {
  std::size_t operator()(ByteArray const& b) const;
};

void write_text(TextBuffer& buffer, ByteArray const& b);

inline void ByteArray::clear() {
  resize(0);
}

inline void ByteArray::resize(std::size_t size) {
  if (size > m_capacity)
    reserve(size);
  m_size = size;
}

inline void ByteArray::append(ByteArray const& b) {
  append(b.span());
}

inline void ByteArray::append(std::span<char const> data) {
  write_from(data, m_size);
}

inline void ByteArray::push_back(char byte) {
  if (m_size == m_capacity)
    reserve(checked_size_sum(m_size, 1));
  m_data[m_size++] = byte;
}

inline bool ByteArray::empty() const {
  return m_size == 0;
}

inline char const* ByteArray::data() const & noexcept {
  return m_data;
}

inline char* ByteArray::data() & noexcept {
  return m_data;
}

inline std::size_t ByteArray::size() const {
  return m_size;
}

inline std::size_t ByteArray::capacity() const {
  return m_capacity;
}

inline void ByteArray::copy_to(std::span<char> data) const {
  auto len = std::min(m_size, data.size());
  if (len == 0)
    return;

  std::memmove(data.data(), m_data, len);
}

inline void ByteArray::copy_to(std::span<char> data, std::size_t pos) const {
  auto len = data.size();
  if (len == 0 || pos >= m_size)
    return;

  len = std::min(m_size - pos, len);
  std::memmove(data.data(), m_data + pos, len);
}

inline void ByteArray::write_from(std::span<char const> data, std::size_t pos) {
  auto count = data.size();
  if (count == 0)
    return;
  auto stop = checked_size_sum(pos, count);
  auto source = data.data();
  if (stop > m_capacity) {
    // Address arithmetic avoids subtracting pointers from unrelated objects.
    // Sources may also refer to initialized bytes in our reserved tail.
    auto address = reinterpret_cast<std::uintptr_t>(source);
    auto base = reinterpret_cast<std::uintptr_t>(m_data);
    auto offset = address - base;
    bool internal = m_data && address >= base && offset < m_capacity;
    resize(stop);
    if (internal)
      source = m_data + offset;
  } else if (stop > m_size) {
    m_size = stop;
  }
  std::memmove(m_data + pos, source, count);
}

template <byte_combiner Combiner>
ByteArray ByteArray::combine(Combiner&& combine, ByteArray const& rhs, bool extend) const {
  auto smaller = rhs.span();
  auto larger = span();
  if (larger.size() < smaller.size())
    std::swap(smaller, larger);
  ByteArray result;
  result.resize(extend ? larger.size() : smaller.size());
  for (std::size_t i = 0; i < smaller.size(); ++i)
    result[i] = combine(char(smaller[i]), char(larger[i]));
  if (extend && larger.size() != smaller.size())
    std::memcpy(result.m_data + smaller.size(), larger.data() + smaller.size(), larger.size() - smaller.size());
  return result;
}

inline ByteArray::iterator ByteArray::begin() & noexcept {
  return m_data;
}

inline ByteArray::iterator ByteArray::end() & noexcept {
  return m_data + m_size;
}

inline ByteArray::const_iterator ByteArray::begin() const & noexcept {
  return m_data;
}

inline ByteArray::const_iterator ByteArray::end() const & noexcept {
  return m_data + m_size;
}

inline char& ByteArray::operator[](std::size_t i) {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((i < m_size));
  }
  return m_data[i];
}

inline char ByteArray::operator[](std::size_t i) const {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((i < m_size));
  }
  return m_data[i];
}

[[nodiscard]] inline std::expected<std::reference_wrapper<char const>, StarException> ByteArray::at(std::size_t i) const & {
  if (i >= m_size)
    return std::unexpected(OutOfRangeException(Star::format("Out of range in ByteArray::at({})", i)));

  return std::cref(m_data[i]);
}

inline std::size_t hash<ByteArray>::operator()(ByteArray const& b) const { return pl_hash_bytes(b.span()); }
}

export module star.byte_array;

export namespace Star {
  template <>
  struct hash<ByteArray>;
}


export template <> struct std::formatter<Star::ByteArray> : Star::TextFormatter {};

export namespace Star {
  using ::Star::ByteArray;
  using ::Star::byte_combiner;
  using ::Star::hash;
  using ::Star::write_text;
}
