module;
import std;
import star.text_formatter;
import star.format;

namespace Star {

template <typename T>
concept optional_take_value = requires(std::optional<T>& value) {
  std::expected<T, StarException>(std::in_place, std::move(*value));
} && std::is_move_constructible_v<std::expected<T, StarException>>;

// Construct the result before resetting the source. No assignment or default
// payload construction is required, and an empty extraction changes nothing.
template <optional_take_value T>
[[nodiscard]] std::expected<T, StarException> take_optional(std::optional<T>& value) {
  if (!value)
    return std::unexpected(OutOfRangeException("takeOptional called on empty optional"));
  std::expected<T, StarException> result(std::in_place, std::move(*value));
  value.reset();
  return result;
}

template <typename Source>
concept optional_borrow_source = std::is_lvalue_reference_v<Source>
  && std::is_class_v<std::remove_cvref_t<Source>>
  && requires(Source source) {
       static_cast<bool>(source);
       *source;
       requires std::is_lvalue_reference_v<decltype(*source)>;
       std::reference_wrapper<std::remove_reference_t<decltype(*source)>>(*source);
     };

// Borrow only a named nullable source. Typed construction preserves a nested
// reference_wrapper slot instead of unwrapping it as std::ref would.
template <typename Source>
  requires optional_borrow_source<Source&&>
[[nodiscard]] constexpr auto optional_ref(Source&& source)
  noexcept(noexcept(static_cast<bool>(source)) && noexcept(*source))
  -> std::optional<std::reference_wrapper<std::remove_reference_t<decltype(*source)>>> {
  using value_type = std::remove_reference_t<decltype(*source)>;
  if (static_cast<bool>(source))
    return std::reference_wrapper<value_type>(*source);
  return std::nullopt;
}

// Diagnostic views never own their optional. Reject temporary owners both at
// the factory and direct-construction boundary, including const temporaries.
template <typename T>
class OptionalOutput {
public:
  explicit constexpr OptionalOutput(std::optional<T> const& value) noexcept : m_value(value) {}
  OptionalOutput(std::optional<T>&&) = delete;
  OptionalOutput(std::optional<T> const&&) = delete;

  constexpr std::optional<T> const& get() const noexcept { return m_value.get(); }

private:
  std::reference_wrapper<std::optional<T> const> m_value;
};

template <typename T>
[[nodiscard]] constexpr OptionalOutput<T> output_optional(std::optional<T> const& value) noexcept {
  return OptionalOutput<T>(value);
}

template <typename T>
void output_optional(std::optional<T>&&) = delete;
template <typename T>
void output_optional(std::optional<T> const&&) = delete;

template <typename T>
[[nodiscard]] constexpr decltype(auto) optional_diagnostic_value(T const& value) {
  if constexpr (requires { output_optional(value); })
    return output_optional(value);
  else
    return (value);
}

template <typename T>
void optional_diagnostic_value(T const&&) = delete;

template <typename T>
using optional_diagnostic_result = decltype(append_text(std::declval<TextBuffer&>(),
    optional_diagnostic_value(std::declval<T const&>())));

template <typename T>
concept optional_text_value = requires(TextBuffer& output, T const& value) {
  append_text(output, optional_diagnostic_value(value));
} && (std::same_as<optional_diagnostic_result<T>, void>
      || std::same_as<optional_diagnostic_result<T>, std::expected<void, StarException>>);

template <optional_text_value T>
[[nodiscard]] auto write_text(TextBuffer& output, OptionalOutput<T> const& view) -> optional_diagnostic_result<T> {
  using result_type = optional_diagnostic_result<T>;
  auto const& value = view.get();
  if (!value) {
    output.append("Nothing");
    return result_type();
  }
  output.append("Just (");
  if constexpr (std::is_void_v<result_type>) {
    append_text(output, optional_diagnostic_value(*value));
  } else {
    auto written = append_text(output, optional_diagnostic_value(*value));
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  output.push_back(')');
  return result_type();
}

}

export module star.optional;

export template <typename T>
  requires Star::nonfallible_text_writer<Star::OptionalOutput<T>>
struct std::formatter<Star::OptionalOutput<T>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::take_optional;
  using ::Star::optional_ref;
  using ::Star::optional_take_value;
  using ::Star::optional_borrow_source;
  using ::Star::optional_text_value;
  using ::Star::OptionalOutput;
  using ::Star::output_optional;
  using ::Star::optional_diagnostic_value;
  using ::Star::optional_diagnostic_result;
  using ::Star::write_text;
}
