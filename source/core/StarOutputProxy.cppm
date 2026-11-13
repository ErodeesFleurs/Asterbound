module;
import std;
import star.text_formatter;
import star.type_identity;
import star.optional;
import star.format;

namespace Star {

namespace output_any_detail {
  template <typename T>
  struct Wrapper {
    T const& wrapped;
  };

  template <typename T>
  decltype(auto) append_value(TextBuffer& buffer, T const& value) {
    if constexpr (requires { output_optional(value); }) {
      if constexpr (text_writable<decltype(output_optional(value))>)
        return append_text(buffer, output_optional(value));
      else
        std::format_to(std::back_inserter(buffer), "<type {} at address: {}>", type_name<T>(),
                       static_cast<void const*>(std::addressof(value)));
    } else if constexpr (text_writable<T>) {
      return append_text(buffer, value);
    } else {
      std::format_to(std::back_inserter(buffer), "<type {} at address: {}>", type_name<T>(),
                     static_cast<void const*>(std::addressof(value)));
    }
  }

  template <typename T>
  decltype(auto) write_text(TextBuffer& buffer, Wrapper<T> const& wrapper) {
    return append_value(buffer, wrapper.wrapped);
  }
}

// Unprintable values use diagnostic type/address text; supported fallible
// printers are always dispatched and their exact checked result is preserved.
template <typename T>
output_any_detail::Wrapper<T> output_any(T const& value) {
  return {value};
}

struct OutputProxy {
  using PrintFunction = std::function<void(TextBuffer&)>;

  template <typename Printer>
    requires std::is_copy_constructible_v<std::remove_cvref_t<Printer>>
        && requires(Printer& printer, TextBuffer& buffer) {
          { std::invoke(printer, buffer) } -> std::same_as<void>;
          PrintFunction(std::forward<Printer>(printer));
        }
  OutputProxy(Printer&& printer)
    : m_print(std::forward<Printer>(printer)) {}

private:
  friend void write_text(TextBuffer& buffer, OutputProxy const& proxy);
  PrintFunction m_print;
};

inline void write_text(TextBuffer& buffer, OutputProxy const& proxy) {
  proxy.m_print(buffer);
}

}

export module star.output_proxy;

export namespace Star {
  using ::Star::output_any;
  using ::Star::OutputProxy;
  using ::Star::write_text;
}

export namespace Star::output_any_detail {
  using ::Star::output_any_detail::Wrapper;
  using ::Star::output_any_detail::write_text;
}

export template <typename T>
  requires requires(Star::TextBuffer& buffer, Star::output_any_detail::Wrapper<T> const& value) {
    { write_text(buffer, value) } -> std::same_as<void>;
  }
struct std::formatter<Star::output_any_detail::Wrapper<T>> : Star::TextFormatter {};
export template <> struct std::formatter<Star::OutputProxy> : Star::TextFormatter {};
