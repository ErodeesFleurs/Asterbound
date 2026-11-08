import star.hash;
import star.json_extra;
import star.json;
import star.item_recipe;
import star.format;
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
import star.version;

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
    inputList.push_back(input.toJson());
  auto currencies = jsonFromMap(currencyInputs);
  if (!currencies)
    return std::unexpected(std::move(currencies.error()));
  auto collectableJson = jsonFromMap(collectables);
  if (!collectableJson)
    return std::unexpected(std::move(collectableJson.error()));
  return Json(JsonObject{
      {"currencyInputs", std::move(*currencies)},
      {"input", std::move(inputList)},
      {"output", output.toJson()},
      {"duration", duration},
      {"groups", jsonFromStringSet(groups)},
      {"collectables", std::move(*collectableJson)},
      {"matchInputParameters", matchInputParameters}
    });
}

bool ItemRecipe::isNull() const { return currencyInputs.empty() && inputs.empty() && output.isNull(); }

bool ItemRecipe::operator==(ItemRecipe const& rhs) const {
  return std::tie(currencyInputs, inputs, output) == std::tie(rhs.currencyInputs, rhs.inputs, rhs.output);
}

bool ItemRecipe::operator!=(ItemRecipe const& rhs) const {
  return std::tie(currencyInputs, inputs, output) != std::tie(rhs.currencyInputs, rhs.inputs, rhs.output);
}

std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& os, ItemRecipe const& recipe) {
  os << "CurrencyInputs: " << recipe.currencyInputs << "Inputs: [";
  bool first = true;
  for (auto const& input : recipe.inputs) {
    if (!first)
      os << ", ";
    first = false;
    auto written = os << input;
    if (!written)
      return std::unexpected(std::move(written.error()));
  }
  os << "]\nOutput: ";
  auto output = os << recipe.output;
  if (!output)
    return std::unexpected(std::move(output.error()));
  os << "\nDuration: " << recipe.duration << "\nGroups: " << recipe.groups;
  return std::ref(os);
}

std::size_t hash<ItemRecipe>::operator()(ItemRecipe const& v) const {
  return hash_of(v.currencyInputs.keys(), v.currencyInputs.values(), v.inputs, v.output);
}

}
