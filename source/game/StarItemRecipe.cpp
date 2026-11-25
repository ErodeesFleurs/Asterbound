import std;
import star.format;
import star.hash;
import star.item_descriptor;
import star.item_recipe;
import star.json;
import star.json_extra;
import star.list;
import star.map;
import star.set;
import star.string;
import star.text_formatter;

namespace Star {

std::expected<Json, StarException> ItemRecipe::to_json() const {
  JsonArray input_list;
  input_list.reserve(inputs.size());
  for (auto const& input : inputs)
    input_list.push_back(input.to_json());
  auto currencies = json_from_map(currency_inputs);
  if (!currencies)
    return std::unexpected(std::move(currencies.error()));
  auto collectable_json = json_from_map(collectables);
  if (!collectable_json)
    return std::unexpected(std::move(collectable_json.error()));
  return Json(JsonObject{
      {"currencyInputs", std::move(*currencies)},
      {"input", std::move(input_list)},
      {"output", output.to_json()},
      {"duration", duration},
      {"groups", json_from_string_set(groups)},
      {"collectables", std::move(*collectable_json)},
      {"matchInputParameters", match_input_parameters}
    });
}

void RecipeStock::add(ItemDescriptor const& item) {
  if (!item.count())
    return;
  auto add_count = [&](std::uint64_t& total) {
    total += std::min(item.count(), std::numeric_limits<std::uint64_t>::max() - total);
  };
  add_count(by_name[item.name()]);
  add_count(by_parameters[item.singular()]);
}

bool ItemRecipe::is_null() const { return currency_inputs.empty() && inputs.empty() && output.is_null(); }

bool ItemRecipe::operator==(ItemRecipe const& rhs) const {
  return match_input_parameters == rhs.match_input_parameters && currency_inputs == rhs.currency_inputs
      && inputs == rhs.inputs && output == rhs.output;
}

std::expected<void, StarException> write_text(TextBuffer& output, ItemRecipe const& recipe) {
  output.append("CurrencyInputs: ");
  append_text(output, recipe.currency_inputs);
  output.append("Inputs: [");
  bool first = true;
  for (auto const& input : recipe.inputs) {
    if (!first)
      output.append(", ");
    first = false;
    auto written = write_text(output, input);
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  output.append("]\nOutput: ");
  auto written = write_text(output, recipe.output);
  if (!written)
    return std::unexpected(std::move(written.error()));
  output.append("\nDuration: ");
  append_text(output, recipe.duration);
  output.append("\nGroups: ");
  append_text(output, recipe.groups);
  return {};
}

std::size_t ValueHash<ItemRecipe>::operator()(ItemRecipe const& recipe) const {
  // Currency order is immaterial; ingredient order is part of recipe identity.
  std::size_t currencies = 0;
  for (auto const& [name, amount] : recipe.currency_inputs)
    currencies += hash_of(name, amount);
  return hash_of(currencies, recipe.match_input_parameters, recipe.inputs, recipe.output);
}

}
