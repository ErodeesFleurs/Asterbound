export module star.numeric_option;

import std;
import star.format;
import star.lexical_cast;
import star.option_parser;
import star.string;

export namespace Star {

enum class EmptyParameterList { UseFallback, Reject };

template <typename T>
concept numeric_option_target = requires(std::string_view text, T fallback) {
  { lexical_cast<T>(text) } -> std::same_as<std::expected<T, StarException>>;
  { std::expected<T, StarException>(std::in_place, std::move(fallback)) };
};

template <numeric_option_target T>
[[nodiscard]] std::expected<T, StarException> numeric_option(OptionParser::Options const& options,
    std::string_view name, T fallback, EmptyParameterList empty_policy) {
  auto values = options.parameters.find(name);
  if (values == options.parameters.end()
      || (empty_policy == EmptyParameterList::UseFallback && values->second.empty()))
    return std::expected<T, StarException>(std::in_place, std::move(fallback));
  auto text = values->second.first();
  if (!text)
    return std::unexpected(std::move(text.error()));
  return lexical_cast<T>(std::string_view(text->get().utf8()));
}

}
