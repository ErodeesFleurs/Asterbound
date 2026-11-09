module;
import std;
import star.format;
import star.string;
import star.string_view;

namespace Star {

struct BadLexicalCastTag {
  static constexpr std::string_view name() { return "BadLexicalCast"; }
};
using BadLexicalCast = StarError<BadLexicalCastTag, StarException>;

BadLexicalCast lexical_cast_error(std::errc ec, std::string_view text);

namespace lexical_detail {
template <typename Type>
concept from_chars_target = std::same_as<Type, std::remove_cvref_t<Type>>
    && (!std::same_as<Type, bool>)
    && requires(char const* first, char const* last, Type& value) {
      { std::from_chars(first, last, value) } -> std::same_as<std::from_chars_result>;
    };

template <typename Type>
concept lexical_target = std::same_as<Type, bool> || from_chars_target<Type>;

template <typename Type>
concept owning_lexical_target = lexical_target<Type> && std::default_initializable<Type>
    && (std::is_constructible_v<Type, Type&&> || std::is_constructible_v<Type, Type const&>);

// Successful results include fully consumed range errors. An unexpected zero
// errc preserves the existing partial-token diagnostic spelling.
template <lexical_target Type>
[[nodiscard]] std::expected<void, std::errc> parse_token(Type& result, std::string_view text) {
  if constexpr (std::same_as<Type, bool>) {
    if (text == "true")
      result = true;
    else if (text == "false")
      result = false;
    else
      return std::unexpected(std::errc());
  } else {
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ptr != text.data() + text.size()
        || (parsed.ec != std::errc() && parsed.ec != std::errc::result_out_of_range))
      return std::unexpected(parsed.ec);
  }
  return {};
}
}

// Full-token range errors remain successful: integers retain the destination,
// while the pinned libc++ floating parser writes signed infinity or zero.
template <lexical_detail::lexical_target Type>
[[nodiscard]] bool try_lexical_cast(Type& result, std::string_view text) {
  return bool(lexical_detail::parse_token(result, text));
}

template <lexical_detail::lexical_target Type>
[[nodiscard]] bool try_lexical_cast(Type& result, String const& s) {
  return try_lexical_cast<Type>(result, std::string_view(s.utf8()));
}

template <lexical_detail::lexical_target Type>
[[nodiscard]] bool try_lexical_cast(Type& result, StringView s) {
  return try_lexical_cast<Type>(result, s.utf8());
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::optional<Type> maybe_lexical_cast(std::string_view text) {
  Type result{};
  if (!try_lexical_cast(result, text))
    return {};
  if constexpr (std::is_constructible_v<Type, Type&&>)
    return std::optional<Type>(std::in_place, std::move(result));
  else
    return std::optional<Type>(std::in_place, std::as_const(result));
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::optional<Type> maybe_lexical_cast(StringView s) {
  return maybe_lexical_cast<Type>(s.utf8());
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::expected<Type, StarException> lexical_cast(std::string_view text) {
  Type result{};
  auto parsed = lexical_detail::parse_token(result, text);
  if (!parsed)
    return std::unexpected(lexical_cast_error(parsed.error(), text));
  if constexpr (std::is_constructible_v<Type, Type&&>)
    return std::expected<Type, StarException>(std::in_place, std::move(result));
  else
    return std::expected<Type, StarException>(std::in_place, std::as_const(result));
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::expected<Type, StarException> lexical_cast(StringView s) {
  return lexical_cast<Type>(s.utf8());
}

template <lexical_detail::lexical_target Type, std::size_t N>
[[nodiscard]] bool try_lexical_cast(Type& result, char const (&text)[N]) {
  return try_lexical_cast<Type>(result, std::string_view(text));
}

template <lexical_detail::lexical_target Type>
[[nodiscard]] bool try_lexical_cast(Type& result, std::string const& text) {
  return try_lexical_cast<Type>(result, std::string_view(text));
}

template <lexical_detail::owning_lexical_target Type, std::size_t N>
[[nodiscard]] std::optional<Type> maybe_lexical_cast(char const (&text)[N]) {
  return maybe_lexical_cast<Type>(std::string_view(text));
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::optional<Type> maybe_lexical_cast(std::string const& text) {
  return maybe_lexical_cast<Type>(std::string_view(text));
}

template <lexical_detail::owning_lexical_target Type, std::size_t N>
[[nodiscard]] std::expected<Type, StarException> lexical_cast(char const (&text)[N]) {
  return lexical_cast<Type>(std::string_view(text));
}

template <lexical_detail::owning_lexical_target Type>
[[nodiscard]] std::expected<Type, StarException> lexical_cast(std::string const& text) {
  return lexical_cast<Type>(std::string_view(text));
}

}

export module star.lexical_cast;

export namespace Star {
  using ::Star::BadLexicalCastTag;
  using ::Star::BadLexicalCast;
  using ::Star::lexical_cast_error;
  using ::Star::try_lexical_cast;
  using ::Star::maybe_lexical_cast;
  using ::Star::lexical_cast;
}
