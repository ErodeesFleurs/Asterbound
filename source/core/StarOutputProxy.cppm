module;
import std;
import star.stream_formatter;
import star.type_identity;
import star.optional;
import star.format;

namespace Star {

namespace OutputAnyDetail {
  template<typename T, typename CharT, typename Traits>
  std::basic_string<CharT, Traits> string(T const& t) {
    return std::format("<type {} at address: {}>", typeName<T>(), (void*)&t);
  }

  template<typename T, typename CharT, typename Traits>
  std::basic_ostream<CharT, Traits>& output(std::basic_ostream<CharT, Traits>& os, T const& t) {
    return os << string<T, CharT, Traits>(t);
  }

  namespace Operator {
    template<typename T, typename CharT, typename Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, T const& t) {
      return output(os, t);
    }
  }

  template <typename T>
  struct Wrapper {
    T const& wrapped;
  };

  template <typename T>
  decltype(auto) outputValue(std::ostream& os, T const& value) {
    using namespace Operator;
    if constexpr (requires { outputOptional(value); })
      return os << outputOptional(value);
    else
      return os << value;
  }

  template <typename T>
    requires std::same_as<decltype(outputValue(std::declval<std::ostream&>(), std::declval<T const&>())), std::ostream&>
  std::ostream& operator<<(std::ostream& os, Wrapper<T> const& wrapper) {
    return outputValue(os, wrapper.wrapped);
  }

  template <typename T>
    requires std::same_as<decltype(outputValue(std::declval<std::ostream&>(), std::declval<T const&>())),
        std::expected<std::reference_wrapper<std::ostream>, StarException>>
  [[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, Wrapper<T> const& wrapper) {
    return outputValue(os, wrapper.wrapped);
  }
}

// Unprintable values use diagnostic type/address text; fallible printers retain
// their checked result rather than being erased into a void callback.
template <typename T>
OutputAnyDetail::Wrapper<T> outputAny(T const& t) {
  return OutputAnyDetail::Wrapper<T>{t};
}

struct OutputProxy {
  using PrintFunction = std::function<void(std::ostream&)>;

  template <typename Printer>
    requires std::invocable<Printer&, std::ostream&>
        && (!requires { typename std::remove_cvref_t<std::invoke_result_t<Printer&, std::ostream&>>::error_type; })
  OutputProxy(Printer&& printer)
    : m_print(std::forward<Printer>(printer)) {}

private:
  friend std::ostream& operator<<(std::ostream& os, OutputProxy const& p);
  PrintFunction m_print;
};

inline std::ostream& operator<<(std::ostream& os, OutputProxy const& p) {
  p.m_print(os);
  return os;
}

}

export module star.output_proxy;

export namespace Star {
  using ::Star::outputAny;
  using ::Star::OutputProxy;
  using ::Star::operator<<;
}

export namespace Star::OutputAnyDetail {
  using ::Star::OutputAnyDetail::string;
  using ::Star::OutputAnyDetail::output;
  using ::Star::OutputAnyDetail::Wrapper;
  using ::Star::OutputAnyDetail::operator<<;
}

export namespace Star::OutputAnyDetail::Operator {
  using ::Star::OutputAnyDetail::Operator::operator<<;
}

export template <typename T>
struct std::formatter<Star::OutputAnyDetail::Wrapper<T>> : Star::StreamFormatter {};
export template <> struct std::formatter<Star::OutputProxy> : Star::StreamFormatter {};
