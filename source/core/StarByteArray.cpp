import std;
import star.text_formatter;
import star.format;
import star.string;
import star.byte_array;
import star.memory;
import star.encode;

import star.algorithm;
namespace Star {

ByteArray ByteArray::from_string(std::string_view str) {
  return ByteArray(std::span<char const>(str));
}

ByteArray ByteArray::from_string_with_null(std::string_view str) {
  ByteArray ba;
  ba.resize(checked_size_sum(str.size(), 1));
  if (!str.empty())
    std::memcpy(ba.span().data(), str.data(), str.size());
  ba[str.size()] = 0;
  return ba;
}

ByteArray ByteArray::with_reserve(std::size_t capacity) {
  ByteArray bytes;
  bytes.reserve(capacity);
  return bytes;
}

ByteArray::ByteArray(std::size_t size, char c) {
  fill(size, c);
}

ByteArray::ByteArray(std::span<char const> data) {
  resize(data.size());
  if (!data.empty())
    std::memcpy(span().data(), data.data(), data.size());
}

ByteArray::ByteArray(ByteArray const& b)
  : ByteArray(b.span()) {
}

ByteArray::ByteArray(ByteArray&& b) noexcept
  : m_storage(std::move(b.m_storage)), m_size(std::exchange(b.m_size, 0)) {}

ByteArray& ByteArray::operator=(ByteArray const& b) {
  if (&b != this) {
    resize(b.m_size);
    if (m_size != 0)
      std::memcpy(span().data(), b.span().data(), m_size);
  }

  return *this;
}

ByteArray& ByteArray::operator=(ByteArray&& b) noexcept {
  if (&b != this) {
    m_storage = std::move(b.m_storage);
    m_size = std::exchange(b.m_size, 0);
  }

  return *this;
}

void ByteArray::reset() {
  m_storage = {};
  m_size = 0;
}

void ByteArray::reserve(std::size_t new_capacity) {
  if (new_capacity <= capacity())
    return;
  if (new_capacity > max_size())
    Star::fatal_error("ByteArray capacity exceeds max_size", false);
  if (capacity() != 0) {
    auto grown = capacity() > max_size() / 2 ? max_size() : capacity() * 2;
    new_capacity = std::max({grown, new_capacity, std::size_t{8}});
  }
  if (!m_storage.resize(new_capacity))
    Star::fatal_error("Could not allocate ByteArray capacity", false);
}

void ByteArray::shrink_to_fit() {
  if (m_size != capacity() && !m_storage.resize(m_size))
    Star::fatal_error("Could not shrink ByteArray capacity", false);
}

void ByteArray::resize(std::size_t size, char f) {
  if (m_size == size)
    return;

  std::size_t old_size = m_size;
  resize(size);
  if (m_size > old_size)
    std::memset(span().subspan(old_size).data(), f, m_size - old_size);
}

void ByteArray::fill(std::size_t s, char c) {
  if (s != npos)
    resize(s);

  if (m_size == 0)
    return;

  std::memset(span().data(), c, m_size);
}

void ByteArray::fill(char c) {
  fill(npos, c);
}

ByteArray ByteArray::substr(std::size_t b, std::size_t s) const {
  if (b == 0 && s >= m_size) {
    return ByteArray(*this);
  } else if (b >= m_size) {
    return ByteArray();
  } else {
    // The sub range has to be clamped against what is left *after* the offset;
    // min(m_size, b + s) let the copy read past the end of the buffer.
    return ByteArray(span().subspan(b, std::min(s, m_size - b)));
  }
}


void ByteArray::remove_prefix(std::size_t s) {
  if (s >= m_size) {
    clear();
  } else {
    std::memmove(span().data(), span().subspan(s).data(), m_size - s);
    resize(m_size - s);
  }
}

void ByteArray::remove_suffix(std::size_t s) {
  if (s >= m_size)
    clear();
  else
    resize(m_size - s);
}

std::size_t ByteArray::mismatch_index(const ByteArray& b) const {
  auto size = std::min(m_size, b.size());
  if (size == 0)
    return 0;
  auto mismatch = std::mismatch(begin(), begin() + size, b.begin());
  return static_cast<std::size_t>(mismatch.first - begin());
}

int ByteArray::compare(const ByteArray& b) const {
  auto common_size = std::min(m_size, b.m_size);
  if (common_size != 0) {
    int comparison = std::memcmp(span().data(), b.span().data(), common_size);
    if (comparison != 0)
      return (comparison > 0) - (comparison < 0);
  }
  return (m_size > b.m_size) - (m_size < b.m_size);
}

ByteArray ByteArray::bit_and(ByteArray const& rhs, bool extend) const {
  return combine([](char a, char b) { return a & b; }, rhs, extend);
}

ByteArray ByteArray::bit_or(ByteArray const& rhs, bool extend) const {
  return combine([](char a, char b) { return a | b; }, rhs, extend);
}

ByteArray ByteArray::bit_xor(ByteArray const& rhs, bool extend) const {
  return combine([](char a, char b) { return a ^ b; }, rhs, extend);
}

void ByteArray::insert(std::size_t pos, char byte) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((pos <= m_size));
  }
  auto old_size = m_size;
  resize(checked_size_sum(old_size, 1));
  std::memmove(span().subspan(pos + 1).data(), span().subspan(pos).data(), old_size - pos);
  (*this)[pos] = byte;
}

ByteArray::iterator ByteArray::insert(const_iterator pos, char byte) & {
  auto d = static_cast<std::size_t>(pos - std::as_const(*this).begin());
  insert(d, byte);
  return begin() + d;
}


bool ByteArray::operator<(const ByteArray& b) const {
  return compare(b) < 0;
}

bool ByteArray::operator==(const ByteArray& b) const {
  return m_size == b.m_size && (m_size == 0 || std::memcmp(span().data(), b.span().data(), m_size) == 0);
}

bool ByteArray::operator!=(const ByteArray& b) const {
  return !(*this == b);
}

void write_text(TextBuffer& buffer, ByteArray const& b) {
  buffer.append("0x");
  std::array<char, 256> encoded;
  auto remaining = b.span();
  while (!remaining.empty()) {
    auto input = remaining.first(std::min(remaining.size(), encoded.size() / 2));
    auto size = hex_encode(input, encoded);
    buffer.append(std::string_view(encoded.data(), size));
    remaining = remaining.subspan(input.size());
  }
}

}
