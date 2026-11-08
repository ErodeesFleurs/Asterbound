import std;
import star.memory;
import star.string_view;
import star.string;
import star.unicode;
import star.format;
import star.hash;
import star.byte_array;

namespace Star {
namespace {
std::expected<void, StarException> cursor_skip(Utf8Cursor& cursor, std::size_t count) {
  while (count && !cursor.at_end()) {
    auto moved = cursor.advance();
    if (!moved) return std::unexpected(std::move(moved.error()));
    --count;
  }
  return {};
}

std::expected<bool, StarException> cursor_matches(Utf8Cursor source, std::string_view needle, String::CaseSensitivity cs) {
  Utf8Cursor pattern(needle);
  while (!source.at_end() && !pattern.at_end()) {
    auto expected = pattern.read(); if (!expected) return std::unexpected(std::move(expected.error()));
    auto actual = source.read(); if (!actual) return std::unexpected(std::move(actual.error()));
    if (!String::char_equal(*actual, *expected, cs)) return false;
    auto source_moved = source.advance(); if (!source_moved) return std::unexpected(std::move(source_moved.error()));
    auto pattern_moved = pattern.advance(); if (!pattern_moved) return std::unexpected(std::move(pattern_moved.error()));
  }
  return pattern.at_end();
}

std::expected<std::size_t, StarException> cursor_find_membership(std::string_view bytes, StringView pattern, std::size_t position, bool member) {
  Utf8Cursor cursor(bytes);
  auto skipped = cursor_skip(cursor, position);
  if (!skipped) return std::unexpected(std::move(skipped.error()));
  while (!cursor.at_end()) {
    auto point = cursor.read(); if (!point) return std::unexpected(std::move(point.error()));
    auto match = pattern.has_char_or_whitespace(*point); if (!match) return std::unexpected(std::move(match.error()));
    if (*match == member) return position;
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    ++position;
  }
  return NPos;
}
}

String::String(StringView s) : m_string(s.utf8()) {}
String::String(std::string_view s) : m_string(s) {}
String& String::operator+=(StringView s) { m_string += s.utf8(); return *this; }
String& String::operator+=(std::string_view s) { m_string += s; return *this; }
StringView::StringView() = default;
StringView::StringView(StringView const& s) = default;
StringView::StringView(StringView&& s) noexcept = default;
StringView::StringView(String const& s) : m_view(s.utf8()) {}
StringView::StringView(std::string_view const& s) : m_view(s) {}
StringView::StringView(std::string_view&& s) noexcept : m_view(s) {}
StringView::StringView(std::string const& s) : m_view(s) {}
std::size_t hash<String>::operator()(StringView const& s) const { return (*this)(s.utf8()); }


std::string_view StringView::utf8() const { return m_view; }
std::string_view StringView::take_utf8() { return std::exchange(m_view, {}); }
ByteArray StringView::utf8_bytes() const { return ByteArray(std::span<char const>(m_view)); }
char const* StringView::utf8_data() const { return m_view.data(); }
std::size_t StringView::utf8_size() const { return m_view.size(); }
std::expected<StringView::const_iterator, StarException> StringView::begin() const { return const_iterator::create(m_view.begin(), m_view.end()); }
StringView::const_iterator StringView::end() const { return const_iterator::end(m_view.end()); }
std::expected<std::size_t, StarException> StringView::size() const { return utf8_length(m_view.data(), m_view.size()); }
std::expected<std::size_t, StarException> StringView::length() const { return size(); }
bool StringView::empty() const { return m_view.empty(); }
std::expected<StringView::Char, StarException> StringView::operator[](std::size_t index) const { return at(index); }
std::expected<StringView::Char, StarException> StringView::at(std::size_t index) const {
  Utf8Cursor cursor(m_view);
  for (std::size_t i = 0; i != index; ++i) {
    if (cursor.at_end()) return std::unexpected(OutOfRangeException("Out of range in StringView::at"));
    auto moved = cursor.advance();
    if (!moved) return std::unexpected(std::move(moved.error()));
  }
  if (cursor.at_end()) return std::unexpected(OutOfRangeException("Out of range in StringView::at"));
  return cursor.read();
}
std::expected<int, StarException> StringView::compare(std::size_t selfOffset, std::size_t selfLen, StringView other, std::size_t otherOffset, std::size_t otherLen, CaseSensitivity cs) const {
  Utf8Cursor a(m_view), b(other.m_view);
  auto self_skipped = cursor_skip(a, selfOffset);
  if (!self_skipped) return std::unexpected(std::move(self_skipped.error()));
  auto other_skipped = cursor_skip(b, otherOffset);
  if (!other_skipped) return std::unexpected(std::move(other_skipped.error()));
  for (;;) {
    bool ae = a.at_end() || selfLen == 0, be = b.at_end() || otherLen == 0;
    if (ae || be) return ae ? (be ? 0 : -1) : 1;
    auto ac = a.read(); if (!ac) return std::unexpected(std::move(ac.error()));
    auto bc = b.read(); if (!bc) return std::unexpected(std::move(bc.error()));
    auto av = cs == CaseSensitivity::CaseInsensitive ? String::to_lower(*ac) : *ac;
    auto bv = cs == CaseSensitivity::CaseInsensitive ? String::to_lower(*bc) : *bc;
    if (av != bv) return av < bv ? -1 : 1;
    auto am = a.advance(); if (!am) return std::unexpected(std::move(am.error()));
    auto bm = b.advance(); if (!bm) return std::unexpected(std::move(bm.error()));
    --selfLen; --otherLen;
  }
}
std::expected<int, StarException> StringView::compare(StringView other, CaseSensitivity cs) const {
  if (cs == CaseSensitivity::CaseSensitive) return m_view.compare(other.m_view);
  return compare(0, NPos, other, 0, NPos, cs);
}
std::expected<bool, StarException> StringView::equals(StringView other, CaseSensitivity cs) const {
  auto result = compare(other, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result == 0;
}
std::expected<bool, StarException> StringView::equals_ignore_case(StringView other) const { return equals(other, CaseSensitivity::CaseInsensitive); }
std::expected<bool, StarException> StringView::starts_with(StringView prefix, CaseSensitivity cs) const {
  auto length = prefix.size();
  if (!length) return std::unexpected(std::move(length.error()));
  auto result = compare(0, *length, prefix, 0, NPos, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result == 0;
}
std::expected<bool, StarException> StringView::ends_with(StringView suffix, CaseSensitivity cs) const {
  auto suffixLength = suffix.size();
  if (!suffixLength) return std::unexpected(std::move(suffixLength.error()));
  if (*suffixLength == 0) return true;
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (suffix.m_view.size() > m_view.size()) return false;
  if (cs == CaseSensitivity::CaseSensitive) return m_view.ends_with(suffix.m_view);
  auto tail = m_view.substr(m_view.size() - suffix.m_view.size());
  return std::ranges::equal(tail, suffix.m_view, [](unsigned char left, unsigned char right) {
    return String::char_equal(left, right, CaseSensitivity::CaseInsensitive);
  });
}
std::expected<bool, StarException> StringView::starts_with(Char point, CaseSensitivity cs) const {
  if (empty()) return false;
  Utf8Cursor cursor(m_view);
  auto first = cursor.read();
  if (!first) return std::unexpected(std::move(first.error()));
  return String::char_equal(*first, point, cs);
}
std::expected<bool, StarException> StringView::ends_with(Char point, CaseSensitivity cs) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  if (*first == end()) return false;
  auto last = end(); --last;
  return String::char_equal(*last, point, cs);
}
std::expected<std::size_t, StarException> StringView::find(Char point, std::size_t pos, CaseSensitivity cs) const {
  Utf8Cursor cursor(m_view);
  auto skipped = cursor_skip(cursor, pos);
  if (!skipped) return std::unexpected(std::move(skipped.error()));
  while (!cursor.at_end()) {
    auto current = cursor.read(); if (!current) return std::unexpected(std::move(current.error()));
    if (String::char_equal(*current, point, cs)) return pos;
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    ++pos;
  }
  return NPos;
}
std::expected<std::size_t, StarException> StringView::find(StringView needle, std::size_t pos, CaseSensitivity cs) const {
  if (needle.empty()) return 0;
  Utf8Cursor cursor(m_view);
  auto skipped = cursor_skip(cursor, pos);
  if (!skipped) return std::unexpected(std::move(skipped.error()));
  while (!cursor.at_end()) {
    auto match = cursor_matches(cursor, needle.m_view, cs);
    if (!match) return std::unexpected(std::move(match.error()));
    if (*match) return pos;
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    ++pos;
  }
  return NPos;
}
std::expected<std::size_t, StarException> StringView::find_last(Char point, CaseSensitivity cs) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  std::size_t found = NPos, pos = 0;
  for (auto it = *first; it != end(); ++it, ++pos) if (String::char_equal(*it, point, cs)) found = pos;
  return found;
}
std::expected<std::size_t, StarException> StringView::find_last(StringView needle, CaseSensitivity cs) const {
  if (needle.empty()) return 0;
  Utf8Cursor cursor(m_view);
  std::size_t found = NPos, pos = 0;
  while (!cursor.at_end()) {
    auto match = cursor_matches(cursor, needle.m_view, cs);
    if (!match) return std::unexpected(std::move(match.error()));
    if (*match) found = pos;
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    ++pos;
  }
  return found;
}
std::expected<bool, StarException> StringView::has_char(Char point) const {
  auto result = find(point, 0, CaseSensitivity::CaseSensitive);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result != NPos;
}
std::expected<bool, StarException> StringView::has_char_or_whitespace(Char point) const {
  if (empty()) return String::is_space(point);
  return has_char(point);
}
std::expected<std::size_t, StarException> StringView::find_first_of(StringView pattern, std::size_t pos) const {
  return cursor_find_membership(m_view, pattern, pos, true);
}
std::expected<std::size_t, StarException> StringView::find_first_not_of(StringView pattern, std::size_t pos) const {
  return cursor_find_membership(m_view, pattern, pos, false);
}
std::expected<std::size_t, StarException> StringView::find_next_boundary(std::size_t index, bool backwards) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  auto it = *first;
  std::size_t pos = 0;
  while (pos != index && it != end()) { ++it; ++pos; }
  if (pos != index) return std::unexpected(OutOfRangeException("Out of range in StringView::find_next_boundary"));
  if ((!backwards && it == end()) || (backwards && index == 0)) return index;
  if (backwards) { --it; --index; }
  while (!String::is_space(*it)) {
    if (backwards) { if (index == 0) return 0; --it; --index; }
    else { ++it; ++index; if (it == end()) return index; }
  }
  while (String::is_space(*it)) {
    if (backwards) { if (index == 0) return 0; --it; --index; }
    else { ++it; ++index; if (it == end()) return index; }
  }
  return backwards ? index + 1 : index;
}
std::expected<bool, StarException> StringView::contains(StringView needle, CaseSensitivity cs) const {
  auto result = find(needle, 0, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result != NPos;
}
std::expected<StringView, StarException> StringView::substr(std::size_t position, std::size_t count) const {
  Utf8Cursor cursor(m_view);
  for (std::size_t i = 0; i != position; ++i) {
    if (cursor.at_end()) return std::unexpected(OutOfRangeException("Out of range in StringView::substr"));
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
  }
  auto start = cursor.offset();
  for (std::size_t i = 0; i != count && !cursor.at_end(); ++i) {
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
  }
  return StringView(m_view.substr(start, cursor.offset() - start));
}
StringView& StringView::operator=(StringView s) { m_view = s.m_view; return *this; }
bool operator==(StringView a, std::string_view b) { return a.m_view == b; }
bool operator==(StringView a, std::string const& b) { return a.m_view.compare(std::string_view(b)) == 0; }
bool operator==(StringView a, String const& b) { return a.m_view.compare(std::string_view(b.utf8())) == 0; }
bool operator==(StringView a, StringView b) { return a.m_view == b.m_view; }
bool operator!=(StringView a, StringView b) { return a.m_view != b.m_view; }
bool operator<(StringView a, StringView b) { return a.m_view < b.m_view; }
std::ostream& operator<<(std::ostream& os, StringView const& s) { return os << s.utf8(); }
}
