module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.rpc_promise;

import star.lua_components;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.container_entity;




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
import star.container_interactor;
import star.widget_parsing;
import star.gui_reader;

namespace Star {

class ContainerEntity;
class Player;
class WorldClient;
class Item;
class ItemGridWidget;
class ItemBag;

class ContainerPane : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<ContainerPane, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ContainerPane>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<ContainerPane>, StarException> create(GuiContext& guiContext, std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor);

  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const& screenPosition) override;

  [[nodiscard]] std::expected<bool, StarException> giveContainerResult(ContainerResult result);

protected:
  [[nodiscard]] std::expected<void, StarException> update(float dt) override;

private:
  explicit ContainerPane(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<WorldClient> worldClient, std::shared_ptr<Player> player, std::shared_ptr<ContainerInteractor> containerInteractor);
  enum class ExpectingSwap {
    None,
    Inventory,
    SwapSlot,
    SwapSlotStack
  };

  [[nodiscard]] std::expected<void, StarException> swapSlot(ItemGridWidget& grid);
  [[nodiscard]] std::expected<void, StarException> startCrafting();
  [[nodiscard]] std::expected<void, StarException> stopCrafting();
  [[nodiscard]] std::expected<void, StarException> toggleCrafting();
  [[nodiscard]] std::expected<void, StarException> clear();
  [[nodiscard]] std::expected<void, StarException> burn();

  std::shared_ptr<WorldClient> m_worldClient;
  std::shared_ptr<Player> m_player;
  std::shared_ptr<ContainerInteractor> m_containerInteractor;
  std::shared_ptr<ItemBag> m_itemBag;

  ExpectingSwap m_expectingSwap;

  GuiReader m_reader;

  std::optional<LuaWorldUpdateScript> m_script;
};

}

export module star.container_interface;

export namespace Star {
  using ::Star::ContainerEntity;
  using ::Star::Player;
  using ::Star::WorldClient;
  using ::Star::Item;
  using ::Star::ItemGridWidget;
  using ::Star::ItemBag;
  using ::Star::ContainerPane;
}
