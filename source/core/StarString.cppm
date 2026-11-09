module;
import star.memory;
import star.hash;

import star.format;
import star.text_formatter;
import std;

import star.byte_array;
import star.list;
import star.map;
import star.set;
import star.unicode;

import star.pythonic;
namespace Star {

class StringList;
class StringView;
class String;

template <typename Lookup>
concept string_tag_lookup = std::invocable<Lookup&, String const&>
  && (std::same_as<std::remove_cvref_t<std::invoke_result_t<Lookup&, String const&>>, String>
      || std::same_as<std::remove_cvref_t<std::invoke_result_t<Lookup&, String const&>>,
                      std::expected<String, StarException>>);

template <typename Lookup>
concept string_view_tag_lookup = std::invocable<Lookup&, std::string_view>
  && (std::same_as<std::remove_cvref_t<std::invoke_result_t<Lookup&, std::string_view>>,
                   std::expected<String, StarException>>
      || requires(std::invoke_result_t<Lookup&, std::string_view>& replacement, std::string& output) {
           output.append(replacement.utf8());
         });

template <typename Map>
concept string_tag_map = requires(Map const& map, String const& key) {
  static_cast<bool>(map.find(key) == map.end());
  { map.find(key)->second } -> std::convertible_to<String>;
};

struct StringExceptionTag {
  static constexpr std::string_view name() { return "StringException"; }
};
using StringException = StarError<StringExceptionTag, StarException>;

// A Unicode string class, which is a basic UTF-8 aware wrapper around
// std::string.  Provides methods for accessing UTF-32 "Char" type, which
// provides access to each individual code point.  Printing, hashing, copying,
// and in-order access should be basically as fast as std::string, but the more
// complex string processing methods may be much worse.
//
// All case sensitive / insensitive functionality is based on ASCII tolower and
// toupper, and will have no effect on characters outside ASCII.  Therefore,
// case insensitivity is really only appropriate for code / script processing,
// not for general strings.
class String {
public:
  using Char = Utf32Type;

  // std::basic_string equivalent that guarantees const access time for
  // operator[], etc
  using WideString = std::basic_string<Char>;

  using const_iterator = U8ToU32Iterator<std::string::const_iterator>;
  using value_type = Char;
  using const_reference = value_type;

  enum CaseSensitivity {
    CaseSensitive,
    CaseInsensitive
  };

  // Space, horizontal tab, newline, carriage return, and BOM / ZWNBSP
  static bool is_space(Char c);
  static bool is_ascii_number(Char c);
  static bool is_ascii_letter(Char c);

  // These methods only actually work on unicode characters below 127, i.e.
  // ASCII subset.
  static Char to_lower(Char c);
  static Char to_upper(Char c);
  static bool char_equal(Char c1, Char c2, CaseSensitivity cs);

  // Join two strings together with a joiner, so that only one instance of the
  // joiner is in between the left and right strings.  For example, joins "foo"
  // and "bar" with "?" to produce "foo?bar".  Gets rid of repeat joiners, so
  // "foo?" and "?bar" with "?" also becomes "foo?bar".  Also, if left or right
  // is empty, does not add a joiner, for example "" and "baz" joined with "?"
  // produces "baz".
  [[nodiscard]] static std::expected<String, StarException> join_with(String const& join, String const& left, String const& right);
  template <typename... StringType>
    requires (std::convertible_to<StringType const&, String> && ...)
  [[nodiscard]] static std::expected<String, StarException> join_with(String const& join, String const& first, String const& second, String const& third, StringType const&... rest);

  String();
  String(String const& s);
  String(String&& s) noexcept;

  // These assume utf8 input
  template <std::size_t N>
  String(char const (&s)[N]) : m_string(s) {}
  String(std::string const& s);
  String(std::string&& s) noexcept;

  [[nodiscard]] static std::expected<String, StarException> create(std::wstring const& s);
  [[nodiscard]] static std::expected<String, StarException> create(std::u32string_view s);
  [[nodiscard]] static std::expected<String, StarException> create(Char c, std::size_t n);
  [[nodiscard]] static std::expected<String, StarException> create(Char c);

  // Lvalues borrow; temporary owners transfer or copy their bytes.
  std::string const& utf8() const &;
  std::string utf8() && noexcept;
  std::string utf8() const &&;
  std::string take_utf8();
  ByteArray utf8_bytes() const;
  // Pointer to internal utf8 data, null-terminated.
  char const* utf8_data() const &;
  char const* utf8_data() const && = delete;
  std::size_t utf8_size() const;

