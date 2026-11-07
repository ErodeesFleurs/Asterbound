module;

import std;
import star.memory;
import star.format;
import star.string;
import star.unicode;

import star.byte_array;
namespace Star {

class String;

// This is a StringView version of Star::String
// I literally just copy-pasted it all from there
class StringView {
public:
  using Char = String::Char;

  using const_iterator = U8ToU32Iterator<std::string_view::const_iterator>;
  using value_type = Char;
  using const_reference = value_type const&;

  using CaseSensitivity = String::CaseSensitivity;

  StringView();
  StringView(StringView const& s);
  StringView(StringView&& s) noexcept;
  StringView(String const& s);

  // These assume utf8 input
  template <std::size_t N>
  StringView(char const (&s)[N]) : m_view(s) {}
  StringView(std::string_view const& s);
  StringView(std::string_view&& s) noexcept;
  StringView(std::string const& s);


  // const& to internal utf8 data
  std::string_view const& utf8() const;
  std::string_view takeUtf8();
  ByteArray utf8Bytes() const;
  // Pointer to the viewed bytes; not necessarily null-terminated.
  char const* utf8Ptr() const;
  std::size_t utf8Size() const;

  [[nodiscard]] std::expected<const_iterator, StarException> begin() const;
  const_iterator end() const;

  [[nodiscard]] std::expected<std::size_t, StarException> size() const;
  [[nodiscard]] std::expected<std::size_t, StarException> length() const;

  bool empty() const;

  [[nodiscard]] std::expected<Char, StarException> operator[](std::size_t index) const;
  [[nodiscard]] std::expected<Char, StarException> at(std::size_t i) const;

  [[nodiscard]] std::expected<bool, StarException> endsWith(StringView end, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> endsWith(Char end, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> beginsWith(StringView beg, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> beginsWith(Char beg, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  using SplitCallback = std::function<std::expected<void, StarException>(StringView, std::size_t, std::size_t)>;
  [[nodiscard]] std::expected<void, StarException> forEachSplitAnyView(StringView pattern, SplitCallback) const;
  [[nodiscard]] std::expected<void, StarException> forEachSplitView(StringView pattern, SplitCallback) const;

  [[nodiscard]] std::expected<bool, StarException> hasChar(Char c) const;
  // Identical to hasChar, except, if string is empty, tests if c is
  // whitespace.
  [[nodiscard]] std::expected<bool, StarException> hasCharOrWhitespace(Char c) const;

  [[nodiscard]] std::expected<std::size_t, StarException> find(Char c, std::size_t beg = 0, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find(StringView s, std::size_t beg = 0, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> findLast(Char c, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> findLast(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  // If pattern is empty, finds first whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> findFirstOf(StringView chars = "", std::size_t beg = 0) const;

  // If pattern is empty, finds first non-whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> findFirstNotOf(StringView chars = "", std::size_t beg = 0) const;

  // finds the the start of the next 'boundary' in a string.  used for quickly
  // scanning a string
  [[nodiscard]] std::expected<std::size_t, StarException> findNextBoundary(std::size_t index, bool backwards = false) const;

  [[nodiscard]] std::expected<bool, StarException> contains(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  [[nodiscard]] std::expected<int, StarException> compare(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equals(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equalsIgnoreCase(StringView s) const;

  [[nodiscard]] std::expected<StringView, StarException> substr(std::size_t position, std::size_t n = NPos) const;

  StringView& operator=(StringView s);

  friend bool operator==(StringView s1, std::string_view s2);
  template <std::size_t N>
  friend bool operator==(StringView s, char const (&text)[N]) { return s.m_view == std::string_view(text); }
  friend bool operator==(StringView s1, std::string const& s2);
  friend bool operator==(StringView s1, String const& s2);
  friend bool operator==(StringView s1, StringView s2);
  friend bool operator!=(StringView s1, StringView s2);
  friend bool operator<(StringView s1, StringView s2);

  friend std::ostream& operator<<(std::ostream& os, StringView const& s);

private:
  friend class String;
  [[nodiscard]] std::expected<int, StarException> compare(std::size_t selfOffset,
      std::size_t selfLen,
      StringView other,
      std::size_t otherOffset,
      std::size_t otherLen,
      CaseSensitivity cs) const;

  std::string_view m_view;
};

}

export module star.string_view;

export namespace Star {
  using ::Star::StringView;
}

export template <> struct std::formatter<Star::StringView> : std::formatter<std::string_view> {
  template <typename Context>
  auto format(Star::StringView const& s, Context& ctx) const -> typename Context::iterator {
    return std::formatter<std::string_view>::format(s.utf8(), ctx);
  }
};
