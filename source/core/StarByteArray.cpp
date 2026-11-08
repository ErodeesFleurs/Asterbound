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
    std::memcpy(ba.m_data, str.data(), str.size());
  ba.m_data[str.size()] = 0;
  return ba;
}

ByteArray ByteArray::with_reserve(std::size_t capacity) {
  ByteArray bytes;
  bytes.reserve(capacity);
  return bytes;
}

ByteArray::ByteArray() {
  m_data = nullptr;
  m_capacity = 0;
  m_size = 0;
}

ByteArray::ByteArray(std::size_t dataSize, char c)
  : ByteArray() {
  fill(dataSize, c);
}

ByteArray::ByteArray(std::span<char const> data)
  : ByteArray() {
  resize(data.size());
  if (!data.empty())
    std::memcpy(m_data, data.data(), data.size());
}

ByteArray::ByteArray(ByteArray const& b)
  : ByteArray(b.span()) {
}

ByteArray::ByteArray(ByteArray&& b) noexcept
  : ByteArray() {
  operator=(std::move(b));
}

ByteArray::~ByteArray() {
  reset();
}

ByteArray& ByteArray::operator=(ByteArray const& b) {
  if (&b != this) {
    resize(b.m_size);
    if (m_size != 0)
      std::memcpy(m_data, b.m_data, m_size);
  }

  return *this;
}

ByteArray& ByteArray::operator=(ByteArray&& b) noexcept {
  if (&b != this) {
    reset();

    m_data = std::exchange(b.m_data, nullptr);
    m_capacity = std::exchange(b.m_capacity, 0);
    m_size = std::exchange(b.m_size, 0);
  }

  return *this;
}

void ByteArray::reset() {
  if (m_data) {
    Star::free(m_data, m_capacity);
    m_data = nullptr;
    m_capacity = 0;
    m_size = 0;
  }
}

void ByteArray::reserve(std::size_t newCapacity) {
  if (newCapacity > max_size())
    Star::fatal_error("ByteArray capacity exceeds max_size", false);
  if (newCapacity > m_capacity) {
    if (!m_data) {
      auto newMem = static_cast<char*>(Star::malloc(newCapacity));
      if (!newMem)
        Star::fatal_error("Could not allocate ByteArray capacity", false);
      m_data = newMem;
      m_capacity = newCapacity;
    } else {
      auto grown = m_capacity > max_size() / 2 ? max_size() : m_capacity * 2;
      newCapacity = std::max({grown, newCapacity, static_cast<std::size_t>(8)});
      auto newMem = static_cast<char*>(Star::realloc(m_data, newCapacity));
      if (!newMem)
        Star::fatal_error("Could not allocate ByteArray capacity", false);
      m_data = newMem;
      m_capacity = newCapacity;
    }
  }
}

void ByteArray::shrink_to_fit() {
  if (m_size == 0) {
    reset();
  } else if (m_capacity != m_size) {
    auto memory = static_cast<char*>(Star::realloc(m_data, m_size));
    if (!memory)
      Star::fatal_error("Could not shrink ByteArray capacity", false);
    m_data = memory;
    m_capacity = m_size;
  }
}

void ByteArray::resize(std::size_t size, char f) {
  if (m_size == size)
    return;

  std::size_t oldSize = m_size;
  resize(size);
  if (m_size > oldSize)
    std::memset(m_data + oldSize, f, m_size - oldSize);
}

void ByteArray::fill(std::size_t s, char c) {
  if (s != NPos)
    resize(s);

  if (m_size == 0)
    return;

  std::memset(m_data, c, m_size);
}

void ByteArray::fill(char c) {
  fill(NPos, c);
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
    std::memmove(m_data, m_data + s, m_size - s);
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
  auto mismatch = std::mismatch(m_data, m_data + size, b.m_data);
  return static_cast<std::size_t>(mismatch.first - m_data);
}

int ByteArray::compare(const ByteArray& b) const {
  auto commonSize = std::min(m_size, b.m_size);
  if (commonSize != 0) {
    int comparison = std::memcmp(m_data, b.m_data, commonSize);
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
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((pos <= m_size));
  }
  auto old_size = m_size;
  resize(checked_size_sum(old_size, 1));
  std::memmove(m_data + pos + 1, m_data + pos, old_size - pos);
  m_data[pos] = byte;
}

ByteArray::iterator ByteArray::insert(const_iterator pos, char byte) & {
  std::size_t d = pos - begin();
  insert(d, byte);
  return begin() + d;
}


bool ByteArray::operator<(const ByteArray& b) const {
  return compare(b) < 0;
}

bool ByteArray::operator==(const ByteArray& b) const {
  return m_size == b.m_size && (m_size == 0 || std::memcmp(m_data, b.m_data, m_size) == 0);
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
    auto size = hexEncode(input, encoded);
    buffer.append(std::string_view(encoded.data(), size));
    remaining = remaining.subspan(input.size());
  }
}

}
