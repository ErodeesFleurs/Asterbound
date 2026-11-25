module;
import std;
import star.string;
import star.list;
import star.map;
import star.format;
import star.text_formatter;
import star.hash;

import star.json;
import star.item_descriptor;
import star.game_types;

namespace Star {

struct RecipeExceptionTag {
  static constexpr std::string_view name() { return "RecipeException"; }
};
using RecipeException = StarError<RecipeExceptionTag, StarException>;

struct RecipeStock {
  void add(ItemDescriptor const& item);
  StringMap<std::uint64_t> by_name;
  HashMap<ItemDescriptor, std::uint64_t> by_parameters;
};

struct ItemRecipe {
  [[nodiscard]]
  std::expected<Json, StarException> to_json() const;

  bool is_null() const;

  bool operator==(ItemRecipe const& rhs) const;

  StringMap<std::uint64_t> currency_inputs;
  List<ItemDescriptor> inputs;
  ItemDescriptor output;
  float duration = 0;
  StringSet groups;
  Rarity output_rarity = Rarity::Common;
  String gui_filter_string;
  StringMap<String> collectables;
  bool match_input_parameters = false;
  // Catalog tie-break only; not part of recipe identity or serialization.
  String source_path;
};

template <>
struct ValueHash<ItemRecipe> {
  std::size_t operator()(ItemRecipe const& recipe) const;
};

[[nodiscard]]
std::expected<void, StarException> write_text(TextBuffer& output, ItemRecipe const& recipe);
}

export module star.item_recipe;

export namespace Star {
  template <>
  struct ValueHash<ItemRecipe>;
  using ::Star::RecipeExceptionTag;
  using ::Star::RecipeException;
  using ::Star::RecipeStock;
  using ::Star::ItemRecipe;
  using ::Star::ValueHash;
  using ::Star::write_text;
}
