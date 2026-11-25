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
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  void query_type(TypeQuery& query) const noexcept override { query_type_of<CraftingPane, Pane>(*this, query); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<CraftingPane>(); }

  CraftingPane(ConstructionKey, GuiContext& gui_context);
  [[nodiscard]] static std::expected<std::shared_ptr<CraftingPane>, StarException> create(GuiContext& gui_context,
      std::shared_ptr<WorldClient> world_client, std::shared_ptr<Player> player, Json const& settings, EntityId source_entity_id = null_entity_id);

  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const& screen_position) override;

  EntityId source_entity_id() const;

private:
  struct RecipeInventory {
    RecipeStock stock;
    StringMap<std::uint64_t> balances;
    bool admin;
  };

  struct RecipeListing {
    List<ItemRecipe> recipes;
    bool all_craftable;
  };

  using RecipeInventoryView = std::optional<std::reference_wrapper<RecipeInventory const>>;
  using SelectedRecipe = std::optional<std::reference_wrapper<ItemRecipe const>>;

  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<WorldClient> world_client, std::shared_ptr<Player> player, Json const& settings, EntityId source_entity_id);
  [[nodiscard]] std::expected<void, StarException> upgrade_table();
  [[nodiscard]] std::expected<void, StarException> unlock_configured_recipes(String const& setting);

  [[nodiscard]] std::expected<RecipeInventory, StarException> capture_recipe_inventory() const;
  [[nodiscard]] std::expected<RecipeListing, StarException> determine_recipes(RecipeInventory const& inventory);

  [[nodiscard]] std::expected<void, StarException> update(float dt) override;
  [[nodiscard]] std::expected<void, StarException> update_craft_buttons(RecipeInventory const& inventory, bool selected_available);
  [[nodiscard]] std::expected<void, StarException> refresh_recipes(bool selection_changed = false);
  void stop_crafting();
  [[nodiscard]] std::expected<void, StarException> toggle_craft();
  [[nodiscard]] std::expected<void, StarException> craft(int count);
  [[nodiscard]] std::expected<void, StarException> count_changed();
  [[nodiscard]] std::expected<void, StarException> count_text_changed(RecipeInventoryView inventory = {});
  [[nodiscard]] std::expected<int, StarException> max_craft(RecipeInventoryView inventory = {}) const;
  [[nodiscard]] std::expected<SelectedRecipe, StarException> selected_recipe() const;
  [[nodiscard]] std::expected<void, StarException> setup_widget(std::shared_ptr<Widget> const& widget, ItemRecipe const& recipe, bool available);
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> setup_tooltip(ItemRecipe const& recipe);

  std::shared_ptr<WorldClient> m_world_client;
  std::shared_ptr<Player> m_player;
  std::shared_ptr<PlayerBlueprints> m_blueprints;

  bool m_crafting = false;
  GameTimer m_craft_timer;
  int m_batch_size = 1;
  std::shared_ptr<AudioInstance> m_crafting_sound;
  int m_count = 1;
  BiHashMap<ItemRecipe, std::shared_ptr<Widget>> m_recipe_widgets;

  std::shared_ptr<ListWidget> m_gui_list;
  std::shared_ptr<TextBoxWidget> m_text_box;
  std::shared_ptr<ButtonWidget> m_filter_have_materials;
  std::size_t m_displayed_recipe;

  StringSet m_filter;

  int m_max_spin_count;

  int m_recipe_refresh_cooldown = 0;
  bool m_refreshing_recipes = false;

  HashMap<ItemDescriptor, std::shared_ptr<Item>> m_item_cache;

  EntityId m_source_entity_id;
  Json m_settings;

  std::optional<ItemRecipe> m_upgrade_recipe;
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
