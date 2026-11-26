import star.json;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.material_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;
import star.data_stream_extra;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.string;

import star.lua_root;
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
import star.tile_modification;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.liquids_database;

namespace Star {

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, MaterialSpace const& materialSpace) {
  return ds << materialSpace.space << materialSpace.material;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, MaterialSpace& materialSpace) {
  MaterialSpace decoded;
  auto read = ds >> decoded.space >> decoded.material;
  if (!read)
    return std::unexpected(std::move(read.error()));
  materialSpace = decoded;
  return std::ref(ds);
}

TileEntity::TileEntity() {
  setPersistent(true);
}

Vec2F TileEntity::position() const {
  return Vec2F(tilePosition());
}

List<Vec2I> TileEntity::spaces() const {
  return {};
}

List<Vec2I> TileEntity::roots() const {
  return {};
}

List<MaterialSpace> TileEntity::materialSpaces() const {
  return {};
}

std::expected<bool, StarException> TileEntity::damageTiles(List<Vec2I> const&, Vec2F const&, TileDamage const&) {
  return false;
}

std::expected<bool, StarException> TileEntity::canBeDamaged() const {
  return true;
}

bool TileEntity::is_interactive() const {
  return false;
}

List<Vec2I> TileEntity::interactiveSpaces() const {
  return spaces();
}

std::expected<InteractAction, StarException> TileEntity::interact(InteractRequest const& request) {
  ((void)(request));
  return InteractAction();
}

std::expected<List<QuestArcDescriptor>, StarException> TileEntity::offered_quests() const {
  return List<QuestArcDescriptor>{};
}

std::expected<StringSet, StarException> TileEntity::turn_in_quests() const {
  return StringSet();
}

std::expected<Vec2F, StarException> TileEntity::quest_indicator_position() const {
  return position();
}

std::expected<bool, StarException> TileEntity::anySpacesOccupied(List<Vec2I> const& spaces) const {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  Vec2I tp = tilePosition();
  for (auto pos : spaces) {
    pos += tp;
    if (is_connectable_material(currentWorld->get().material(pos, TileLayer::Foreground)))
      return true;
  }

  return false;
}

std::expected<bool, StarException> TileEntity::allSpacesOccupied(List<Vec2I> const& spaces) const {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  Vec2I tp = tilePosition();
  for (auto pos : spaces) {
    pos += tp;
    if (!is_connectable_material(currentWorld->get().material(pos, TileLayer::Foreground)))
      return false;
  }

  return true;
}

std::expected<float, StarException> TileEntity::spacesLiquidFillLevel(List<Vec2I> const& relativeSpaces) const {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  float total = 0.0f;
  for (auto pos : relativeSpaces) {
    pos += tilePosition();
    total += currentWorld->get().liquidLevel(pos).level;
  }
  return total / relativeSpaces.size();
}

}
