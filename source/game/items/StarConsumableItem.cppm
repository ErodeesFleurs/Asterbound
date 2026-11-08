module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.json_extra;
import star.math_common;
import star.id_map;
import star.game_types;
import star.casting;
import star.data_stream;
import star.strong_typedef;
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
import star.random;
import star.set;
import star.string;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;
import star.observer_stream;
import star.either;
import star.periodic_function;
import star.audio;
import star.map;


import star.drawable;
import star.item;
import star.damage_types;
import star.world_geometry;
import star.status_types;
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
import star.status_effect_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;

import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.lua_actor_movement_component;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;

export module star.consumable_item;

export namespace Star {

class ConsumableItem : public Item, public SwingableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ConsumableItem, Item, SwingableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ConsumableItem>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<ConsumableItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  [[nodiscard]]
  std::expected<void, StarException> update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) override;
  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<void, StarException> fireTriggered() override;
  [[nodiscard]]
  std::expected<void, StarException> uninit() override;

private:
  explicit ConsumableItem(Root& root) : Item(root) {}
  [[nodiscard]]
  std::expected<bool, StarException> canUse() const;

  [[nodiscard]]
  std::expected<void, StarException> triggerEffects();
  [[nodiscard]]
  std::expected<void, StarException> maybeConsume();

  StringSet m_blockingEffects;
  std::optional<float> m_foodValue;
  StringSet m_emitters;
  String m_emote;
  bool m_consuming;
};

}

