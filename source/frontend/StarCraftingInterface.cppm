module;
import std;
import star.format;
import star.type_identity;
import star.json;

import star.data_stream;
import star.bi_map;
import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;


import star.item_descriptor;
import star.item_recipe;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.pane;
import star.game_timers;

namespace Star {
class Player;
}

namespace Star {

class WorldClient;
class PlayerBlueprints;
class ListWidget;
class TextBoxWidget;
class ButtonWidget;
class LabelWidget;
class AudioInstance;

class CraftingPane : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<CraftingPane, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CraftingPane>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<CraftingPane>, StarException> create(GuiContext& guiContext,
      std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId = NullEntityId);

  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const& screenPosition) override;

  EntityId sourceEntityId() const;

private:
  explicit CraftingPane(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, Json const& settings, EntityId sourceEntityId);
  [[nodiscard]] std::expected<void, StarException> upgradeTable();
  [[nodiscard]] std::expected<void, StarException> unlockConfiguredRecipes(String const& setting);

  [[nodiscard]] std::expected<List<ItemRecipe>, StarException> determineRecipes();

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  [[nodiscard]] std::expected<void, StarException> updateCraftButtons();
  [[nodiscard]] std::expected<void, StarException> updateAvailableRecipes();
  [[nodiscard]] std::expected<bool, StarException> consumeIngredients(ItemRecipe& recipe, int count);
  void stopCrafting();
  [[nodiscard]] std::expected<void, StarException> toggleCraft();
  [[nodiscard]] std::expected<void, StarException> craft(int count);
  [[nodiscard]] std::expected<void, StarException> countChanged();
  [[nodiscard]] std::expected<void, StarException> countTextChanged();
  [[nodiscard]] std::expected<int, StarException> maxCraft();
  [[nodiscard]] std::expected<ItemRecipe, StarException> recipeFromSelectedWidget() const;
  [[nodiscard]] std::expected<void, StarException> setupWidget(std::shared_ptr<Widget> const& widget, ItemRecipe const& recipe, HashMap<ItemDescriptor, std::uint64_t> const& normalizedBag);

  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> setupTooltip(ItemRecipe const& recipe);

  [[nodiscard]] std::expected<std::size_t, StarException> itemCount(List<std::shared_ptr<Item>> const& store, ItemDescriptor const& item);

  std::shared_ptr<WorldClient> m_worldClient;
  std::shared_ptr<Player> m_player;
  std::shared_ptr<PlayerBlueprints> m_blueprints;

  bool m_crafting;
  GameTimer m_craftTimer;
  std::shared_ptr<AudioInstance> m_craftingSound;
  int m_count;
  List<ItemRecipe> m_recipes;
  BiHashMap<ItemRecipe, std::shared_ptr<Widget>> m_recipesWidgetMap; // maps ItemRecipe to guiList WidgetPtrs

  std::shared_ptr<ListWidget> m_guiList;
  std::shared_ptr<TextBoxWidget> m_textBox;
  std::shared_ptr<ButtonWidget> m_filterHaveMaterials;
  std::size_t m_displayedRecipe;

  StringSet m_filter;

  int m_maxSpinCount;

  int m_recipeAutorefreshCooldown;

  HashMap<ItemDescriptor, std::shared_ptr<Item>> m_itemCache;

  EntityId m_sourceEntityId;
  Json m_settings;

  std::optional<ItemRecipe> m_upgradeRecipe;
};

}

export module star.crafting_interface;

export namespace Star {
  using ::Star::WorldClient;
  using ::Star::PlayerBlueprints;
  using ::Star::ListWidget;
  using ::Star::TextBoxWidget;
  using ::Star::ButtonWidget;
  using ::Star::LabelWidget;
  using ::Star::AudioInstance;
  using ::Star::CraftingPane;
}
