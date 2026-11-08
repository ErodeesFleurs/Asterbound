module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
import star.math_common;
import star.id_map;
import star.strong_typedef;
import star.data_stream;
import star.string;
import star.casting;
import star.vector;
import star.game_types;
import star.bi_map;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.rpc_promise;
import star.a_star;
import std;
import star.set;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import star.version;


import star.item;
import star.status_types;
import star.status_effect_item;
import star.effect_source_item;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.activatable_item;
import star.drawable;
import star.pointable_item;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.instrument_item;

export namespace Star {

class InstrumentItem : public Item,
                       public StatusEffectItem,
                       public EffectSourceItem,
                       public ToolUserItem,
                       public ActivatableItem,
                       public PointableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<InstrumentItem, Item, StatusEffectItem, EffectSourceItem, ToolUserItem, ActivatableItem, PointableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<InstrumentItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<InstrumentItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;
  [[nodiscard]]
  std::expected<StringSet, StarException> effectSources() const override;

  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;

  [[nodiscard]]
  std::expected<bool, StarException> active() const override;
  void setActive(bool active) override;
  bool usable() const override;
  [[nodiscard]]
  std::expected<void, StarException> activate() override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  [[nodiscard]]
  std::expected<float, StarException> getAngle(float angle) override;

private:
  explicit InstrumentItem(Root& root) : Item(root) {}
  List<PersistentStatusEffect> m_activeStatusEffects;
  List<PersistentStatusEffect> m_inactiveStatusEffects;
  StringSet m_activeEffectSources;
  StringSet m_inactiveEffectSources;
  List<Drawable> m_drawables;
  List<Drawable> m_activeDrawables;
  int m_activeCooldown;

  float m_activeAngle;
  String m_kind;
};

}

namespace Star {

std::expected<std::shared_ptr<InstrumentItem>, StarException> InstrumentItem::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<InstrumentItem>(new InstrumentItem(root));
  auto initialized = item->initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  item->m_activeCooldown = 0;
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto position = item->instanceValue("handPosition", JsonArray{0, 0}).and_then(jsonToVec2F);
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto drawable =
    Drawable::makeImage(**metadata, AssetPath::relative_to(directory, *image), 1.0f / TilePixels, true, *position);
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  item->m_drawables.push_back(std::move(*drawable));
  auto activeImage = item->instanceValue("activeImage").and_then([](Json const& value) { return value.toString(); });
  if (!activeImage)
    return std::unexpected(std::move(activeImage.error()));
  auto activePosition = item->instanceValue("activeHandPosition", JsonArray{0, 0}).and_then(jsonToVec2F);
  if (!activePosition)
    return std::unexpected(std::move(activePosition.error()));
  auto activeDrawable = Drawable::makeImage(**metadata, AssetPath::relative_to(directory, *activeImage),
                                            1.0f / TilePixels, true, *activePosition);
  if (!activeDrawable)
    return std::unexpected(std::move(activeDrawable.error()));
  item->m_activeDrawables.push_back(std::move(*activeDrawable));
  auto angle = item->instanceValue("activeAngle").and_then([](Json const& value) { return value.toFloat(); });
  if (!angle)
    return std::unexpected(std::move(angle.error()));
  item->m_activeAngle = (*angle / 180.0f) * std::numbers::pi;
  for (auto const& [key, target] : {std::pair{std::string_view("activeStatusEffects"), std::ref(item->m_activeStatusEffects)}, std::pair{std::string_view("inactiveStatusEffects"), std::ref(item->m_inactiveStatusEffects)}}) {
    auto effects = item->instanceValue(key, JsonArray()).and_then([](Json const& value) { return value.toArray(); });
    if (!effects)
      return std::unexpected(std::move(effects.error()));
    for (auto const& value : *effects) {
      auto effect = jsonToPersistentStatusEffect(value);
      if (!effect)
        return std::unexpected(std::move(effect.error()));
      target.get().push_back(std::move(*effect));
    }
  }
  auto activeSources = item->instanceValue("activeEffectSources", JsonArray()).and_then(jsonToStringSet);
  if (!activeSources)
    return std::unexpected(std::move(activeSources.error()));
  item->m_activeEffectSources = std::move(*activeSources);
  auto inactiveSources = item->instanceValue("inactiveEffectSources", JsonArray()).and_then(jsonToStringSet);
  if (!inactiveSources)
    return std::unexpected(std::move(inactiveSources.error()));
  item->m_inactiveEffectSources = std::move(*inactiveSources);
  auto kind = item->instanceValue("kind").and_then([](Json const& value) { return value.toString(); });
  if (!kind)
    return std::unexpected(std::move(kind.error()));
  item->m_kind = std::move(*kind);
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> InstrumentItem::clone() const {
  return std::make_shared<InstrumentItem>(*this);
}

std::expected<List<PersistentStatusEffect>, StarException> InstrumentItem::statusEffects() const {
  auto isActive = active();
  if (!isActive)
    return std::unexpected(std::move(isActive.error()));
  return *isActive ? m_activeStatusEffects : m_inactiveStatusEffects;
}

std::expected<StringSet, StarException> InstrumentItem::effectSources() const {
  auto isActive = active();
  if (!isActive)
    return std::unexpected(std::move(isActive.error()));
  return *isActive ? m_activeEffectSources : m_inactiveEffectSources;
}

std::expected<void, StarException> InstrumentItem::update(float, FireMode, bool, HashSet<MoveControlType> const&) {
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  if (*mode == EntityMode::Master) {
    auto isActive = active();
    if (!isActive)
      return std::unexpected(std::move(isActive.error()));
    if (*isActive) {
      m_activeCooldown--;
      auto emitted = itemOwner->get().addEffectEmitters({"music"});
      if (!emitted)
        return std::unexpected(std::move(emitted.error()));
    }
  }
  return itemOwner->get().instrumentEquipped(m_kind);
}

std::expected<bool, StarException> InstrumentItem::active() const {
  if (!initialized())
    return false;
  if (m_activeCooldown > 0)
    return true;
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  return itemOwner->get().instrumentPlaying();
}

void InstrumentItem::setActive(bool active) {
  if (active)
    m_activeCooldown = 3;
  else
    m_activeCooldown = 0;
}

bool InstrumentItem::usable() const {
  return true;
}

std::expected<void, StarException> InstrumentItem::activate() {
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  return itemOwner->get().interact(InteractAction{InteractActionType::OpenSongbookInterface, itemOwner->get().entityId(), {}});
}

std::expected<List<Drawable>, StarException> InstrumentItem::drawables() const {
  auto isActive = active();
  if (!isActive)
    return std::unexpected(std::move(isActive.error()));
  return *isActive ? m_activeDrawables : m_drawables;
}

std::expected<float, StarException> InstrumentItem::getAngle(float angle) {
  auto isActive = active();
  if (!isActive)
    return std::unexpected(std::move(isActive.error()));
  return *isActive ? m_activeAngle : angle;
}

}
