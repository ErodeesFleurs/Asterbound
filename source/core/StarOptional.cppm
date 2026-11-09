module;
import std;
import star.text_formatter;
import star.format;

namespace Star {

// Failed extraction leaves the optional unchanged.
template <typename T>
[[nodiscard]]
std::expected<T, StarException> takeOptional(std::optional<T>& value) {
  if (!value)
    return std::unexpected(OutOfRangeException("takeOptional called on empty optional"));
  T result(std::move(*value));
  value.reset();
  return result;
}

// A nullable borrow never owns storage. Only borrow from a named, live owner.
template <typename T>
std::optional<std::reference_wrapper<T>> optionalRef(std::optional<T>& value) {
  if (value)
    return std::reference_wrapper<T>(*value);
  return std::nullopt;
}

template <typename T>
std::optional<std::reference_wrapper<T const>> optionalRef(std::optional<T> const& value) {
  if (value)
    return std::reference_wrapper<T const>(*value);
  return std::nullopt;
}

template <typename T>
void optionalRef(std::optional<T>&&) = delete;
template <typename T>
void optionalRef(std::optional<T> const&&) = delete;

template <typename Owner>
  requires (std::is_lvalue_reference_v<Owner&&>
      && requires(Owner& owner) { owner.get(); }
      && std::is_pointer_v<decltype(std::declval<Owner&>().get())>)
auto optionalRef(Owner&& owner)
    -> std::optional<std::reference_wrapper<std::remove_pointer_t<decltype(owner.get())>>> {
  using Value = std::remove_pointer_t<decltype(owner.get())>;
  if (owner)
    return std::reference_wrapper<Value>(*owner);
  return std::nullopt;
}

// Explicit diagnostic view: std::optional itself has no formatter hook.
// The referenced optional must outlive the view, including deferred formatting.
template <typename T>
struct OptionalOutput {
  std::optional<T> const& value;
};

template <typename T>
OptionalOutput<T> output_optional(std::optional<T> const& value) {
  return {value};
}

template <typename T>
decltype(auto) optional_diagnostic_value(T const& value) {
  if constexpr (requires { output_optional(value); })
    return output_optional(value);
  else
    return (value);
}

template <typename T>
using optional_diagnostic_result = decltype(append_text(std::declval<TextBuffer&>(),
    optional_diagnostic_value(std::declval<T const&>())));

template <typename T>
void write_text(TextBuffer& output, OptionalOutput<T> const& value)
    requires std::same_as<optional_diagnostic_result<T>, void> {
  if (!value.value) {
    output.append("Nothing");
    return;
  }
  output.append("Just (");
  append_text(output, optional_diagnostic_value(*value.value));
  output.push_back(')');
}

template <typename T>
[[nodiscard]]
std::expected<void, StarException> write_text(TextBuffer& output, OptionalOutput<T> const& value)
    requires std::same_as<optional_diagnostic_result<T>, std::expected<void, StarException>> {
  if (!value.value) {
    output.append("Nothing");
    return {};
  }
  output.append("Just (");
  auto written = append_text(output, optional_diagnostic_value(*value.value));
  if (!written)
    return std::unexpected(std::move(written.error()));
  output.push_back(')');
  return {};
}

}

export module star.optional;

export template <typename T>
  requires Star::nonfallible_text_writer<Star::OptionalOutput<T>>
struct std::formatter<Star::OptionalOutput<T>> : Star::TextFormatter {};

export namespace Star {
  using ::Star::takeOptional;
  using ::Star::optionalRef;
  using ::Star::OptionalOutput;
  using ::Star::output_optional;
  using ::Star::optional_diagnostic_value;
  using ::Star::optional_diagnostic_result;
  using ::Star::write_text;
}
