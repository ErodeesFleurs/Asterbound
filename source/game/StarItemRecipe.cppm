module;
import std;
import star.string;
import star.list;
import star.format;
import star.text_formatter;
import star.hash;

import star.json;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.game_types;

namespace Star {

struct RecipeExceptionTag {
  static constexpr std::string_view name() { return "RecipeException"; }
};
using RecipeException = StarError<RecipeExceptionTag, StarException>;

struct ItemRecipe {
  friend std::expected<void, StarException> write_text(TextBuffer& output, ItemRecipe const& recipe);
  [[nodiscard]]
  std::expected<Json, StarException> toJson() const;

  bool isNull() const;

  bool operator==(ItemRecipe const& rhs) const;
  bool operator!=(ItemRecipe const& rhs) const;

  StringMap<std::uint64_t> currencyInputs;
  List<ItemDescriptor> inputs;
  ItemDescriptor output;
  float duration;
  StringSet groups;
  Rarity outputRarity;
  String guiFilterString;
  StringMap<String> collectables;
  bool matchInputParameters;
};

template <>
struct hash<ItemRecipe> {
  std::size_t operator()(ItemRecipe const& v) const;
};

[[nodiscard]]
std::expected<void, StarException> write_text(TextBuffer& output, ItemRecipe const& recipe);
}

export module star.item_recipe;

export namespace Star {
  template <>
  struct hash<ItemRecipe>;
}



export namespace Star {
  using ::Star::RecipeExceptionTag;
  using ::Star::RecipeException;
  using ::Star::ItemRecipe;
  using ::Star::hash;
  using ::Star::write_text;
}
