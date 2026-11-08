import star.memory;
import star.text_formatter;
import star.string;
import star.string_view;
import star.hash;
import star.unicode;
import star.list;
import star.format;
import std;
#include <re2/re2.h>
import star.byte_array;
import star.algorithm;
import star.pythonic;

namespace Star {
namespace {
// Match reverse()'s validation direction without materializing a reversed owner.
std::expected<void, StarException> validate_reverse(std::string_view bytes) {
  auto cursor = Utf8Cursor::end(bytes);
  while (cursor.offset() != 0) {
    auto moved = cursor.retreat();
    if (!moved) return std::unexpected(std::move(moved.error()));
  }
  return {};
}

struct StringByteBounds {
  std::size_t length = 0;
  std::size_t start;
  std::size_t stop;
};

std::expected<StringByteBounds, StarException> string_byte_bounds(std::string_view bytes, std::size_t position, std::size_t count) {
  StringByteBounds bounds{0, bytes.size(), bytes.size()};
  std::size_t offset = 0;
  while (offset != bytes.size()) {
    if (bounds.length == position) bounds.start = offset;
    if (bounds.length >= position && bounds.length - position == count) bounds.stop = offset;
    Utf32Type point;
    auto width = utf8_decode_char(bytes.data() + offset, &point, bytes.size() - offset);
    if (!width) return std::unexpected(std::move(width.error()));
    offset += *width;
    ++bounds.length;
  }
  return bounds;
}

// Overflow remains the ordinary terminal standard-storage capacity failure.
// Reserving SIZE_MAX asks the storage to report it without wrapping the sum.
std::size_t byte_size_sum(std::size_t left, std::size_t right) {
  auto maximum = std::numeric_limits<std::size_t>::max();
  return right > maximum - left ? maximum : left + right;
}
}
bool String::is_space(Char c) { return c == 0x20 || c == 9 || c == 10 || c == 13 || c == 0xfeff; }
bool String::is_ascii_number(Char c) { return c >= '0' && c <= '9'; }
bool String::is_ascii_letter(Char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
String::Char String::to_lower(Char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
String::Char String::to_upper(Char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
bool String::char_equal(Char a, Char b, CaseSensitivity cs) { return cs == CaseInsensitive ? to_lower(a) == to_lower(b) : a == b; }
std::expected<String, StarException> String::join_with(String const& join, String const& left, String const& right) {
  String result = left;
  auto appended = append_joined(result, join, right);
  if (!appended) return std::unexpected(std::move(appended.error()));
  return result;
}

std::expected<void, StarException> String::append_joined(String& accumulator, String const& join, String const& right) {
  if (accumulator.empty()) { accumulator = right; return {}; }
  if (right.empty()) return {};
  auto leftJoined = accumulator.ends_with(join);
  if (!leftJoined) return std::unexpected(std::move(leftJoined.error()));
  auto rightJoined = right.starts_with(join);
  if (!rightJoined) return std::unexpected(std::move(rightJoined.error()));
  std::size_t skip = 0;
  if (*leftJoined && *rightJoined) {
    // The old right.substr() eagerly validated right even for an empty join.
    auto validated = right.size();
    if (!validated) return std::unexpected(std::move(validated.error()));
    skip = join.utf8_size();
  }
  bool insertJoin = !*leftJoined && !*rightJoined;
  if (insertJoin) accumulator.append(join);
  accumulator.append(std::string_view(right.m_string).substr(skip));
  return {};
}
String::String() = default;
String::String(String const& s) = default;
String::String(String&& s) noexcept = default;
String::String(std::string const& s) : m_string(s) {}
String::String(std::string&& s) noexcept : m_string(std::move(s)) {}
std::expected<String, StarException> String::create(std::wstring const& text) {
  String result;
  result.reserve(text.size());
  for (auto point : text) {
    auto appended = result.append(static_cast<Char>(point));
    if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return result;
}
std::expected<String, StarException> String::create(std::u32string_view text) {
  String result;
  result.reserve(text.size());
  auto appended = result.append(text);
  if (!appended) return std::unexpected(std::move(appended.error()));
  return result;
}
std::expected<String, StarException> String::create(Char point, std::size_t count) {
  if (count == 0) return String();
  char bytes[4];
  auto length = utf8_encode_char(bytes, point, sizeof(bytes));
  if (!length) return std::unexpected(std::move(length.error()));
  String result;
  result.reserve(count);
  for (std::size_t i = 0; i != count; ++i) result.append(std::string_view(bytes, *length));
  return result;
}
std::expected<String, StarException> String::create(Char point) { return create(point, 1); }
std::string const& String::utf8() const & { return m_string; }
std::string String::utf8() && noexcept { return std::move(m_string); }
std::string String::utf8() const && { return m_string; }
std::string String::take_utf8() { return std::exchange(m_string, {}); }
ByteArray String::utf8_bytes() const { return ByteArray(std::span<char const>(m_string)); }
char const* String::utf8_data() const & { return m_string.c_str(); }
std::size_t String::utf8_size() const { return m_string.size(); }
std::expected<std::wstring, StarException> String::wstring() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  std::wstring result;
  for (auto it = *first; it != end(); ++it) result.push_back(static_cast<wchar_t>(*it));
  return result;
}
std::expected<String::WideString, StarException> String::wide_string() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  WideString result;
  result.reserve(m_string.size());
  for (auto it = *first; it != end(); ++it) result.push_back(*it);
  return result;
}
std::expected<String::const_iterator, StarException> String::begin() const & { return const_iterator::create(m_string.begin(), m_string.end()); }
String::const_iterator String::end() const & { return const_iterator::end(m_string.end()); }
std::expected<std::size_t, StarException> String::size() const { return utf8_length(m_string.data(), m_string.size()); }
std::expected<std::size_t, StarException> String::length() const { return size(); }
void String::clear() { m_string.clear(); }
void String::reserve(std::size_t n) { m_string.reserve(n); }
bool String::empty() const { return m_string.empty(); }
std::expected<String::Char, StarException> String::operator[](std::size_t index) const { return StringView(*this).at(index); }
std::expected<String::Char, StarException> String::at(std::size_t index) const {
  Char result{};
  std::size_t position = 0;
  std::size_t offset = 0;
  while (offset != m_string.size()) {
    Char point;
    auto width = utf8_decode_char(m_string.data() + offset, &point, m_string.size() - offset);
    if (!width) return std::unexpected(std::move(width.error()));
    if (position == index) result = point;
    offset += *width;
    ++position;
  }
  if (index >= position) return std::unexpected(OutOfRangeException(Star::format("Out of range in String::at({})", index)));
  return result;
}
CaseInsensitiveString::CaseInsensitiveString() : m_hash(PLHasher().hash()) {}
std::expected<CaseInsensitiveStringView, StarException> CaseInsensitiveStringView::create(String const& string) {
  return create(std::string_view(string.utf8()));
}
std::expected<CaseInsensitiveStringView, StarException> CaseInsensitiveStringView::create(std::string_view bytes) {
  Utf8Cursor cursor(bytes);
  PLHasher hash;
  while (!cursor.at_end()) {
    auto point = cursor.read();
    if (!point) return std::unexpected(std::move(point.error()));
    hash.put(String::to_lower(*point));
    auto moved = cursor.advance();
    if (!moved) return std::unexpected(std::move(moved.error()));
  }
  return CaseInsensitiveStringView(bytes, hash.hash());
}
std::expected<CaseInsensitiveString, StarException> CaseInsensitiveString::create(String string) {
  auto validated = CaseInsensitiveStringView::create(string);
  if (!validated) return std::unexpected(std::move(validated.error()));
  return CaseInsensitiveString(std::move(string), validated->hash());
}
bool CaseInsensitiveStringCompare::operator()(CaseInsensitiveStringView const& lhs, CaseInsensitiveStringView const& rhs) const {
  auto a = lhs.utf8();
  auto b = rhs.utf8();
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i != a.size(); ++i)
    if (String::to_lower(static_cast<unsigned char>(a[i])) != String::to_lower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}
bool operator==(CaseInsensitiveString const& lhs, CaseInsensitiveString const& rhs) {
  return CaseInsensitiveStringCompare()(lhs, rhs);
}
std::expected<String, StarException> String::to_upper() const {
  auto validated = size();
  if (!validated) return std::unexpected(std::move(validated.error()));
  String result = *this;
  for (auto& byte : result.m_string)
    if (byte >= 'a' && byte <= 'z') byte -= 32;
  return result;
}
std::expected<String, StarException> String::to_lower() const {
  auto validated = size();
  if (!validated) return std::unexpected(std::move(validated.error()));
  String result = *this;
  for (auto& byte : result.m_string)
    if (byte >= 'A' && byte <= 'Z') byte += 32;
  return result;
}
std::expected<String, StarException> String::title_case() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  String result;
  result.reserve(m_string.size());
  bool capNext = true;
  for (auto it = *first; it != end(); ++it) {
    auto appended = result.append(capNext ? to_upper(*it) : to_lower(*it));
    if (!appended) return std::unexpected(std::move(appended.error()));
    capNext = *it > std::numeric_limits<unsigned char>::max() || !std::isalpha(static_cast<unsigned char>(*it));
  }
  return result;
}
std::expected<bool, StarException> String::ends_with(String const& suffix, CaseSensitivity cs) const { return StringView(*this).ends_with(suffix, cs); }
std::expected<bool, StarException> String::ends_with(Char point, CaseSensitivity cs) const { return StringView(*this).ends_with(point, cs); }
std::expected<bool, StarException> String::starts_with(String const& prefix, CaseSensitivity cs) const {
  auto prefixLength = prefix.size();
  if (!prefixLength) return std::unexpected(std::move(prefixLength.error()));
  if (*prefixLength == 0) return true;
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (prefix.m_string.size() > m_string.size()) return false;
  if (cs == CaseSensitive) return m_string.starts_with(prefix.m_string);
  return std::equal(prefix.m_string.begin(), prefix.m_string.end(), m_string.begin(), [](unsigned char left, unsigned char right) {
    return to_lower(left) == to_lower(right);
  });
}
std::expected<bool, StarException> String::starts_with(Char point, CaseSensitivity cs) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  return *first != end() && char_equal(**first, point, cs);
}
std::expected<String, StarException> String::reverse() const {
  auto cursor = Utf8Cursor::end(m_string);
  String result;
  result.reserve(m_string.size());
  while (cursor.offset() != 0) {
    auto moved = cursor.retreat(); if (!moved) return std::unexpected(std::move(moved.error()));
    auto point = cursor.read(); if (!point) return std::unexpected(std::move(point.error()));
    auto appended = result.append(*point); if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return result;
}
std::expected<String, StarException> String::rot13() const {
  auto validated = size();
  if (!validated) return std::unexpected(std::move(validated.error()));
  String result = *this;
  for (auto& byte : result.m_string) {
    if ((byte >= 'a' && byte <= 'm') || (byte >= 'A' && byte <= 'M')) byte += 13;
    else if ((byte >= 'n' && byte <= 'z') || (byte >= 'N' && byte <= 'Z')) byte -= 13;
  }
  return result;
}
std::expected<StringList, StarException> String::split(Char point, std::size_t maxSplit) const {
  auto pattern = create(point);
  if (!pattern) return std::unexpected(std::move(pattern.error()));
  return split(*pattern, maxSplit);
}
std::expected<StringList, StarException> String::split(String const& pattern, std::size_t maxSplit) const {
  // This overload has always operated on byte patterns, including raw UTF-8.
  StringList result;
  if (pattern.empty()) return StringList(1, *this);
  std::size_t start = 0;
  for (;;) {
    auto stop = result.size() == maxSplit ? NPos : m_string.find(pattern.m_string, start);
    result.push_back(String(std::string_view(m_string).substr(start, stop == NPos ? NPos : stop - start)));
    if (stop == NPos) return result;
    start = stop + pattern.m_string.size();
  }
}
std::expected<StringList, StarException> String::rsplit(Char point, std::size_t maxSplit) const {
  auto pattern = create(point);
  if (!pattern) return std::unexpected(std::move(pattern.error()));
  return rsplit(*pattern, maxSplit);
}
std::expected<StringList, StarException> String::rsplit(String const& pattern, std::size_t maxSplit) const {
  auto validated = validate_reverse(m_string);
  if (!validated) return std::unexpected(std::move(validated.error()));
  auto patternValidated = validate_reverse(pattern.m_string);
  if (!patternValidated) return std::unexpected(std::move(patternValidated.error()));
  if (pattern.empty()) return StringList(1, *this);
  StringList result;
  std::size_t stop = m_string.size();
  for (;;) {
    auto found = result.size() == maxSplit || stop < pattern.m_string.size()
        ? NPos : m_string.rfind(pattern.m_string, stop - pattern.m_string.size());
    auto start = found == NPos ? 0 : found + pattern.m_string.size();
    result.push_back(String(std::string_view(m_string).substr(start, stop - start)));
    if (found == NPos) break;
    stop = found;
  }
  std::reverse(result.begin(), result.end());
  return result;
}
std::expected<StringList, StarException> String::split_any(String const& chars, std::size_t maxSplit) const {
  Utf8Cursor cursor(m_string);
  StringList result;
  std::size_t start = 0;
  bool done = false;
  while (!cursor.at_end()) {
    auto point = cursor.read(); if (!point) return std::unexpected(std::move(point.error()));
    bool delimiter = false;
    if (!done) {
      auto match = chars.has_char_or_whitespace(*point);
      if (!match) return std::unexpected(std::move(match.error()));
      delimiter = *match;
    }
    if (delimiter) {
      if (start != cursor.offset())
        result.push_back(String(std::string_view(m_string).substr(start, cursor.offset() - start)));
    } else {
      if (result.size() == maxSplit) done = true;
    }
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    if (delimiter) start = cursor.offset();
  }
  if (start != m_string.size())
    result.push_back(String(std::string_view(m_string).substr(start)));
  return result;
}
std::expected<StringList, StarException> String::rsplit_any(String const& chars, std::size_t maxSplit) const {
  auto validated = validate_reverse(m_string);
  if (!validated) return std::unexpected(std::move(validated.error()));
  auto cursor = Utf8Cursor::end(m_string);
  StringList result;
  std::size_t fieldEnd = m_string.size();
  bool done = false;
  while (cursor.offset() != 0) {
    auto previousEnd = cursor.offset();
    auto moved = cursor.retreat();
    if (!moved) return std::unexpected(std::move(moved.error()));
    auto point = cursor.read();
    if (!point) return std::unexpected(std::move(point.error()));
    bool delimiter = false;
    if (!done) {
      auto match = chars.has_char_or_whitespace(*point);
      if (!match) return std::unexpected(std::move(match.error()));
      delimiter = *match;
    }
    if (delimiter) {
      if (fieldEnd != previousEnd)
        result.push_back(String(std::string_view(m_string).substr(previousEnd, fieldEnd - previousEnd)));
      fieldEnd = cursor.offset();
    } else if (result.size() == maxSplit) {
      done = true;
    }
  }
  if (fieldEnd != 0) result.push_back(String(std::string_view(m_string).substr(0, fieldEnd)));
  std::reverse(result.begin(), result.end());
  return result;
}
std::expected<StringList, StarException> String::split_lines(std::size_t maxSplit) const { return split_any("\r\n", maxSplit); }
std::expected<StringList, StarException> String::split_whitespace(std::size_t maxSplit) const { return split_any("", maxSplit); }
std::expected<String, StarException> String::extract(String const& chars) {
  auto parts = split_any(chars, 1);
  if (!parts) return std::unexpected(std::move(parts.error()));
  if (parts->empty()) return String();
  String result = std::move((*parts)[0]);
  if (parts->size() == 1) clear(); else *this = std::move((*parts)[1]);
  return result;
}
std::expected<String, StarException> String::rextract(String const& chars) {
  if (empty()) return String();
  auto parts = rsplit_any(chars, 1);
  if (!parts) return std::unexpected(std::move(parts.error()));
  if (parts->empty()) return std::unexpected(OutOfRangeException("No token in String::rextract"));
  if (parts->size() == 1) { clear(); return std::move((*parts)[0]); }
  *this = std::move((*parts)[0]);
  return std::move((*parts)[1]);
}
std::expected<bool, StarException> String::has_char(Char point) const { return StringView(*this).has_char(point); }
std::expected<bool, StarException> String::has_char_or_whitespace(Char point) const { return StringView(*this).has_char_or_whitespace(point); }
std::expected<String, StarException> String::replace(String const& pattern, String const& value, CaseSensitivity cs) const {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  auto patternLength = pattern.size();
  if (!patternLength) return std::unexpected(std::move(patternLength.error()));
  if (pattern.empty()) return *this;
  String result;
  result.reserve(m_string.size());
  std::size_t copied = 0;
  if (cs == CaseSensitive) {
    for (;;) {
      auto found = m_string.find(pattern.m_string, copied);
      if (found == NPos) break;
      result.append(std::string_view(m_string).substr(copied, found - copied));
      result.append(value);
      copied = found + pattern.m_string.size();
    }
  } else {
    Utf8Cursor cursor(m_string);
    while (!cursor.at_end()) {
      auto candidate = cursor;
      Utf8Cursor needle(pattern.m_string);
      while (!candidate.at_end() && !needle.at_end()) {
        auto point = candidate.read();
        if (!point) return std::unexpected(std::move(point.error()));
        auto expected = needle.read();
        if (!expected) return std::unexpected(std::move(expected.error()));
        if (!char_equal(*point, *expected, cs)) break;
        auto moved = candidate.advance();
        if (!moved) return std::unexpected(std::move(moved.error()));
        auto needleMoved = needle.advance();
        if (!needleMoved) return std::unexpected(std::move(needleMoved.error()));
      }
      if (needle.at_end()) {
        result.append(std::string_view(m_string).substr(copied, cursor.offset() - copied));
        result.append(value);
        cursor = candidate;
        copied = cursor.offset();
      } else {
        auto moved = cursor.advance();
        if (!moved) return std::unexpected(std::move(moved.error()));
      }
    }
  }
  result.append(std::string_view(m_string).substr(copied));
  return result;
}
std::expected<String, StarException> String::trim_end(String const& pattern) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  auto stop = end();
  while (stop != *first) {
    auto previous = stop; --previous;
    auto match = pattern.has_char_or_whitespace(*previous);
    if (!match) return std::unexpected(std::move(match.error()));
    if (!*match) break;
    stop = previous;
  }
  return String(std::string_view(m_string).substr(0, static_cast<std::size_t>(std::distance(m_string.begin(), stop.base()))));
}
std::expected<String, StarException> String::trim_begin(String const& pattern) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  auto start = *first;
  while (start != end()) {
    auto match = pattern.has_char_or_whitespace(*start);
    if (!match) return std::unexpected(std::move(match.error()));
    if (!*match) break;
    ++start;
  }
  auto offset = static_cast<std::size_t>(std::distance(m_string.begin(), start.base()));
  return String(std::string_view(m_string).substr(offset));
}
std::expected<String, StarException> String::trim(String const& pattern) const {
  auto result = trim_end(pattern);
  if (!result) return std::unexpected(std::move(result.error()));
  return result->trim_begin(pattern);
}
std::expected<std::size_t, StarException> String::find(Char point, std::size_t pos, CaseSensitivity cs) const { return StringView(*this).find(point, pos, cs); }
std::expected<std::size_t, StarException> String::find(String const& pattern, std::size_t pos, CaseSensitivity cs) const { return StringView(*this).find(pattern, pos, cs); }
std::expected<std::size_t, StarException> String::find_last(Char point, CaseSensitivity cs) const { return StringView(*this).find_last(point, cs); }
std::expected<std::size_t, StarException> String::find_last(String const& pattern, CaseSensitivity cs) const { return StringView(*this).find_last(pattern, cs); }
std::expected<std::size_t, StarException> String::find_first_of(String const& chars, std::size_t pos) const { return StringView(*this).find_first_of(chars, pos); }
std::expected<std::size_t, StarException> String::find_first_not_of(String const& chars, std::size_t pos) const { return StringView(*this).find_first_not_of(chars, pos); }
std::expected<std::size_t, StarException> String::find_next_boundary(std::size_t index, bool backwards) const { return StringView(*this).find_next_boundary(index, backwards); }
std::expected<String, StarException> String::slice(SliceIndex a, SliceIndex b, int step) const {
  auto wide = wide_string();
  if (!wide) return std::unexpected(std::move(wide.error()));
  auto sliced = Star::slice(*wide, a, b, step);
  return create(std::u32string_view(sliced));
}
void String::append(String const& s) { m_string.append(s.m_string); }
void String::append(std::string const& s) { m_string.append(s); }
std::expected<void, StarException> String::append(std::u32string_view s) {
  for (auto point : s) { auto result = append(point); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}
void String::append(std::string_view s) { m_string.append(s); }
std::expected<void, StarException> String::append(Char point) {
  char bytes[4];
  auto count = utf8_encode_char(bytes, point, sizeof(bytes));
  if (!count) return std::unexpected(std::move(count.error()));
  m_string.append(bytes, *count);
  return {};
}
void String::prepend(String const& s) { m_string.insert(0, s.m_string); }
void String::prepend(std::string const& s) { m_string.insert(0, s); }
void String::prepend(std::string_view s) { m_string.insert(0, s); }
std::expected<void, StarException> String::prepend(std::u32string_view s) {
  auto prefix = create(s);
  if (!prefix) return std::unexpected(std::move(prefix.error()));
  prepend(*prefix); return {};
}
std::expected<void, StarException> String::prepend(Char point) {
  char bytes[4];
  auto count = utf8_encode_char(bytes, point, sizeof(bytes));
  if (!count) return std::unexpected(std::move(count.error()));
  m_string.insert(0, bytes, *count);
  return {};
}
std::expected<void, StarException> String::push_back(Char point) { return append(point); }
std::expected<void, StarException> String::push_front(Char point) { return prepend(point); }
std::expected<bool, StarException> String::contains(String const& pattern, CaseSensitivity cs) const { return StringView(*this).contains(pattern, cs); }
std::expected<bool, StarException> String::regex_match(String const& pattern, bool full, bool caseSensitive) const {
  re2::RE2::Options options;
  options.set_case_sensitive(caseSensitive);
  RE2 regex(pattern.utf8(), options);
  if (!regex.ok()) return std::unexpected(StringException::format("Invalid regex pattern '{}': {}", pattern, regex.error()));
  return full ? RE2::FullMatch(utf8(), regex) : RE2::PartialMatch(utf8(), regex);
}
std::expected<int, StarException> String::compare(String const& other, CaseSensitivity cs) const {
  if (cs == CaseSensitive) return m_string.compare(other.m_string);
  return StringView(*this).compare(other, cs);
}
std::expected<bool, StarException> String::equals(String const& other, CaseSensitivity cs) const {
  auto result = compare(other, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result == 0;
}
std::expected<bool, StarException> String::equals_ignore_case(String const& other) const { return equals(other, CaseInsensitive); }
std::expected<String, StarException> String::substr(std::size_t position, std::size_t count) const {
  auto bounds = string_byte_bounds(m_string, position, count);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  if (position > bounds->length) return std::unexpected(OutOfRangeException(Star::format("out of range in String::substr({}, {})", position, count)));
  return String(std::string_view(m_string).substr(bounds->start, bounds->stop - bounds->start));
}
std::expected<void, StarException> String::erase(std::size_t pos, std::size_t count) {
  auto bounds = string_byte_bounds(m_string, pos, count);
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  if (pos > bounds->length) return std::unexpected(OutOfRangeException("Out of range in String::erase"));
  m_string.erase(bounds->start, bounds->stop - bounds->start);
  return {};
}
std::expected<String, StarException> String::pad_left(std::size_t target, String const& filler) const {
  auto fillLength = filler.length();
  if (!fillLength) return std::unexpected(std::move(fillLength.error()));
  if (*fillLength == 0) return *this;
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  String result;
  std::size_t added = 0;
  while (added < target && *length < target - added) { result.append(filler); added += *fillLength; }
  result.append(*this);
  return result;
}
std::expected<String, StarException> String::pad_right(std::size_t target, String const& filler) const {
  auto fillLength = filler.length();
  if (!fillLength) return std::unexpected(std::move(fillLength.error()));
  if (*fillLength == 0) return *this;
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  String result = *this;
  for (auto n = *length; n < target; n += *fillLength) result.append(filler);
  return result;
}
String& String::operator=(String const& s) = default;
String& String::operator=(String&& s) noexcept = default;
String& String::operator+=(String const& s) { append(s); return *this; }
String& String::operator+=(std::string const& s) { append(s); return *this; }
std::expected<std::reference_wrapper<String>, StarException> String::operator+=(std::u32string_view s) { auto result = append(s); if (!result) return std::unexpected(std::move(result.error())); return std::ref(*this); }
std::expected<std::reference_wrapper<String>, StarException> String::operator+=(Char point) { auto result = append(point); if (!result) return std::unexpected(std::move(result.error())); return std::ref(*this); }
bool operator==(String const& a, String const& b) { return a.m_string == b.m_string; }
bool operator==(String const& a, std::string const& b) { return a.m_string == b; }
bool operator==(std::string const& a, String const& b) { return a == b.m_string; }
std::expected<bool, StarException> operator==(String const& a, std::u32string_view b) { auto text = String::create(b); if (!text) return std::unexpected(std::move(text.error())); return a == *text; }
std::expected<bool, StarException> operator==(std::u32string_view a, String const& b) { return b == a; }
std::strong_ordering operator<=>(String const& a, String const& b) { return a.m_string <=> b.m_string; }
std::strong_ordering operator<=>(String const& a, std::string const& b) { return a.m_string <=> b; }
std::strong_ordering operator<=>(String const& a, std::string_view b) { return std::string_view(a.m_string) <=> b; }
std::strong_ordering operator<=>(std::string const& a, String const& b) { return a <=> b.m_string; }
std::strong_ordering operator<=>(std::string_view a, String const& b) { return a <=> std::string_view(b.m_string); }
std::expected<std::strong_ordering, StarException> operator<=>(String const& a, std::u32string_view b) { auto text = String::create(b); if (!text) return std::unexpected(std::move(text.error())); return a <=> *text; }
std::expected<std::strong_ordering, StarException> operator<=>(std::u32string_view a, String const& b) { auto text = String::create(a); if (!text) return std::unexpected(std::move(text.error())); return *text <=> b; }
String operator+(String a, String const& b) { a.append(b); return a; }
String operator+(String a, std::string const& b) { a.append(b); return a; }
String operator+(String a, std::string_view b) { a.append(b); return a; }
String operator+(std::string const& a, String const& b) { return String(a + b.m_string); }
String operator+(std::string_view a, String const& b) { String result(a); result.append(b); return result; }
std::expected<String, StarException> operator+(String a, std::u32string_view b) { auto result = a.append(b); if (!result) return std::unexpected(std::move(result.error())); return a; }
std::expected<String, StarException> operator+(std::u32string_view a, String const& b) { auto text = String::create(a); if (!text) return std::unexpected(std::move(text.error())); text->append(b); return std::move(*text); }
std::expected<String, StarException> operator+(String a, String::Char point) { auto result = a.append(point); if (!result) return std::unexpected(std::move(result.error())); return a; }
std::expected<String, StarException> operator+(String::Char point, String const& b) { auto text = String::create(point); if (!text) return std::unexpected(std::move(text.error())); text->append(b); return std::move(*text); }
String operator*(String const& text, unsigned times) { String result; for (unsigned i = 0; i != times; ++i) result.append(text); return result; }
String operator*(unsigned times, String const& text) { return text * times; }
void write_text(TextBuffer& output, String const& text) { output.append(text.utf8()); }
StringList::StringList() = default;
StringList::StringList(Base const& list) : Base(list) {}
StringList::StringList(Base&& list) noexcept : Base(std::move(list)) {}
StringList::StringList(StringList const& list) = default;
StringList::StringList(StringList&& list) noexcept : Base(std::move(list)) {}
std::expected<StringList, StarException> StringList::create(std::span<std::u32string_view const> strings) {
  StringList result;
  result.reserve(strings.size());
  for (auto source : strings) {
    auto text = String::create(source);
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}
StringList::StringList(std::span<std::string_view const> strings) {
  reserve(strings.size());
  for (auto source : strings)
    push_back(String(source));
}
StringList::StringList(std::size_t count, String const& text) : Base(count, text) {}
StringList::StringList(std::initializer_list<String> list) : Base(list) {}
StringList& StringList::operator=(Base const& list) { Base::operator=(list); return *this; }
StringList& StringList::operator=(Base&& list) noexcept { Base::operator=(std::move(list)); return *this; }
StringList& StringList::operator=(StringList const& list) = default;
StringList& StringList::operator=(StringList&& list) noexcept { Base::operator=(std::move(list)); return *this; }
StringList& StringList::operator=(std::initializer_list<String> list) { Base::operator=(list); return *this; }
std::expected<bool, StarException> StringList::contains(String const& text, String::CaseSensitivity cs) const {
  for (auto const& item : *this) { auto result = text.compare(item, cs); if (!result) return std::unexpected(std::move(result.error())); if (*result == 0) return true; }
  return false;
}
std::expected<StringList, StarException> StringList::trim_all(String const& pattern) const {
  StringList result;
  result.reserve(size());
  for (auto const& item : *this) {
    auto text = item.trim(pattern);
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}
String StringList::join(String const& separator) const {
  std::size_t bytes = 0;
  for (auto it = begin(); it != end(); ++it) {
    if (it != begin()) bytes = byte_size_sum(bytes, separator.utf8_size());
    bytes = byte_size_sum(bytes, it->utf8_size());
  }
  String result;
  result.reserve(bytes);
  for (auto it = begin(); it != end(); ++it) { if (it != begin()) result.append(separator); result.append(*it); }
  return result;
}
std::expected<StringList, StarException> StringList::slice(SliceIndex a, SliceIndex b, int step) const { return Star::slice(*this, a, b, step); }
StringList StringList::sorted() const { StringList result = *this; result.sort(); return result; }
void write_text(TextBuffer& output, StringList const& list) {
  output.push_back('(');
  for (auto it = list.begin(); it != list.end(); ++it) {
    if (it != list.begin())
      output.append(", ");
    output.push_back('\'');
    output.append(it->utf8());
    output.push_back('\'');
  }
  output.push_back(')');
}
std::size_t hash<StringList>::operator()(StringList const& list) const {
  std::size_t hash = 0;
  for (auto const& text : list)
    hash_combine(hash, hash_of(text));
  return hash;
}
}
