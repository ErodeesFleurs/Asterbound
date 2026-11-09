module;

import std;
import star.memory;
import star.format;
import star.string;
import star.text_formatter;
import star.unicode;

import star.byte_array;
namespace Star {

class String;
class StringView;

template <class Callback>
concept split_view_callback = requires(Callback& callback, StringView view, std::size_t offset, std::size_t count) {
  { std::invoke(callback, std::move(view), std::size_t{offset}, std::size_t{count}) }
    -> std::same_as<std::expected<void, StarException>>;
};

// A borrowed UTF-8 byte descriptor with checked codepoint operations.
class StringView {
public:
  using Char = String::Char;

  using const_iterator = U8ToU32Iterator<std::string_view::const_iterator>;
  using value_type = Char;
  using const_reference = value_type;

  using CaseSensitivity = String::CaseSensitivity;

  StringView();
  StringView(StringView const& s);
  StringView(StringView&& s) noexcept;
  StringView(String const& s);
  StringView(String&&) = delete;
  StringView(String const&&) = delete;

  // These assume utf8 input
  template <std::size_t N>
  StringView(char const (&s)[N]) : m_view(s) {}
  StringView(std::string_view const& s);
  StringView(std::string_view&& s) noexcept;
  StringView(std::string const& s);
  StringView(std::string&&) = delete;
  StringView(std::string const&&) = delete;


  // The descriptor is a value; its bytes remain borrowed from the owner.
  std::string_view utf8() const;
  std::string_view take_utf8();
  ByteArray utf8_bytes() const;
  // Pointer to the viewed bytes; not necessarily null-terminated.
  char const* utf8_data() const;
  std::size_t utf8_size() const;

  [[nodiscard]] std::expected<const_iterator, StarException> begin() const;
  const_iterator end() const;

  [[nodiscard]] std::expected<std::size_t, StarException> size() const;
  [[nodiscard]] std::expected<std::size_t, StarException> length() const;

  bool empty() const;

  [[nodiscard]] std::expected<Char, StarException> operator[](std::size_t index) const;
  [[nodiscard]] std::expected<Char, StarException> at(std::size_t i) const;

  [[nodiscard]] std::expected<bool, StarException> ends_with(StringView end, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> ends_with(Char end, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> starts_with(StringView beg, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> starts_with(Char beg, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  // Synchronous byte-oriented splitting: offsets/counts and "any" membership
  // are UTF-8 bytes, not decoded codepoints. Empty patterns do not invoke.
  template <split_view_callback Callback>
  [[nodiscard]] std::expected<void, StarException> for_each_split_any(StringView pattern, Callback&& callback) const {
    return for_each_split_view(pattern, callback, true);
  }
  template <split_view_callback Callback>
  [[nodiscard]] std::expected<void, StarException> for_each_split(StringView pattern, Callback&& callback) const {
    return for_each_split_view(pattern, callback, false);
  }
  [[nodiscard]] std::expected<void, StarException> for_each_split_any(StringView pattern, std::nullptr_t) const {
    if (pattern.empty()) return {};
    return std::unexpected(StringException("Split callback is empty"));
  }
  [[nodiscard]] std::expected<void, StarException> for_each_split(StringView pattern, std::nullptr_t) const {
    if (pattern.empty()) return {};
    return std::unexpected(StringException("Split callback is empty"));
  }

  [[nodiscard]] std::expected<bool, StarException> has_char(Char c) const;
  // Identical to has_char, except, if string is empty, tests if c is
  // whitespace.
  [[nodiscard]] std::expected<bool, StarException> has_char_or_whitespace(Char c) const;

  [[nodiscard]] std::expected<std::size_t, StarException> find(Char c, std::size_t beg = 0, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find(StringView s, std::size_t beg = 0, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find_last(Char c, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<std::size_t, StarException> find_last(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  // If pattern is empty, finds first whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> find_first_of(StringView chars = "", std::size_t beg = 0) const;

  // If pattern is empty, finds first non-whitespace
  [[nodiscard]] std::expected<std::size_t, StarException> find_first_not_of(StringView chars = "", std::size_t beg = 0) const;

  // finds the the start of the next 'boundary' in a string.  used for quickly
  // scanning a string
  [[nodiscard]] std::expected<std::size_t, StarException> find_next_boundary(std::size_t index, bool backwards = false) const;

  [[nodiscard]] std::expected<bool, StarException> contains(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;

  [[nodiscard]] std::expected<int, StarException> compare(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equals(StringView s, CaseSensitivity cs = CaseSensitivity::CaseSensitive) const;
  [[nodiscard]] std::expected<bool, StarException> equals_ignore_case(StringView s) const;

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

  friend void write_text(TextBuffer& output, StringView const& text);

private:
  template <split_view_callback Callback>
  static bool split_callback_present(Callback const& callback) {
    if constexpr (std::is_pointer_v<Callback> || std::is_member_pointer_v<Callback>)
      return callback != nullptr;
    else
      return true;
  }
  template <class Result, class... Args>
    requires split_view_callback<std::function<Result(Args...)>>
  static bool split_callback_present(std::function<Result(Args...)> const& callback) {
    return static_cast<bool>(callback);
  }
  template <split_view_callback Callback>
  [[nodiscard]] std::expected<void, StarException> for_each_split_view(StringView pattern, Callback& callback, bool any) const {
    if (pattern.empty()) return {};
    if (!split_callback_present(callback))
      return std::unexpected(StringException("Split callback is empty"));
    std::size_t start = 0;
    for (;;) {
      auto stop = any ? m_view.find_first_of(pattern.m_view, start) : m_view.find(pattern.m_view, start);
      auto count = stop == NPos ? m_view.size() - start : stop - start;
      auto result = std::invoke(callback, StringView(m_view.substr(start, count)),
                                std::size_t{start}, std::size_t{count});
      if (!result) return std::unexpected(std::move(result.error()));
      if (stop == NPos) return {};
      start = stop + (any ? 1 : pattern.m_view.size());
    }
  }

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
  using ::Star::split_view_callback;
  using ::Star::write_text;
}

export template <> struct std::formatter<Star::StringView> : std::formatter<std::string_view> {
  template <typename Context>
    requires requires(std::formatter<std::string_view> const& formatter, std::string_view text, Context& context) {
      formatter.format(text, context);
    }
  auto format(Star::StringView const& s, Context& ctx) const -> typename Context::iterator {
    return std::formatter<std::string_view>::format(s.utf8(), ctx);
  }
};
