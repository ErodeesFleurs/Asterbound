import star.hash;
import star.json_extra;
import star.json;
import star.item_recipe;
import star.format;
import star.text_formatter;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.bi_map;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;

import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;

namespace Star {

std::expected<Json, StarException> ItemRecipe::toJson() const {
  JsonArray inputList;
  inputList.reserve(inputs.size());
  for (auto const& input : inputs)
    inputList.push_back(input.to_json());
  auto currencies = json_from_map(currencyInputs);
  if (!currencies)
    return std::unexpected(std::move(currencies.error()));
  auto collectableJson = json_from_map(collectables);
  if (!collectableJson)
    return std::unexpected(std::move(collectableJson.error()));
  return Json(JsonObject{
      {"currencyInputs", std::move(*currencies)},
      {"input", std::move(inputList)},
      {"output", output.to_json()},
      {"duration", duration},
      {"groups", json_from_string_set(groups)},
      {"collectables", std::move(*collectableJson)},
      {"matchInputParameters", matchInputParameters}
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

bool ItemRecipe::isNull() const { return currencyInputs.empty() && inputs.empty() && output.is_null(); }

bool ItemRecipe::operator==(ItemRecipe const& rhs) const {
  return matchInputParameters == rhs.matchInputParameters && std::tie(currencyInputs, inputs, output) == std::tie(rhs.currencyInputs, rhs.inputs, rhs.output);
}

bool ItemRecipe::operator!=(ItemRecipe const& rhs) const {
  return !(*this == rhs);
}

std::expected<void, StarException> write_text(TextBuffer& output, ItemRecipe const& recipe) {
  output.append("CurrencyInputs: ");
  append_text(output, recipe.currencyInputs);
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

std::size_t ValueHash<ItemRecipe>::operator()(ItemRecipe const& v) const {
  return hash_of(v.currencyInputs.keys(), v.currencyInputs.values(), v.inputs, v.output);
}

}
