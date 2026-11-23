import std;
import star.text_formatter;
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

std::expected<bool, StarException> matches_prefix(std::string_view bytes, std::string_view needle, String::CaseSensitivity cs) {
  auto common = std::min(bytes.size(), needle.size());
  std::size_t i = 0;
  for (; i != common; ++i) {
    auto a = static_cast<unsigned char>(bytes[i]);
    auto b = static_cast<unsigned char>(needle[i]);
    if ((a | b) >= 0x80) break;
    if (a != b && (cs == String::CaseSensitive || !String::char_equal(a, b, cs))) return false;
  }
  if (i == common) return i == needle.size();
  // At a non-ASCII boundary validate the pattern point before the source point.
  Utf8Cursor source(bytes.substr(i)), pattern(needle.substr(i));
  while (!source.at_end() && !pattern.at_end()) {
    auto expected = pattern.read(); if (!expected) return std::unexpected(std::move(expected.error()));
    auto actual = source.read(); if (!actual) return std::unexpected(std::move(actual.error()));
    if (!String::char_equal(*actual, *expected, cs)) return false;
    auto source_moved = source.advance(); if (!source_moved) return std::unexpected(std::move(source_moved.error()));
    auto pattern_moved = pattern.advance(); if (!pattern_moved) return std::unexpected(std::move(pattern_moved.error()));
  }
  return pattern.at_end();
}

// Consume one point from a nonempty view; ASCII needs no decoder call.
std::expected<Utf32Type, StarException> take_point(std::string_view& bytes) {
  Utf32Type point = static_cast<unsigned char>(bytes.front());
  if (point < 0x80) {
    bytes.remove_prefix(1);
  } else {
    auto width = utf8_decode_char(bytes, point);
    if (!width) return std::unexpected(std::move(width.error()));
    bytes.remove_prefix(*width);
  }
  return point;
}

std::expected<std::size_t, StarException> find_substring(std::string_view bytes, std::string_view needle,
    std::size_t position, String::CaseSensitivity cs, bool last) {
  if (needle.empty()) return 0;
  if (position != 0) {
    Utf8Cursor cursor(bytes);
    auto skipped = cursor_skip(cursor, position);
    if (!skipped) return std::unexpected(std::move(skipped.error()));
    bytes.remove_prefix(cursor.offset());
  }
  if (bytes.empty()) return npos;
  auto first = take_point(needle);
  if (!first) return std::unexpected(std::move(first.error()));
  auto lower = cs == String::CaseInsensitive ? String::to_lower(*first) : *first;
  auto upper = cs == String::CaseInsensitive ? String::to_upper(*first) : *first;
  std::size_t found = npos;
  while (!bytes.empty()) {
    // An ASCII first-point miss consumes one byte and one codepoint. Stop at
    // the first candidate or high byte; an early hit need not scan the suffix.
    std::size_t run = 0;
    for (; run != bytes.size(); ++run) {
      auto byte = static_cast<unsigned char>(bytes[run]);
      if (byte >= 0x80 || byte == lower || byte == upper) break;
    }
    position += run;
    bytes.remove_prefix(run);
    if (bytes.empty()) break;
    auto point = take_point(bytes);
    if (!point) return std::unexpected(std::move(point.error()));
    if (*point == lower || *point == upper) {
      auto match = matches_prefix(bytes, needle, cs);
      if (!match) return std::unexpected(std::move(match.error()));
      if (*match) {
        if (!last) return position;
        found = position;
      }
    }
    ++position;
  }
  return found;
}

std::expected<std::size_t, StarException> cursor_find_membership(std::string_view bytes, StringView pattern, std::size_t position, bool member) {
  Utf8Cursor cursor(bytes);
  auto skipped = cursor_skip(cursor, position);
  if (!skipped) return std::unexpected(std::move(skipped.error()));
  auto set = pattern.utf8();
  bool singleton = set.size() == 1 && static_cast<unsigned char>(set.front()) < 0x80;
  while (!cursor.at_end()) {
    auto point = cursor.read(); if (!point) return std::unexpected(std::move(point.error()));
    bool match;
    if (set.empty()) {
      match = String::is_space(*point);
    } else if (singleton) {
      match = *point == static_cast<unsigned char>(set.front());
    } else {
      auto result = pattern.contains(*point);
      if (!result) return std::unexpected(std::move(result.error()));
      match = *result;
    }
    if (match == member) return position;
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
    ++position;
  }
  return npos;
}
}