namespace Star {

std::expected<std::shared_ptr<ConsumableItem>, StarException> ConsumableItem::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<ConsumableItem>(new ConsumableItem(root));
  auto initialized = item->initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  item->setWindupTime(0);
  item->setCooldownTime(0.25f);
  item->m_requireEdgeTrigger = true;
  auto start = config.getFloat("swingStart", -60);
  if (!start)
    return std::unexpected(std::move(start.error()));
  item->m_swingStart = *start * std::numbers::pi / 180;
  auto finish = config.getFloat("swingFinish", 40);
  if (!finish)
    return std::unexpected(std::move(finish.error()));
  item->m_swingFinish = *finish * std::numbers::pi / 180;
  auto aim = config.getFloat("swingAimFactor", 0.2f);
  if (!aim)
    return std::unexpected(std::move(aim.error()));
  item->m_swingAimFactor = *aim;
  auto blocking = item->instanceValue("blockingEffects", JsonArray()).and_then(jsonToStringSet);
  if (!blocking)
    return std::unexpected(std::move(blocking.error()));
  item->m_blockingEffects = std::move(*blocking);
  auto food = item->instanceValue("foodValue");
  if (!food)
    return std::unexpected(std::move(food.error()));
  if (*food) {
    auto foodValue = food->toFloat();
    if (!foodValue)
      return std::unexpected(std::move(foodValue.error()));
    item->m_foodValue = *foodValue;
    item->m_blockingEffects.add("wellfed");
  }
  auto emitters = item->instanceValue("emitters", JsonArray{"eating"}).and_then(jsonToStringSet);
  if (!emitters)
    return std::unexpected(std::move(emitters.error()));
  item->m_emitters = std::move(*emitters);
  auto emote = item->instanceValue("emote", "eat").and_then([](Json const& value) { return value.toString(); });
  if (!emote)
    return std::unexpected(std::move(emote.error()));
  item->m_emote = std::move(*emote);
  item->m_consuming = false;
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> ConsumableItem::clone() const {
  return std::make_shared<ConsumableItem>(*this);
}

std::expected<List<Drawable>, StarException> ConsumableItem::drawables() const {
  auto drawables = iconDrawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  Drawable::scaleAll(*drawables, 1.0f / TilePixels);
  Drawable::translateAll(*drawables, -handPosition() / TilePixels);
  return drawables;
}

std::expected<void, StarException> ConsumableItem::update(float dt, FireMode fireMode, bool shifting, HashSet<MoveControlType> const& moves) {
  auto updated = SwingableItem::update(dt, fireMode, shifting, moves);
  if (!updated)
    return std::unexpected(std::move(updated.error()));
  auto mode = entityMode();
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  if (*mode == EntityMode::Master) {
    if (m_consuming) {
      auto itemOwner = owner();
      if (!itemOwner)
        return std::unexpected(std::move(itemOwner.error()));
      auto emitted = itemOwner->get().addEffectEmitters(m_emitters);
      if (!emitted)
        return std::unexpected(std::move(emitted.error()));
    }
    if (ready())
      return maybeConsume();
  }
  return {};
}

std::expected<void, StarException> ConsumableItem::fire(FireMode mode, bool shifting, bool edgeTriggered) {
  auto usable = canUse();
  if (!usable)
    return std::unexpected(std::move(usable.error()));
  if (*usable)
    return FireableItem::fire(mode, shifting, edgeTriggered);
  return {};
}

std::expected<void, StarException> ConsumableItem::fireTriggered() {
  auto usable = canUse();
  if (!usable)
    return std::unexpected(std::move(usable.error()));
  if (*usable) {
    auto triggered = triggerEffects();
    if (!triggered)
      return std::unexpected(std::move(triggered.error()));
    return FireableItem::fireTriggered();
  }
  return {};
}

std::expected<void, StarException> ConsumableItem::uninit() {
  auto consumed = maybeConsume();
  if (!consumed)
    return std::unexpected(std::move(consumed.error()));
  return FireableItem::uninit();
}

std::expected<bool, StarException> ConsumableItem::canUse() const {
  if (!count() || m_consuming)
    return false;
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  for (auto const& pair : itemOwner->get().statusController().activeUniqueStatusEffectSummary()) {
    if (m_blockingEffects.contains(pair.first))
      return false;
  }
  return true;
}

std::expected<void, StarException> ConsumableItem::triggerEffects() {
  auto options = instanceValue("effects", JsonArray()).and_then([](Json const& value) { return value.toArray(); });
  if (!options)
    return std::unexpected(std::move(options.error()));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  if (options->size()) {
    auto selected = root().random().randFrom(*options);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    auto option = selected->get().toArray();
    if (!option)
      return std::unexpected(std::move(option.error()));
    List<EphemeralStatusEffect> effects;
    for (auto const& value : *option) {
      auto effect = jsonToEphemeralStatusEffect(value);
      if (!effect)
        return std::unexpected(std::move(effect.error()));
      effects.push_back(std::move(*effect));
    }
    auto added = itemOwner->get().statusController().addEphemeralEffects(effects);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  if (m_foodValue) {
    itemOwner->get().statusController().giveResource("food", *m_foodValue);
    auto percentage = itemOwner->get().statusController().resourcePercentage("food");
    if (!percentage)
      return std::unexpected(std::move(percentage.error()));
    if (*percentage == 1.0f) {
      auto added = itemOwner->get().statusController().addEphemeralEffect(
        EphemeralStatusEffect{.uniqueEffect = UniqueStatusEffect("wellfed"), .duration = {}});
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  }
  if (!m_emote.empty()) {
    auto emote = itemOwner->get().requestEmote(m_emote);
    if (!emote)
      return std::unexpected(std::move(emote.error()));
  }
  m_consuming = true;
  return {};
}

std::expected<void, StarException> ConsumableItem::maybeConsume() {
  if (m_consuming) {
    m_consuming = false;
    auto itemWorld = world();
    if (!itemWorld)
      return std::unexpected(std::move(itemWorld.error()));
    auto itemOwner = owner();
    if (!itemOwner)
      return std::unexpected(std::move(itemOwner.error()));
    auto sent = itemWorld->get().sendEntityMessage(itemOwner->get().entityId(), "recordEvent", {"useItem", JsonObject{{"itemType", name()}}});
    if (!sent)
      return std::unexpected(std::move(sent.error()));
    if (count())
      setCount(count() - 1);
    else
      setCount(0);
  }
  return {};
}

}
