import star.memory;
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
bool String::isSpace(Char c) { return c == 0x20 || c == 9 || c == 10 || c == 13 || c == 0xfeff; }
bool String::isAsciiNumber(Char c) { return c >= '0' && c <= '9'; }
bool String::isAsciiLetter(Char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
String::Char String::toLower(Char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
String::Char String::toUpper(Char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
bool String::charEqual(Char a, Char b, CaseSensitivity cs) { return cs == CaseInsensitive ? toLower(a) == toLower(b) : a == b; }
std::expected<String, StarException> String::joinWith(String const& join, String const& left, String const& right) {
  if (left.empty()) return right;
  if (right.empty()) return left;
  auto leftJoined = left.endsWith(join);
  if (!leftJoined) return std::unexpected(std::move(leftJoined.error()));
  auto rightJoined = right.beginsWith(join);
  if (!rightJoined) return std::unexpected(std::move(rightJoined.error()));
  if (*leftJoined && *rightJoined) {
    auto length = join.size();
    if (!length) return std::unexpected(std::move(length.error()));
    auto tail = right.substr(*length);
    if (!tail) return std::unexpected(std::move(tail.error()));
    return left + *tail;
  }
  return *leftJoined || *rightJoined ? left + right : left + join + right;
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
  auto length = utf8EncodeChar(bytes, point, sizeof(bytes));
  if (!length) return std::unexpected(std::move(length.error()));
  String result;
  result.reserve(count);
  for (std::size_t i = 0; i != count; ++i) result.append(std::string_view(bytes, *length));
  return result;
}
std::expected<String, StarException> String::create(Char point) { return create(point, 1); }
std::string const& String::utf8() const { return m_string; }
std::string String::takeUtf8() { return std::exchange(m_string, {}); }
ByteArray String::utf8Bytes() const { return ByteArray(std::span<char const>(m_string)); }
char const* String::utf8Ptr() const { return m_string.c_str(); }
std::size_t String::utf8Size() const { return m_string.size(); }
std::expected<std::wstring, StarException> String::wstring() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  std::wstring result;
  for (auto it = *first; it != end(); ++it) result.push_back(static_cast<wchar_t>(*it));
  return result;
}
std::expected<String::WideString, StarException> String::wideString() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  WideString result;
  result.reserve(m_string.size());
  for (auto it = *first; it != end(); ++it) result.push_back(*it);
  return result;
}
std::expected<String::const_iterator, StarException> String::begin() const { return const_iterator::create(m_string.begin(), m_string.end()); }
String::const_iterator String::end() const { return const_iterator::end(m_string.end()); }
std::expected<std::size_t, StarException> String::size() const { return utf8Length(m_string.data(), m_string.size()); }
std::expected<std::size_t, StarException> String::length() const { return size(); }
void String::clear() { m_string.clear(); }
void String::reserve(std::size_t n) { m_string.reserve(n); }
bool String::empty() const { return m_string.empty(); }
std::expected<String::Char, StarException> String::operator[](std::size_t index) const { return StringView(*this).at(index); }
std::expected<String::Char, StarException> String::at(std::size_t index) const {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (index >= *length) return std::unexpected(OutOfRangeException(strf("Out of range in String::at({})", index)));
  return StringView(*this).at(index);
}
CaseInsensitiveString::CaseInsensitiveString() : m_hash(PLHasher().hash()) {}
std::expected<CaseInsensitiveStringView, StarException> CaseInsensitiveStringView::create(String const& string) {
  return create(std::string_view(string.utf8()));
}
std::expected<CaseInsensitiveStringView, StarException> CaseInsensitiveStringView::create(std::string_view bytes) {
  Utf8Cursor cursor(bytes);
  PLHasher hash;
  while (!cursor.atEnd()) {
    auto point = cursor.read();
    if (!point) return std::unexpected(std::move(point.error()));
    hash.put(String::toLower(*point));
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
    if (String::toLower(static_cast<unsigned char>(a[i])) != String::toLower(static_cast<unsigned char>(b[i]))) return false;
  return true;
}
bool operator==(CaseInsensitiveString const& lhs, CaseInsensitiveString const& rhs) {
  return CaseInsensitiveStringCompare()(lhs, rhs);
}
std::expected<String, StarException> String::toUpper() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  String result;
  result.reserve(m_string.size());
  for (auto it = *first; it != end(); ++it) {
    auto appended = result.append(toUpper(*it));
    if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return result;
}
std::expected<String, StarException> String::toLower() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  String result;
  result.reserve(m_string.size());
  for (auto it = *first; it != end(); ++it) {
    auto appended = result.append(toLower(*it));
    if (!appended) return std::unexpected(std::move(appended.error()));
  }
  return result;
}
std::expected<String, StarException> String::titleCase() const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  String result;
  result.reserve(m_string.size());
  bool capNext = true;
  for (auto it = *first; it != end(); ++it) {
    auto appended = result.append(capNext ? toUpper(*it) : toLower(*it));
    if (!appended) return std::unexpected(std::move(appended.error()));
    capNext = *it > std::numeric_limits<unsigned char>::max() || !std::isalpha(static_cast<unsigned char>(*it));
  }
  return result;
}
std::expected<bool, StarException> String::endsWith(String const& suffix, CaseSensitivity cs) const { return StringView(*this).endsWith(suffix, cs); }
std::expected<bool, StarException> String::endsWith(Char point, CaseSensitivity cs) const { return StringView(*this).endsWith(point, cs); }
std::expected<bool, StarException> String::beginsWith(String const& prefix, CaseSensitivity cs) const {
  auto prefixLength = prefix.size();
  if (!prefixLength) return std::unexpected(std::move(prefixLength.error()));
  if (*prefixLength == 0) return true;
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (*prefixLength > *length) return false;
  auto comparison = compare(0, *prefixLength, prefix, 0, NPos, cs);
  if (!comparison) return std::unexpected(std::move(comparison.error()));
  return *comparison == 0;
}
std::expected<bool, StarException> String::beginsWith(Char point, CaseSensitivity cs) const {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (*length == 0) return false;
  return StringView(*this).beginsWith(point, cs);
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
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  String result;
  result.reserve(m_string.size());
  for (auto it = *first; it != end(); ++it) {
    auto point = *it;
    if ((point >= 'a' && point <= 'm') || (point >= 'A' && point <= 'M')) point += 13;
    else if ((point >= 'n' && point <= 'z') || (point >= 'N' && point <= 'Z')) point -= 13;
    auto appended = result.append(point);
    if (!appended) return std::unexpected(std::move(appended.error()));
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
  return rsplitAny(*pattern, maxSplit);
}
std::expected<StringList, StarException> String::rsplit(String const& pattern, std::size_t maxSplit) const {
  auto reversed = reverse();
  if (!reversed) return std::unexpected(std::move(reversed.error()));
  auto reversedPattern = pattern.reverse();
  if (!reversedPattern) return std::unexpected(std::move(reversedPattern.error()));
  auto parts = reversed->split(*reversedPattern, maxSplit);
  if (!parts) return std::unexpected(std::move(parts.error()));
  for (auto& part : *parts) {
    auto value = part.reverse();
    if (!value) return std::unexpected(std::move(value.error()));
    part = std::move(*value);
  }
  std::reverse(parts->begin(), parts->end());
  return std::move(*parts);
}
std::expected<StringList, StarException> String::splitAny(String const& chars, std::size_t maxSplit) const {
  Utf8Cursor cursor(m_string);
  StringList result;
  String next;
  bool done = false;
  while (!cursor.atEnd()) {
    auto point = cursor.read(); if (!point) return std::unexpected(std::move(point.error()));
    bool delimiter = false;
    if (!done) {
      auto match = chars.hasCharOrWhitespace(*point);
      if (!match) return std::unexpected(std::move(match.error()));
      delimiter = *match;
    }
    if (delimiter) {
      if (!next.empty())
        result.push_back(std::exchange(next, {}));
    } else {
      if (result.size() == maxSplit) done = true;
      auto appended = next.append(*point);
      if (!appended) return std::unexpected(std::move(appended.error()));
    }
    auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
  }
  if (!next.empty())
    result.push_back(std::move(next));
  return result;
}
std::expected<StringList, StarException> String::rsplitAny(String const& chars, std::size_t maxSplit) const {
  auto reversed = reverse();
  if (!reversed) return std::unexpected(std::move(reversed.error()));
  auto parts = reversed->splitAny(chars, maxSplit);
  if (!parts) return std::unexpected(std::move(parts.error()));
  for (auto& part : *parts) {
    auto value = part.reverse();
    if (!value) return std::unexpected(std::move(value.error()));
    part = std::move(*value);
  }
  std::reverse(parts->begin(), parts->end());
  return std::move(*parts);
}
std::expected<StringList, StarException> String::splitLines(std::size_t maxSplit) const { return splitAny("\r\n", maxSplit); }
std::expected<StringList, StarException> String::splitWhitespace(std::size_t maxSplit) const { return splitAny("", maxSplit); }
std::expected<String, StarException> String::extract(String const& chars) {
  auto parts = splitAny(chars, 1);
  if (!parts) return std::unexpected(std::move(parts.error()));
  if (parts->empty()) return String();
  String result = std::move((*parts)[0]);
  if (parts->size() == 1) clear(); else *this = std::move((*parts)[1]);
  return result;
}
std::expected<String, StarException> String::rextract(String const& chars) {
  if (empty()) return String();
  auto parts = rsplitAny(chars, 1);
  if (!parts) return std::unexpected(std::move(parts.error()));
  if (parts->empty()) return std::unexpected(OutOfRangeException("No token in String::rextract"));
  if (parts->size() == 1) { clear(); return std::move((*parts)[0]); }
  *this = std::move((*parts)[0]);
  return std::move((*parts)[1]);
}
std::expected<bool, StarException> String::hasChar(Char point) const { return StringView(*this).hasChar(point); }
std::expected<bool, StarException> String::hasCharOrWhitespace(Char point) const { return StringView(*this).hasCharOrWhitespace(point); }
std::expected<String, StarException> String::replace(String const& pattern, String const& value, CaseSensitivity cs) const {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  auto patternLength = pattern.size();
  if (!patternLength) return std::unexpected(std::move(patternLength.error()));
  if (pattern.empty()) return *this;
  String result;
  result.reserve(m_string.size());
  Utf8Cursor cursor(m_string);
  std::size_t position = 0;
  for (;;) {
    auto found = find(pattern, position, cs);
    if (!found) return std::unexpected(std::move(found.error()));
    auto start = cursor.offset();
    auto stop = *found == NPos ? *length : *found;
    while (position < stop) {
      auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
      ++position;
    }
    result.append(std::string_view(m_string).substr(start, cursor.offset() - start));
    if (*found == NPos) return result;
    result.append(value);
    for (std::size_t i = 0; i != *patternLength; ++i) {
      auto moved = cursor.advance(); if (!moved) return std::unexpected(std::move(moved.error()));
      ++position;
    }
  }
}
std::expected<String, StarException> String::trimEnd(String const& pattern) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  auto stop = end();
  while (stop != *first) {
    auto previous = stop; --previous;
    auto match = pattern.hasCharOrWhitespace(*previous);
    if (!match) return std::unexpected(std::move(match.error()));
    if (!*match) break;
    stop = previous;
  }
  return String(std::string_view(m_string).substr(0, static_cast<std::size_t>(std::distance(m_string.begin(), stop.base()))));
}
std::expected<String, StarException> String::trimBeg(String const& pattern) const {
  auto first = begin();
  if (!first) return std::unexpected(std::move(first.error()));
  auto start = *first;
  while (start != end()) {
    auto match = pattern.hasCharOrWhitespace(*start);
    if (!match) return std::unexpected(std::move(match.error()));
    if (!*match) break;
    ++start;
  }
  auto offset = static_cast<std::size_t>(std::distance(m_string.begin(), start.base()));
  return String(std::string_view(m_string).substr(offset));
}
std::expected<String, StarException> String::trim(String const& pattern) const {
  auto result = trimEnd(pattern);
  if (!result) return std::unexpected(std::move(result.error()));
  return result->trimBeg(pattern);
}
std::expected<std::size_t, StarException> String::find(Char point, std::size_t pos, CaseSensitivity cs) const { return StringView(*this).find(point, pos, cs); }
std::expected<std::size_t, StarException> String::find(String const& pattern, std::size_t pos, CaseSensitivity cs) const { return StringView(*this).find(pattern, pos, cs); }
std::expected<std::size_t, StarException> String::findLast(Char point, CaseSensitivity cs) const { return StringView(*this).findLast(point, cs); }
std::expected<std::size_t, StarException> String::findLast(String const& pattern, CaseSensitivity cs) const { return StringView(*this).findLast(pattern, cs); }
std::expected<std::size_t, StarException> String::findFirstOf(String const& chars, std::size_t pos) const { return StringView(*this).findFirstOf(chars, pos); }
std::expected<std::size_t, StarException> String::findFirstNotOf(String const& chars, std::size_t pos) const { return StringView(*this).findFirstNotOf(chars, pos); }
std::expected<std::size_t, StarException> String::findNextBoundary(std::size_t index, bool backwards) const { return StringView(*this).findNextBoundary(index, backwards); }
std::expected<String, StarException> String::slice(SliceIndex a, SliceIndex b, int step) const {
  auto wide = wideString();
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
  auto count = utf8EncodeChar(bytes, point, sizeof(bytes));
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
  auto count = utf8EncodeChar(bytes, point, sizeof(bytes));
  if (!count) return std::unexpected(std::move(count.error()));
  m_string.insert(0, bytes, *count);
  return {};
}
std::expected<void, StarException> String::push_back(Char point) { return append(point); }
std::expected<void, StarException> String::push_front(Char point) { return prepend(point); }
std::expected<bool, StarException> String::contains(String const& pattern, CaseSensitivity cs) const { return StringView(*this).contains(pattern, cs); }
std::expected<bool, StarException> String::regexMatch(String const& pattern, bool full, bool caseSensitive) const {
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
std::expected<int, StarException> String::compare(std::size_t offset, std::size_t count, String const& other, std::size_t otherOffset, std::size_t otherCount, CaseSensitivity cs) const {
  return StringView(*this).compare(offset, count, other, otherOffset, otherCount, cs);
}
std::expected<bool, StarException> String::equals(String const& other, CaseSensitivity cs) const {
  auto result = compare(other, cs);
  if (!result) return std::unexpected(std::move(result.error()));
  return *result == 0;
}
std::expected<bool, StarException> String::equalsIgnoreCase(String const& other) const { return equals(other, CaseInsensitive); }
std::expected<String, StarException> String::substr(std::size_t position, std::size_t count) const {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (position > *length) return std::unexpected(OutOfRangeException(strf("out of range in String::substr({}, {})", position, count)));
  auto view = StringView(*this).substr(position, count);
  if (!view) return std::unexpected(std::move(view.error()));
  return String(*view);
}
std::expected<void, StarException> String::erase(std::size_t pos, std::size_t count) {
  auto length = size();
  if (!length) return std::unexpected(std::move(length.error()));
  if (pos > *length) return std::unexpected(OutOfRangeException("Out of range in String::erase"));
  auto view = StringView(*this).substr(pos, count);
  if (!view) return std::unexpected(std::move(view.error()));
  auto offset = static_cast<std::size_t>(view->utf8().data() - m_string.data());
  m_string.erase(offset, view->utf8Size());
  return {};
}
std::expected<String, StarException> String::padLeft(std::size_t target, String const& filler) const {
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
std::expected<String, StarException> String::padRight(std::size_t target, String const& filler) const {
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
std::ostream& operator<<(std::ostream& os, String const& text) { return os << text.utf8(); }
std::istream& operator>>(std::istream& is, String& text) { std::string bytes; is >> bytes; text = String(std::move(bytes)); return is; }
StringList::StringList() = default;
StringList::StringList(Base const& list) : Base(list) {}
StringList::StringList(Base&& list) : Base(std::move(list)) {}
StringList::StringList(StringList const& list) = default;
StringList::StringList(StringList&& list) : Base(std::move(list)) {}
std::expected<StringList, StarException> StringList::create(std::span<std::u32string_view const> strings) {
  StringList result;
  for (auto source : strings) {
    auto text = String::create(source);
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}
StringList::StringList(std::span<std::string_view const> strings) {
  for (auto source : strings)
    push_back(String(source));
}
StringList::StringList(std::size_t count, String const& text) : Base(count, text) {}
StringList::StringList(std::initializer_list<String> list) : Base(list) {}
StringList& StringList::operator=(Base const& list) { Base::operator=(list); return *this; }
StringList& StringList::operator=(Base&& list) { Base::operator=(std::move(list)); return *this; }
StringList& StringList::operator=(StringList const& list) = default;
StringList& StringList::operator=(StringList&& list) { Base::operator=(std::move(list)); return *this; }
StringList& StringList::operator=(std::initializer_list<String> list) { Base::operator=(list); return *this; }
std::expected<bool, StarException> StringList::contains(String const& text, String::CaseSensitivity cs) const {
  for (auto const& item : *this) { auto result = text.compare(item, cs); if (!result) return std::unexpected(std::move(result.error())); if (*result == 0) return true; }
  return false;
}
std::expected<StringList, StarException> StringList::trimAll(String const& pattern) const {
  StringList result;
  for (auto const& item : *this) {
    auto text = item.trim(pattern);
    if (!text)
      return std::unexpected(std::move(text.error()));
    result.push_back(std::move(*text));
  }
  return result;
}
String StringList::join(String const& separator) const {
  String result;
  for (auto it = begin(); it != end(); ++it) { if (it != begin()) result.append(separator); result.append(*it); }
  return result;
}
std::expected<StringList, StarException> StringList::slice(SliceIndex a, SliceIndex b, int step) const { return Star::slice(*this, a, b, step); }
StringList StringList::sorted() const { StringList result = *this; result.sort(); return result; }
std::ostream& operator<<(std::ostream& os, StringList const& list) {
  os << "(";
  for (auto it = list.begin(); it != list.end(); ++it) { if (it != list.begin()) os << ", "; os << '\'' << *it << '\''; }
  return os << ")";
}
std::size_t hash<StringList>::operator()(StringList const& list) const {
  std::size_t hash = 0;
  for (auto const& text : list)
    hash_combine(hash, hash_of(text));
  return hash;
}
}