String::String(StringView s) : m_string(s.utf8()) {}
String::String(std::string_view s) : m_string(s) {}
String& String::operator+=(StringView s) { m_string += s.utf8(); return *this; }
String& String::operator+=(std::string_view s) { m_string += s; return *this; }
StringView::StringView(String const& s) : m_view(s.utf8()) {}
std::size_t ValueHash<String>::operator()(StringView const& s) const { return (*this)(s.utf8()); }

ByteArray StringView::utf8_bytes() const { return ByteArray(std::span<char const>(m_view)); }
std::expected<StringView::const_iterator, StarException> StringView::begin() const { return const_iterator::create(m_view.begin(), m_view.end()); }
StringView::const_iterator StringView::end() const { return const_iterator::end(m_view.end()); }
std::expected<std::size_t, StarException> StringView::size() const { return utf8_length(m_view); }
std::expected<std::size_t, StarException> StringView::length() const { return size(); }
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
std::expected<int, StarException> StringView::compare(StringView other, CaseSensitivity cs) const {
  if (cs == CaseSensitivity::CaseSensitive) return m_view.compare(other.m_view);
  Utf8Cursor a(m_view), b(other.m_view);
  while (!a.at_end() && !b.at_end()) {
    auto ac = a.read(); if (!ac) return std::unexpected(std::move(ac.error()));
    auto bc = b.read(); if (!bc) return std::unexpected(std::move(bc.error()));
    auto av = String::to_lower(*ac);
    auto bv = String::to_lower(*bc);
    if (av != bv) return av < bv ? -1 : 1;
    auto am = a.advance(); if (!am) return std::unexpected(std::move(am.error()));
    auto bm = b.advance(); if (!bm) return std::unexpected(std::move(bm.error()));
  }
  return a.at_end() ? (b.at_end() ? 0 : -1) : 1;
}
std::expected<bool, StarException> StringView::equals(StringView other, CaseSensitivity cs) const {
  auto result = compare(other, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result == 0;
}
std::expected<bool, StarException> StringView::equals_ignore_case(StringView other) const { return equals(other, CaseSensitivity::CaseInsensitive); }
std::expected<bool, StarException> StringView::starts_with(StringView prefix, CaseSensitivity cs) const {
  auto validated = prefix.size();
  if (!validated) return std::unexpected(std::move(validated.error()));
  return matches_prefix(m_view, prefix.m_view, cs);
}
std::expected<bool, StarException> StringView::ends_with(StringView suffix, CaseSensitivity cs) const {
  auto suffix_length = suffix.size();
  if (!suffix_length) return std::unexpected(std::move(suffix_length.error()));
  if (*suffix_length == 0) return true;
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
  return npos;
}
std::expected<std::size_t, StarException> StringView::find(StringView needle, std::size_t pos, CaseSensitivity cs) const {
  return find_substring(m_view, needle.m_view, pos, cs, false);
}
std::expected<std::size_t, StarException> StringView::rfind(Char point, CaseSensitivity cs) const {
  std::size_t found = npos, position = 0;
  auto scan = for_each_codepoint(m_view, [&](Utf32Type current, std::size_t, std::size_t) {
    if (String::char_equal(current, point, cs)) found = position;
    ++position;
  });
  if (!scan) return std::unexpected(std::move(scan.error()));
  return found;
}
std::expected<std::size_t, StarException> StringView::rfind(StringView needle, CaseSensitivity cs) const {
  return find_substring(m_view, needle.m_view, 0, cs, true);
}
std::expected<bool, StarException> StringView::contains(Char point) const {
  auto result = find(point, 0, CaseSensitivity::CaseSensitive);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result != npos;
}
std::expected<bool, StarException> StringView::has_char_or_whitespace(Char point) const {
  if (empty()) return String::is_space(point);
  return contains(point);
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
  return *result != npos;
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
bool operator==(StringView a, std::string_view b) { return a.m_view == b; }
bool operator==(StringView a, std::string const& b) { return a.m_view.compare(std::string_view(b)) == 0; }
bool operator==(StringView a, String const& b) { return a.m_view.compare(std::string_view(b.utf8())) == 0; }
bool operator==(StringView a, StringView b) { return a.m_view == b.m_view; }
bool operator!=(StringView a, StringView b) { return a.m_view != b.m_view; }
bool operator<(StringView a, StringView b) { return a.m_view < b.m_view; }
void write_text(TextBuffer& output, StringView const& text) { output.append(text.utf8()); }
}
