import std;
import star.format;
import star.set;
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
import star.lua_root;
import star.drawable;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.anchorable_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;

namespace Star {

extern std::expected<EnumMap<LoungeOrientation>, StarException> const LoungeOrientationNames = EnumMap<LoungeOrientation>::create({
    {LoungeOrientation::None, "none"},
    {LoungeOrientation::Sit, "sit"},
    {LoungeOrientation::Lay, "lay"},
    {LoungeOrientation::Stand, "stand"}});

extern std::expected<EnumMap<LoungeControl>, StarException> const LoungeControlNames = EnumMap<LoungeControl>::create({
    {LoungeControl::Left, "Left"},
    {LoungeControl::Right, "Right"},
    {LoungeControl::Down, "Down"},
    {LoungeControl::Up, "Up"},
    {LoungeControl::Jump, "Jump"},
    {LoungeControl::PrimaryFire, "PrimaryFire"},
    {LoungeControl::AltFire, "AltFire"},
    {LoungeControl::Special1, "Special1"},
    {LoungeControl::Special2, "Special2"},
    {LoungeControl::Special3, "Special3"},
    {LoungeControl::Walk, "Walk"},
});

std::expected<std::shared_ptr<EntityAnchor const>, StarException> LoungeableEntity::anchor(std::size_t anchorPositionIndex) const {
  auto result = loungeAnchor(anchorPositionIndex);
  if (!result)
    return std::unexpected(std::move(result.error()));
  return std::shared_ptr<EntityAnchor const>(std::move(*result));
}

std::expected<void, StarException> LoungeableEntity::loungeControl(std::size_t, LoungeControl) { return {}; }

std::expected<void, StarException> LoungeableEntity::loungeAim(std::size_t, Vec2F const&) { return {}; }

std::expected<Set<EntityId>, StarException> LoungeableEntity::entitiesLoungingIn(std::size_t positionIndex) const {
  auto lounging = entitiesLounging();
  if (!lounging)
    return std::unexpected(std::move(lounging.error()));
  Set<EntityId> loungingInEntities;
  for (auto const& p : *lounging) {
    if (p.second == positionIndex)
      loungingInEntities.add(p.first);
  }
  return loungingInEntities;
}

std::expected<Set<std::pair<EntityId, std::size_t>>, StarException> LoungeableEntity::entitiesLounging() const {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  Set<std::pair<EntityId, std::size_t>> loungingInEntities;
  auto scanned = currentWorld->get().forEachEntity(meta_bound_box().translated(position()),
      [&](std::shared_ptr<Entity> const& entity) -> std::expected<void, StarException> {
        if (auto lounger = as<LoungingEntity>(entity)) {
          if (auto anchorStatus = lounger->lounging_in()) {
            if (anchorStatus->entityId == entityId())
              loungingInEntities.add({entity->entityId(), anchorStatus->positionIndex});
          }
        }
        return {};
      });
  if (!scanned)
    return std::unexpected(std::move(scanned.error()));
  return loungingInEntities;
}

std::expected<bool, StarException> LoungingEntity::inConflictingLoungeAnchor() const {
  if (auto loungeAnchorState = lounging_in()) {
    auto currentWorld = world();
    if (!currentWorld)
      return std::unexpected(std::move(currentWorld.error()));
    if (auto loungeableEntity = currentWorld->get().get<LoungeableEntity>(loungeAnchorState->entityId)) {
      auto entitiesLoungingIn = loungeableEntity->entitiesLoungingIn(loungeAnchorState->positionIndex);
      if (!entitiesLoungingIn)
        return std::unexpected(std::move(entitiesLoungingIn.error()));
      return entitiesLoungingIn->size() > 1 || !entitiesLoungingIn->contains(entityId());
    }
  }
  return false;
}

}
