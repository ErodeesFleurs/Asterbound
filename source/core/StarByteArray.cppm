module;
import std;
import star.text_formatter;
import star.hash;

import star.format;

namespace Star {

// Class to hold an array of bytes.  Contains an internal buffer that may be
// larger than what is reported by size(), to avoid repeated allocations when a
// ByteArray grows.
class ByteArray {
public:
  using value_type = char;
  using iterator = char*;
  using const_iterator = char const*;

  // Constructs bytes from text without or with a trailing NUL byte.
  static ByteArray fromCString(std::string_view str);
  static ByteArray fromCStringWithNull(std::string_view str);
  static ByteArray withReserve(std::size_t capacity);

  ByteArray();
  ByteArray(std::size_t dataSize, char c);
  ByteArray(std::span<char const> data);
  ByteArray(ByteArray const& b);
  ByteArray(ByteArray&& b) noexcept;
  ~ByteArray();

  ByteArray& operator=(ByteArray const& b);
  ByteArray& operator=(ByteArray&& b) noexcept;

  char const* ptr() const;
  char* ptr();
  std::span<char const> span() const { return {m_data, m_size}; }
  std::span<char> span() { return {m_data, m_size}; }

  std::size_t size() const;
  // Maximum size before realloc
  std::size_t capacity() const;
  // Is zero size
  bool empty() const;

  // Sets size to 0.
  void clear();
  // Clears and resets buffer to empty.
  void reset();

  void reserve(std::size_t capacity);

  void resize(std::size_t size);
  // resize, filling new space with given byte if it exists.
  void resize(std::size_t size, char f);

  // fill array with byte.
  void fill(char c);
  // fill array and resize to new size.
  void fill(std::size_t size, char c);

  void append(ByteArray const& b);
  void append(std::span<char const> data);
  void appendByte(char b);

  void copyTo(std::span<char> data) const;

  // Copy from ByteArray starting at pos, clamped to the destination range.
  void copyTo(std::span<char> data, std::size_t pos) const;
  // Copy the range to ByteArray at pos, resizing if needed.
  void writeFrom(std::span<char const> data, std::size_t pos);

  ByteArray sub(std::size_t b, std::size_t s) const;
  ByteArray left(std::size_t s) const;
  ByteArray right(std::size_t s) const;

  void trimLeft(std::size_t s);
  void trimRight(std::size_t s);

  // returns location of first character that is different than the given
  // ByteArray.
  std::size_t diffChar(ByteArray const& b) const;
  // returns -1 if this < b, 0 if this == b, 1 if this > b
  int compare(ByteArray const& b) const;

  template <typename Combiner>
  ByteArray combineWith(Combiner&& combine, ByteArray const& rhs, bool extend = false);

  ByteArray andWith(ByteArray const& rhs, bool extend = false);
  ByteArray orWith(ByteArray const& rhs, bool extend = false);
  ByteArray xorWith(ByteArray const& rhs, bool extend = false);

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  void insert(std::size_t pos, char byte);
  iterator insert(const_iterator pos, char byte);
  void push_back(char byte);

  char& operator[](std::size_t i);
  char operator[](std::size_t i) const;
  [[nodiscard]] std::expected<std::reference_wrapper<char const>, StarException> at(std::size_t i) const;

  bool operator<(ByteArray const& b) const;
  bool operator==(ByteArray const& b) const;
  bool operator!=(ByteArray const& b) const;

private:
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
  reserve(size);
  m_size = size;
}

inline void ByteArray::append(ByteArray const& b) {
  append(b.span());
}

inline void ByteArray::append(std::span<char const> data) {
  auto len = data.size();
  // memcpy with a null pointer (an empty ByteArray) and a length of 0 is still
  // undefined behaviour, and both sides can be null here.
  if (len == 0)
    return;

  resize(m_size + len);
  std::memcpy(m_data + m_size - len, data.data(), len);
}

inline void ByteArray::appendByte(char b) {
  resize(m_size + 1);
  m_data[m_size - 1] = b;
}

inline bool ByteArray::empty() const {
  return m_size == 0;
}

inline char const* ByteArray::ptr() const {
  return m_data;
}

inline char* ByteArray::ptr() {
  return m_data;
}

inline std::size_t ByteArray::size() const {
  return m_size;
}

inline std::size_t ByteArray::capacity() const {
  return m_capacity;
}

inline void ByteArray::copyTo(std::span<char> data) const {
  auto len = std::min(m_size, data.size());
  if (len == 0)
    return;

  std::memcpy(data.data(), m_data, len);
}

inline void ByteArray::copyTo(std::span<char> data, std::size_t pos) const {
  auto len = data.size();
  if (len == 0 || pos >= m_size)
    return;

  len = std::min(m_size - pos, len);
  std::memcpy(data.data(), m_data + pos, len);
}

inline void ByteArray::writeFrom(std::span<char const> data, std::size_t pos) {
  auto len = data.size();
  // Also avoids computing 'm_data + pos' for an empty array.
  if (len == 0)
    return;

  if (pos + len > m_size)
    resize(pos + len);

  std::memcpy(m_data + pos, data.data(), len);
}

template <typename Combiner>
ByteArray ByteArray::combineWith(Combiner&& combine, ByteArray const& rhs, bool extend) {
  auto smallerArray = std::cref(rhs);
  auto largerArray = std::cref(*this);

  if (m_size < rhs.size())
    std::swap(smallerArray, largerArray);

  ByteArray res;
  res.resize(smallerArray.get().size());

  for (std::size_t i = 0; i < smallerArray.get().size(); ++i)
    res[i] = combine(smallerArray.get()[i], largerArray.get()[i]);

  if (extend) {
    res.resize(largerArray.get().size());
    for (std::size_t i = smallerArray.get().size(); i < largerArray.get().size(); ++i)
      res[i] = largerArray.get()[i];
  }

  return res;
}

inline ByteArray::iterator ByteArray::begin() {
  return m_data;
}

inline ByteArray::iterator ByteArray::end() {
  return m_data + m_size;
}

inline ByteArray::const_iterator ByteArray::begin() const {
  return m_data;
}

inline ByteArray::const_iterator ByteArray::end() const {
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

[[nodiscard]] inline std::expected<std::reference_wrapper<char const>, StarException> ByteArray::at(std::size_t i) const {
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
  using ::Star::hash;
  using ::Star::write_text;
}
