module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.vector;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.rpc_promise;
import star.a_star;
import star.set;
import star.ordered_map;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.version;


import star.item;
import star.drawable;
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
import star.previewable_item;

import star.scripted_entity;
import star.status_effect_entity;
import star.effect_emitter;
import star.projectile;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

export module star.thrown_item;
import star.projectile_database;

export namespace Star {

class ThrownItem : public Item, public SwingableItem, public PreviewableItem {
public:
  void const* query_type(TypeKey key) const noexcept override {
    return query_type_of<ThrownItem, Item, SwingableItem, PreviewableItem>(this, key);
  }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<ThrownItem>(); }

  [[nodiscard]]
  static std::expected<std::shared_ptr<ThrownItem>, StarException> create(Root& root, Json const& config, String const& directory, Json const& itemParameters = JsonObject());

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;
  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

protected:
  [[nodiscard]]
  std::expected<void, StarException> fireTriggered() override;

private:
  explicit ThrownItem(Root& root) : Item(root) {}
  String m_projectileType;
  Json m_projectileConfig;
  std::size_t m_ammoUsage;
  List<Drawable> m_drawables;
};

}

namespace Star {

std::expected<std::shared_ptr<ThrownItem>, StarException> ThrownItem::create(Root& root, Json const& config, String const& directory, Json const& itemParameters) {
  auto item = std::shared_ptr<ThrownItem>(new ThrownItem(root));
  auto initialized = item->initialize(config, directory, itemParameters);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = item->initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  auto projectileType = item->instanceValue("projectileType").and_then([](Json const& value) { return value.toString(); });
  if (!projectileType)
    return std::unexpected(std::move(projectileType.error()));
  item->m_projectileType = std::move(*projectileType);
  auto projectileConfig = item->instanceValue("projectileConfig", {});
  if (!projectileConfig)
    return std::unexpected(std::move(projectileConfig.error()));
  item->m_projectileConfig = std::move(*projectileConfig);
  auto ammoUsage = item->instanceValue("ammoUsage", 1).and_then([](Json const& value) { return value.toUInt(); });
  if (!ammoUsage)
    return std::unexpected(std::move(ammoUsage.error()));
  item->m_ammoUsage = *ammoUsage;
  auto image = item->instanceValue("image").and_then([](Json const& value) { return value.toString(); });
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto metadata = root.imageMetadataDatabase();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto drawable =
    Drawable::makeImage(**metadata, AssetPath::relative_to(directory, *image), 1.0f / TilePixels, true, Vec2F());
  if (!drawable)
    return std::unexpected(std::move(drawable.error()));
  item->m_drawables = {std::move(*drawable)};
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> ThrownItem::clone() const {
  return std::make_shared<ThrownItem>(*this);
}

std::expected<List<Drawable>, StarException> ThrownItem::drawables() const {
  return m_drawables;
}

std::expected<List<Drawable>, StarException> ThrownItem::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const&) const {
  return iconDrawables();
}

std::expected<void, StarException> ThrownItem::fireTriggered() {
  if (!initialized())
    return std::unexpected(ItemException("Thrown item not init'd properly, or user not recognized as Tool User."));
  auto itemOwner = owner();
  if (!itemOwner)
    return std::unexpected(std::move(itemOwner.error()));
  auto itemWorld = world();
  if (!itemWorld)
    return std::unexpected(std::move(itemWorld.error()));
  auto aimPosition = itemOwner->get().aimPosition();
  if (!aimPosition)
    return std::unexpected(std::move(aimPosition.error()));
  Vec2F direction = itemWorld->get().geometry().diff(*aimPosition, itemOwner->get().position()).normalized();
  auto relativeFirePosition = ownerFirePosition();
  if (!relativeFirePosition)
    return std::unexpected(std::move(relativeFirePosition.error()));
  Vec2F firePosition = itemOwner->get().position() + *relativeFirePosition;
  auto collision = itemWorld->get().lineTileCollision(itemOwner->get().position(), firePosition);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  if (*collision)
    return {};
  if (consume(m_ammoUsage)) {
    auto database = root().projectileDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto projectile = (*database)->createProjectile(m_projectileType, m_projectileConfig);
    if (!projectile)
      return std::unexpected(std::move(projectile.error()));
    (*projectile)->setInitialPosition(firePosition);
    (*projectile)->setInitialDirection(direction);
    (*projectile)->setSourceEntity(itemOwner->get().entityId(), false);
    auto multiplier = itemOwner->get().powerMultiplier();
    if (!multiplier)
      return std::unexpected(std::move(multiplier.error()));
    (*projectile)->setPowerMultiplier(*multiplier);
    auto added = itemWorld->get().addEntity(*projectile);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  return FireableItem::fireTriggered();
}

}