  [[nodiscard]] std::expected<std::wstring, StarException> wstring() const;
  [[nodiscard]] std::expected<WideString, StarException> wide_string() const;

  [[nodiscard]] std::expected<const_iterator, StarException> begin() const &;
  [[nodiscard]] std::expected<const_iterator, StarException> begin() const && = delete;
  const_iterator end() const &;
  const_iterator end() const && = delete;

  [[nodiscard]] std::expected<std::size_t, StarException> size() const;
  [[nodiscard]] std::expected<std::size_t, StarException> length() const;

  void clear();
  void reserve(std::size_t n);
  bool empty() const;

  [[nodiscard]] std::expected<Char, StarException> operator[](std::size_t i) const;
  [[nodiscard]] std::expected<Char, StarException> at(std::size_t i) const;

  [[nodiscard]] std::expected<String, StarException> to_upper() const;
  [[nodiscard]] std::expected<String, StarException> to_lower() const;
  [[nodiscard]] std::expected<String, StarException> title_case() const;

  [[nodiscard]] std::expected<bool, StarException> ends_with(String const& end, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> ends_with(Char end, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> starts_with(String const& beg, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> starts_with(Char beg, CaseSensitivity cs = CaseSensitive) const;

  [[nodiscard]] std::expected<String, StarException> reverse() const;
  [[nodiscard]] std::expected<String, StarException> rot13() const;

  [[nodiscard]] std::expected<StringList, StarException> split(Char c, std::size_t maxSplit = NPos) const;
  [[nodiscard]] std::expected<StringList, StarException> split(String const& pattern, std::size_t maxSplit = NPos) const;
  [[nodiscard]] std::expected<StringList, StarException> rsplit(Char c, std::size_t maxSplit = NPos) const;
  [[nodiscard]] std::expected<StringList, StarException> rsplit(String const& pattern, std::size_t maxSplit = NPos) const;

  // Splits on any number of contiguous instances of any of the given
  // characters.  Behaves differently than regular split in that leading and
  // trailing instances of the characters are also ignored, and in general no
  // empty strings will be in the resulting split list.  If chars is empty,
  // then splits on any whitespace.
  [[nodiscard]] std::expected<StringList, StarException> split_any(String const& chars = "", std::size_t maxSplit = NPos) const;
  [[nodiscard]] std::expected<StringList, StarException> rsplit_any(String const& chars = "", std::size_t maxSplit = NPos) const;

  [[nodiscard]] std::expected<StringList, StarException> split_lines(std::size_t maxSplit = NPos) const;
  [[nodiscard]] std::expected<StringList, StarException> split_whitespace(std::size_t maxSplit = NPos) const;

  // Splits a string once based on the given characters (defaulting to
  // whitespace), and returns the first part.  This string is set to the
  // second part.
  [[nodiscard]] std::expected<String, StarException> extract(String const& chars = "");
  [[nodiscard]] std::expected<String, StarException> rextract(String const& chars = "");

  [[nodiscard]] std::expected<bool, StarException> has_char(Char c) const;
  // Identical to has_char, except, if string is empty, tests if c is
  // whitespace.
  [[nodiscard]] std::expected<bool, StarException> has_char_or_whitespace(Char c) const;

  [[nodiscard]] std::expected<String, StarException> replace(String const& rplc, String const& val, CaseSensitivity cs = CaseSensitive) const;

  [[nodiscard]] std::expected<String, StarException> trim_end(String const& chars = "") const;
  [[nodiscard]] std::expected<String, StarException> trim_begin(String const& chars = "") const;
  [[nodiscard]] std::expected<String, StarException> trim(String const& chars = "") const;

  [[nodiscard]] std::expected<std::size_t, StarException> find(Char c, std::size_t beg = 0, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find(String const& s, std::size_t beg = 0, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find_last(Char c, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find_last(String const& s, CaseSensitivity cs = CaseSensitive) const;

  // If pattern is empty, finds first whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> find_first_of(String const& chars = "", std::size_t beg = 0) const;

  // If pattern is empty, finds first non-whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> find_first_not_of(String const& chars = "", std::size_t beg = 0) const;

  // finds the the start of the next 'boundary' in a string.  used for quickly
  // scanning a string
  [[nodiscard]] std::expected<std::size_t, StarException> find_next_boundary(std::size_t index, bool backwards = false) const;

  [[nodiscard]] std::expected<String, StarException> slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  void append(String const& s);
  void append(std::string const& s);
  [[nodiscard]] std::expected<void, StarException> append(std::u32string_view s);
  void append(std::string_view s);
  template <std::size_t N>
  void append(char const (&s)[N]) { append(std::string_view(s)); }
  [[nodiscard]] std::expected<void, StarException> append(Char c);

  void prepend(String const& s);
  void prepend(std::string const& s);
  [[nodiscard]] std::expected<void, StarException> prepend(std::u32string_view s);
  void prepend(std::string_view s);
  template <std::size_t N>
  void prepend(char const (&s)[N]) { prepend(std::string_view(s)); }
  [[nodiscard]] std::expected<void, StarException> prepend(Char c);

  [[nodiscard]] std::expected<void, StarException> push_back(Char c);
  [[nodiscard]] std::expected<void, StarException> push_front(Char c);

  [[nodiscard]] std::expected<bool, StarException> contains(String const& s, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> regex_match(String const& regex, bool full = true, bool caseSensitive = true) const;

  [[nodiscard]] std::expected<int, StarException> compare(String const& s, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equals(String const& s, CaseSensitivity cs = CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equals_ignore_case(String const& s) const;

  [[nodiscard]] std::expected<String, StarException> substr(std::size_t position, std::size_t n = NPos) const;
  [[nodiscard]] std::expected<void, StarException> erase(std::size_t pos = 0, std::size_t n = NPos);

  [[nodiscard]] std::expected<String, StarException> pad_left(std::size_t size, String const& filler) const;
  [[nodiscard]] std::expected<String, StarException> pad_right(std::size_t size, String const& filler) const;

  // Replace angle bracket tags in the string with values given by the given
  // lookup function.  Will be called as:
  // String lookup(String const& key), or expected<String, StarException>.
  template <string_tag_lookup Lookup>
  [[nodiscard]] auto lookup_tags(Lookup&& lookup) const;

  // StringView variant
  template <string_view_tag_lookup Lookup>
  [[nodiscard]] auto maybe_lookup_tags_view(Lookup&& lookup) const;

  template <string_view_tag_lookup Lookup>
  [[nodiscard]] auto lookup_tags_view(Lookup&& lookup) const;

  // Replace angle bracket tags in the string with values given by the tags
  // map.  If replaceWithDefault is true, then values that are not found in the
  // tags map are replace with the default string.  If replaceWithDefault is
  // false, tags that are not found are not replaced at all.
  template <string_tag_map MapType>
  String replace_tags(MapType const& tags, bool replaceWithDefault = false, String defaultValue = "") const;

  String& operator=(String const& s);
  String& operator=(String&& s) noexcept;

  String& operator+=(String const& s);
  String& operator+=(std::string const& s);
  [[nodiscard]] std::expected<std::reference_wrapper<String>, StarException> operator+=(std::u32string_view s);
  template <std::size_t N>
  String& operator+=(char const (&s)[N]) { append(std::string_view(s)); return *this; }
  [[nodiscard]] std::expected<std::reference_wrapper<String>, StarException> operator+=(Char c);

  friend bool operator==(String const& s1, String const& s2);
  friend bool operator==(String const& s1, std::string const& s2);
  friend bool operator==(String const& s1, std::string_view s2) { return s1.m_string == s2; }
  friend std::expected<bool, StarException> operator==(String const& s1, std::u32string_view s2);
  friend bool operator==(std::string const& s1, String const& s2);
  friend std::expected<bool, StarException> operator==(std::u32string_view s1, String const& s2);

  // C++20 rewrites !=, >, <= and >= from these, including the mixed operand
  // forms: any of the seven rewrites produces an ambiguity only if a candidate
  // with a worse conversion is needed, and every form has a <=> here.
  friend std::strong_ordering operator<=>(String const& s1, String const& s2);
  friend std::strong_ordering operator<=>(String const& s1, std::string const& s2);
  friend std::expected<std::strong_ordering, StarException> operator<=>(String const& s1, std::u32string_view s2);
  friend std::strong_ordering operator<=>(String const& s1, std::string_view s2);
  friend std::strong_ordering operator<=>(std::string const& s1, String const& s2);
  friend std::expected<std::strong_ordering, StarException> operator<=>(std::u32string_view s1, String const& s2);
  friend std::strong_ordering operator<=>(std::string_view s1, String const& s2);

  friend String operator+(String s1, String const& s2);
  friend String operator+(String s1, std::string const& s2);
  friend std::expected<String, StarException> operator+(String s1, std::u32string_view s2);
  friend String operator+(String s1, std::string_view s2);
  friend String operator+(std::string const& s1, String const& s2);
  friend std::expected<String, StarException> operator+(std::u32string_view s1, String const& s2);
  friend String operator+(std::string_view s1, String const& s2);

  friend std::expected<String, StarException> operator+(String s, Char c);
  friend std::expected<String, StarException> operator+(Char c, String const& s);

  friend String operator*(String const& s, unsigned times);
  friend String operator*(unsigned times, String const& s);

  friend void write_text(TextBuffer& output, String const& text);

  // String view functions
  String(StringView s);
  String(std::string_view s);

  String& operator+=(StringView s);
  String& operator+=(std::string_view s);

  template <std::size_t N>
  friend bool operator==(String const& s, char const (&text)[N]) { return s.m_string == std::string_view(text); }
  template <std::size_t N>
  friend std::strong_ordering operator<=>(String const& s, char const (&text)[N]) { return s.m_string <=> std::string_view(text); }
  template <std::size_t N>
  friend std::strong_ordering operator<=>(char const (&text)[N], String const& s) { return std::string_view(text) <=> s.m_string; }
  template <std::size_t N>
  friend String operator+(String s, char const (&text)[N]) { s.append(std::string_view(text)); return s; }
  template <std::size_t N>
  friend String operator+(char const (&text)[N], String const& s) { String result(text); result.append(s); return result; }

private:
  [[nodiscard]] static std::expected<void, StarException> append_joined(
      String& accumulator, String const& join, String const& right);

  std::string m_string;
};

[[nodiscard]] std::expected<bool, StarException> operator==(String const& s1, std::u32string_view s2);
[[nodiscard]] std::expected<bool, StarException> operator==(std::u32string_view s1, String const& s2);
[[nodiscard]] std::expected<std::strong_ordering, StarException> operator<=>(String const& s1, std::u32string_view s2);
[[nodiscard]] std::expected<std::strong_ordering, StarException> operator<=>(std::u32string_view s1, String const& s2);
[[nodiscard]] std::expected<String, StarException> operator+(String s1, std::u32string_view s2);
[[nodiscard]] std::expected<String, StarException> operator+(std::u32string_view s1, String const& s2);
[[nodiscard]] std::expected<String, StarException> operator+(String s, String::Char c);
[[nodiscard]] std::expected<String, StarException> operator+(String::Char c, String const& s);


class StringList : public List<String> {
  // Retain a concrete owning string sequence for text-specific operations and codecs.
public:
  using Base = List<String>;

  using iterator = Base::iterator;
  using const_iterator = Base::const_iterator;
  using value_type = Base::value_type;
  using reference = Base::reference;
  using const_reference = Base::const_reference;

  template <typename Range>
    requires sequence_source<Range const&, String>
  static StringList from(Range const& range);

  StringList();
  StringList(Base const& l);
  StringList(Base&& l) noexcept;
  StringList(StringList const& l);
  StringList(StringList&& l) noexcept;
  [[nodiscard]] static std::expected<StringList, StarException> create(std::span<std::u32string_view const> list);
  StringList(std::span<std::string_view const> list);
  explicit StringList(std::size_t len, String const& s1 = String());
  StringList(std::initializer_list<String> list);

  template <std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel = Iterator>
    requires std::constructible_from<String, std::iter_reference_t<Iterator>>
  StringList(Iterator first, Sentinel last) : Base(std::move(first), std::move(last)) {}

  template <typename Range>
    requires sequence_source<Range, String>
  explicit StringList(std::from_range_t, Range&& range)
    : Base(std::from_range, std::forward<Range>(range)) {}

  StringList& operator=(Base const& rhs);
  StringList& operator=(Base&& rhs) noexcept;
  StringList& operator=(StringList const& rhs);
  StringList& operator=(StringList&& rhs) noexcept;
  StringList& operator=(std::initializer_list<String> list);

  [[nodiscard]] std::expected<bool, StarException> contains(String const& s, String::CaseSensitivity cs = String::CaseSensitive) const;
  [[nodiscard]] std::expected<StringList, StarException> trim_all(String const& chars = "") const;
  String join(String const& separator = "") const;

  [[nodiscard]] std::expected<StringList, StarException> slice(SliceIndex a = SliceIndex(), SliceIndex b = SliceIndex(), int i = 1) const;

  template <typename Comparator>
    requires requires(List<String>& list, Comparator&& compare) { list.sort(std::forward<Comparator>(compare)); }
  StringList sorted(Comparator&& comparator) const;

  StringList sorted() const;
};

void write_text(TextBuffer& output, StringList const& list);

template <>
struct hash<String> {
  using is_transparent = void;
  std::size_t operator()(String const& s) const;
  std::size_t operator()(StringView const& s) const;
  constexpr std::size_t operator()(std::string_view s) const noexcept {
    return pl_hash_bytes(std::span(s.data(), s.size()));
  }
  template <std::size_t N> constexpr std::size_t operator()(char const (&s)[N]) const noexcept {
    return (*this)(std::string_view(s));
  }
  constexpr std::size_t operator()(std::string const& s) const noexcept { return (*this)(std::string_view(s)); }
};

class CaseInsensitiveString;

// Borrowed validated key. Source bytes must outlive this view and must not be
// mutated while it is used.
class CaseInsensitiveStringView {
public:
  [[nodiscard]] static std::expected<CaseInsensitiveStringView, StarException> create(String const& string);
  [[nodiscard]] static std::expected<CaseInsensitiveStringView, StarException> create(String&&) = delete;
  [[nodiscard]] static std::expected<CaseInsensitiveStringView, StarException> create(String const&&) = delete;
  [[nodiscard]] static std::expected<CaseInsensitiveStringView, StarException> create(std::string_view bytes);
  template <typename Owner>
    requires std::same_as<std::remove_cvref_t<Owner>, std::string> && (!std::is_lvalue_reference_v<Owner>)
  [[nodiscard]] static std::expected<CaseInsensitiveStringView, StarException> create(Owner&&) = delete;
  std::string_view utf8() const { return m_bytes; }
  std::size_t hash() const { return m_hash; }
  friend void write_text(TextBuffer& output, CaseInsensitiveStringView const& key) { output.append(key.utf8()); }
private:
  friend class CaseInsensitiveString;
  CaseInsensitiveStringView(std::string_view bytes, std::size_t hash) : m_bytes(bytes), m_hash(hash) {}
  std::string_view m_bytes;
  std::size_t m_hash;
};

// Standard hash/equality callbacks cannot carry input errors. This owning key
// validates at creation and caches the original codepoint hash.
class CaseInsensitiveString {
public:
  CaseInsensitiveString();
  CaseInsensitiveString(CaseInsensitiveString const&) = default;
  CaseInsensitiveString& operator=(CaseInsensitiveString const&) = default;
  CaseInsensitiveString(CaseInsensitiveString&& other) noexcept
    : m_string(std::move(other.m_string)), m_hash(std::exchange(other.m_hash, PLHasher().hash())) { other.m_string.clear(); }
  CaseInsensitiveString& operator=(CaseInsensitiveString&& other) noexcept {
    if (this != &other) {
      m_string = std::move(other.m_string);
      m_hash = std::exchange(other.m_hash, PLHasher().hash());
      other.m_string.clear();
    }
    return *this;
  }
  [[nodiscard]] static std::expected<CaseInsensitiveString, StarException> create(String string);
  String const& string() const & { return m_string; }
  String const& string() const && = delete;
  std::size_t hash() const { return m_hash; }
  CaseInsensitiveStringView view() const & { return CaseInsensitiveStringView(m_string.utf8(), m_hash); }
  CaseInsensitiveStringView view() const && = delete;
  friend void write_text(TextBuffer& output, CaseInsensitiveString const& key) { output.append(key.string().utf8()); }
  friend bool operator==(CaseInsensitiveString const& lhs, CaseInsensitiveString const& rhs);
private:
  CaseInsensitiveString(String&& string, std::size_t hash) : m_string(std::move(string)), m_hash(hash) {}
  String m_string;
  std::size_t m_hash;
};

struct CaseInsensitiveStringHash {
  using is_transparent = void;
  std::size_t operator()(CaseInsensitiveString const& s) const { return s.hash(); }
  std::size_t operator()(CaseInsensitiveStringView const& s) const { return s.hash(); }
};

struct CaseInsensitiveStringCompare {
  using is_transparent = void;
  bool operator()(CaseInsensitiveStringView const& lhs, CaseInsensitiveStringView const& rhs) const;
  bool operator()(CaseInsensitiveString const& lhs, CaseInsensitiveString const& rhs) const { return (*this)(lhs.view(), rhs.view()); }
  bool operator()(CaseInsensitiveString const& lhs, CaseInsensitiveStringView const& rhs) const { return (*this)(lhs.view(), rhs); }
  bool operator()(CaseInsensitiveStringView const& lhs, CaseInsensitiveString const& rhs) const { return (*this)(lhs, rhs.view()); }
};

using StringSet = HashSet<String, hash<String>, std::equal_to<>>;

using CaseInsensitiveStringSet =
  HashSet<CaseInsensitiveString, CaseInsensitiveStringHash, CaseInsensitiveStringCompare>;

template <typename MappedT, typename HashT = hash<String>, typename ComparatorT = std::equal_to<>>
using StringMap = HashMap<String, MappedT, HashT, ComparatorT>;

template <typename MappedT, typename HashT = hash<String>, typename ComparatorT = std::equal_to<>>
using StableStringMap = StableHashMap<String, MappedT, HashT, ComparatorT>;

template <typename MappedT>
using CaseInsensitiveStringMap = HashMap<CaseInsensitiveString, MappedT, CaseInsensitiveStringHash, CaseInsensitiveStringCompare>;

template <>
struct hash<StringList> {
  std::size_t operator()(StringList const& s) const;
};

template <typename... StringType>
  requires (std::convertible_to<StringType const&, String> && ...)
[[nodiscard]] std::expected<String, StarException> String::join_with(
    String const& join, String const& first, String const& second, String const& third, StringType const&... rest) {
  String result(first);
  std::expected<void, StarException> status;
  auto append = [&]<typename Text>(Text const& next)
    requires std::convertible_to<Text const&, String> {
    if (status)
      status = append_joined(result, join, next);
  };
  append(second);
  append(third);
  (append(rest), ...);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return result;
}

template <string_tag_lookup Lookup>
[[nodiscard]] auto String::lookup_tags(Lookup&& lookup) const {
  using Replacement = std::remove_cvref_t<std::invoke_result_t<Lookup&, String const&>>;
  constexpr bool fallible = std::same_as<Replacement, std::expected<String, StarException>>;
  using Result = std::conditional_t<fallible, std::expected<String, StarException>, String>;
  // Tag delimiters and offsets are bytes, not codepoints.

  std::string finalString;

  std::size_t start = 0;
  std::size_t size = m_string.size();

  finalString.reserve(size);

  String key;

  while (true) {
    if (start >= size)
      break;

    std::size_t beginTag = m_string.find("<", start);
    std::size_t endTag = m_string.find(">", beginTag);
    if (beginTag != NPos && endTag != NPos) {
      key.m_string.append(m_string, beginTag + 1, endTag - beginTag - 1);
      finalString.append(m_string, start, beginTag - start);
      decltype(auto) replacement = std::invoke(lookup, key);
      if constexpr (fallible) {
        if (!replacement) return Result(std::unexpected(std::move(replacement.error())));
        finalString += replacement->m_string;
      } else {
        finalString += replacement.m_string;
      }
      key.m_string.clear();
      start = endTag + 1;

    } else {
      finalString.append(m_string, start, std::string::npos);
      break;
    }
  }

  return Result(String(std::move(finalString)));
}

template <string_view_tag_lookup Lookup>
[[nodiscard]] auto String::maybe_lookup_tags_view(Lookup&& lookup) const {
  using Replacement = std::remove_cvref_t<std::invoke_result_t<Lookup&, std::string_view>>;
  constexpr bool fallible = std::same_as<Replacement, std::expected<String, StarException>>;
  using Result = std::conditional_t<fallible, std::expected<std::optional<String>, StarException>, std::optional<String>>;
  std::string finalString;
  std::string_view view(utf8());

  std::size_t start = 0;
  while (true) {
    if (start >= view.size())
      break;

    std::size_t beginTag = view.find_first_of('<', start);
    if (beginTag == NPos && !start)
      return Result(std::optional<String>());

    std::size_t endTag = view.find_first_of('>', beginTag);
    if (beginTag != NPos && endTag != NPos) {
      if (!start)
        finalString.reserve(view.size());
      finalString.append(view.substr(start, beginTag - start));
      decltype(auto) replacement = std::invoke(lookup, view.substr(beginTag + 1, endTag - beginTag - 1));
      if constexpr (fallible) {
        if (!replacement) return Result(std::unexpected(std::move(replacement.error())));
        finalString.append(replacement->utf8());
      } else {
        finalString.append(replacement.utf8());
      }
      start = endTag + 1;
    } else {
      finalString.append(view.substr(start));
      break;
    }
  }

  return Result(std::optional<String>(String(std::move(finalString))));
}

template <string_view_tag_lookup Lookup>
[[nodiscard]] auto String::lookup_tags_view(Lookup&& lookup) const {
  using Replacement = std::remove_cvref_t<std::invoke_result_t<Lookup&, std::string_view>>;
  constexpr bool fallible = std::same_as<Replacement, std::expected<String, StarException>>;
  auto result = maybe_lookup_tags_view(lookup);
  if constexpr (fallible) {
    if (!result) return std::expected<String, StarException>(std::unexpected(std::move(result.error())));
    return std::expected<String, StarException>(*result ? std::move(**result) : String());
  } else {
    return result ? std::move(*result) : String();
  }
}

template <string_tag_map MapType>
String String::replace_tags(MapType const& tags, bool replaceWithDefault, String defaultValue) const {
  return lookup_tags([&](String const& key) -> String {
    auto i = tags.find(key);
    if (i == tags.end()) {
      if (replaceWithDefault)
        return defaultValue;
      else
        return "<" + key + ">";
    } else {
      return i->second;
    }
  });
}

inline std::size_t hash<String>::operator()(String const& s) const { return (*this)(std::string_view(s.utf8())); }

template <typename Range>
  requires sequence_source<Range const&, String>
StringList StringList::from(Range const& range) {
  return StringList(std::from_range, range);
}

template <typename Comparator>
  requires requires(List<String>& list, Comparator&& compare) { list.sort(std::forward<Comparator>(compare)); }
StringList StringList::sorted(Comparator&& comparator) const {
  StringList l(*this);
  l.sort(std::forward<Comparator>(comparator));
  return l;
}
}

export module star.string;

export namespace Star {
  template <>
  struct hash<String>;
  template <>
  struct hash<StringList>;
}


export namespace Star {
  using ::Star::StringExceptionTag;
  using ::Star::StringException;
  using ::Star::string_tag_lookup;
  using ::Star::string_view_tag_lookup;
  using ::Star::string_tag_map;
  using ::Star::String;
  using ::Star::StringList;
  using ::Star::StringView;
  using ::Star::CaseInsensitiveString;
  using ::Star::CaseInsensitiveStringView;
  using ::Star::CaseInsensitiveStringHash;
  using ::Star::CaseInsensitiveStringCompare;
  using ::Star::StringSet;
  using ::Star::CaseInsensitiveStringSet;
  using ::Star::StringMap;
  using ::Star::StableStringMap;
  using ::Star::CaseInsensitiveStringMap;
  using ::Star::hash;
  using ::Star::write_text;
}

export template <> struct std::formatter<Star::String> : std::formatter<std::string> {
  template <typename Context>
    requires requires(std::formatter<std::string> const& formatter, std::string const& text, Context& context) {
      formatter.format(text, context);
    }
  auto format(Star::String const& s, Context& ctx) const -> typename Context::iterator {
    return std::formatter<std::string>::format(s.utf8(), ctx);
  }
};

export template <> struct std::formatter<Star::CaseInsensitiveString> : std::formatter<std::string> {
  template <typename Context>
    requires requires(std::formatter<std::string> const& formatter, std::string const& text, Context& context) {
      formatter.format(text, context);
    }
  auto format(Star::CaseInsensitiveString const& key, Context& ctx) const -> typename Context::iterator {
    return std::formatter<std::string>::format(key.string().utf8(), ctx);
  }
};

export template <> struct std::formatter<Star::StringList> : Star::TextFormatter {};
