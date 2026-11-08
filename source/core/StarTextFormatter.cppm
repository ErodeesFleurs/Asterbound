module;
import std;

namespace Star {

// Ordinary diagnostic representations stay on the stack; only longer text spills.
class TextBuffer {
public:
  using value_type = char;

  void append(std::string_view text) {
    if (text.empty())
      return;
    if (m_dynamic.empty() && text.size() <= m_inline.size() - m_size) {
      std::copy_n(text.data(), text.size(), m_inline.data() + m_size);
      m_size += text.size();
      return;
    }
    if (m_dynamic.empty()) {
      m_dynamic.reserve(std::max(m_inline.size() * 2, m_size + text.size()));
      m_dynamic.assign(m_inline.data(), m_size);
    }
    m_dynamic.append(text);
  }

  void push_back(char value) {
    append(std::string_view(&value, 1));
  }

  std::string_view view() const& {
    if (!m_dynamic.empty())
      return m_dynamic;
    return {m_inline.data(), m_size};
  }

  std::string_view view() const&& = delete;

  std::string str() && {
    if (!m_dynamic.empty())
      return std::move(m_dynamic);
    return std::string(m_inline.data(), m_size);
  }

private:
  std::array<char, 512> m_inline;
  std::size_t m_size = 0;
  std::string m_dynamic;
};

namespace TextDetail {

// Reject conversions to unrelated owners (for example primitive -> Json).
// Exact owner overloads and constrained generic writers still win ADL.
template <typename T>
void write_text(TextBuffer&, T const&) = delete;

template <typename T>
concept adl_text_writable = requires(TextBuffer& output, T const& value) {
  write_text(output, value);
};

template <adl_text_writable T>
decltype(auto) write_adl_text(TextBuffer& output, T const& value) {
  return write_text(output, value);
}

// Test the formatter operation itself. In particular, do not ask whether a
// project TextFormatter is formattable while deciding its own write_text hook.
template <typename T>
concept standard_text_writable = requires(std::formatter<std::remove_cvref_t<T>>& formatter,
    std::formatter<std::remove_cvref_t<T>> const& const_formatter,
    T const& value, std::format_parse_context& parse_context, std::format_context& context) {
  std::formatter<std::remove_cvref_t<T>>{};
  { formatter.parse(parse_context) } -> std::same_as<std::format_parse_context::iterator>;
  { const_formatter.format(value, context) } -> std::same_as<std::format_context::iterator>;
};

template <typename T>
concept scalar_enum_text_writable = std::is_enum_v<T> && requires(T const& value) {
  +value;
} && standard_text_writable<decltype(+std::declval<T const&>())>;

template <typename T>
concept raw_pointer_text_writable = std::is_pointer_v<T> && requires(T value) {
  static_cast<void const volatile*>(value);
};

template <typename T>
struct standard_smart_pointer : std::false_type {};

template <typename T>
struct standard_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template <typename T, typename Deleter>
struct standard_smart_pointer<std::unique_ptr<T, Deleter>> : std::true_type {};

template <typename T>
concept smart_pointer_text_writable = standard_smart_pointer<std::remove_cvref_t<T>>::value
    && requires(T const& value) { value.get(); }
    && raw_pointer_text_writable<decltype(std::declval<T const&>().get())>;

}

inline void append_text(TextBuffer& output, std::string_view value) {
  output.append(value);
}

template <typename T>
  requires TextDetail::adl_text_writable<T>
decltype(auto) append_text(TextBuffer& output, T const& value) {
  return TextDetail::write_adl_text(output, value);
}

template <typename T>
  requires (!TextDetail::adl_text_writable<T>)
      && (TextDetail::standard_text_writable<T> || TextDetail::scalar_enum_text_writable<T>
          || TextDetail::raw_pointer_text_writable<T> || TextDetail::smart_pointer_text_writable<T>)
void append_text(TextBuffer& output, T const& value) {
  using Value = std::remove_cvref_t<T>;
  if constexpr (std::same_as<Value, bool>) {
    output.push_back(value ? '1' : '0');
  } else if constexpr (std::same_as<Value, signed char> || std::same_as<Value, unsigned char>) {
    output.push_back(static_cast<char>(value));
  } else if constexpr (std::floating_point<Value>) {
    std::format_to(std::back_inserter(output), "{:.6g}", value);
  } else if constexpr (TextDetail::scalar_enum_text_writable<Value>) {
    std::format_to(std::back_inserter(output), "{}", +value);
  } else if constexpr (std::same_as<Value, std::nullptr_t>) {
    output.append("nullptr");
  } else if constexpr (TextDetail::smart_pointer_text_writable<Value>) {
    append_text(output, value.get());
  } else if constexpr (TextDetail::raw_pointer_text_writable<Value>
      && (!std::same_as<Value, char*> && !std::same_as<Value, char const*>
          && !std::same_as<Value, signed char*> && !std::same_as<Value, signed char const*>
          && !std::same_as<Value, unsigned char*> && !std::same_as<Value, unsigned char const*>)) {
    if (!value)
      output.push_back('0');
    else
      std::format_to(std::back_inserter(output), "{}",
          const_cast<void const*>(static_cast<void const volatile*>(value)));
  } else if constexpr (std::same_as<Value, signed char*> || std::same_as<Value, signed char const*>
      || std::same_as<Value, unsigned char*> || std::same_as<Value, unsigned char const*>) {
    output.append(reinterpret_cast<char const*>(value));
  } else if constexpr (std::same_as<Value, std::string>
      || std::same_as<Value, char*> || std::same_as<Value, char const*>) {
    output.append(value);
  } else {
    std::format_to(std::back_inserter(output), "{}", value);
  }
}

template <typename T>
concept text_writable = requires(TextBuffer& output, T const& value) {
  append_text(output, value);
};

template <typename T>
concept nonfallible_text_writable = requires(TextBuffer& output, T const& value) {
  { append_text(output, value) } -> std::same_as<void>;
};

template <typename T>
concept nonfallible_text_writer = TextDetail::adl_text_writable<T>
    && requires(TextBuffer& output, T const& value) {
      { TextDetail::write_adl_text(output, value) } -> std::same_as<void>;
    };

// Compose the standard string formatter so alignment, width and precision are
// preserved without allocating a string for short project representations.
struct TextFormatter {
  constexpr auto parse(std::format_parse_context& context) {
    return m_string.parse(context);
  }

  void set_debug_format() = delete;

  template <typename T, typename Context>
    requires nonfallible_text_writer<T>
  auto format(T const& value, Context& context) const -> typename Context::iterator {
    TextBuffer output;
    TextDetail::write_adl_text(output, value);
    return m_string.format(output.view(), context);
  }

private:
  std::formatter<std::string_view> m_string;
};

}

export module star.text_formatter;

export namespace Star {
using ::Star::TextBuffer;
using ::Star::TextFormatter;
using ::Star::append_text;
using ::Star::text_writable;
using ::Star::nonfallible_text_writable;
using ::Star::nonfallible_text_writer;
}
