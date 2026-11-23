module;
import star.format;
import std;
import star.lua;
import star.json;
import star.id_map;
import star.data_stream;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.game_types;
import star.message_types;
import star.strong_typedef;
import star.bi_map;
import star.rpc_promise;

import star.uuid;
import star.drawable;
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
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;

namespace Star {

class Root;
class Player;

class Companion {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<Companion>, StarException> create(Root& root, Json const& json);
  Json toJson() const;

  [[nodiscard]] std::expected<Uuid, StarException> podUuid() const;
  [[nodiscard]] std::expected<std::optional<String>, StarException> name() const;
  [[nodiscard]] std::expected<std::optional<String>, StarException> description() const;

  List<Drawable> portrait() const;

  [[nodiscard]] std::expected<std::optional<float>, StarException> resource(String const& resourceName) const;
  [[nodiscard]] std::expected<std::optional<float>, StarException> resourceMax(String const& resourceName) const;

  [[nodiscard]] std::expected<std::optional<float>, StarException> stat(String const& statName) const;

private:
  explicit Companion(Json json) : m_json(std::move(json)) {}
  [[nodiscard]] std::expected<std::optional<float>, StarException> statusValue(String const& group, String const& name) const;
  Json m_json;
  List<Drawable> m_portrait;
};

class PlayerCompanions {
public:
  PlayerCompanions(Root& root, Json const& config);

  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& diskStore);
  [[nodiscard]] std::expected<Json, StarException> diskStore() const;

  List<std::shared_ptr<Companion>> getCompanions(String const& category) const;

  [[nodiscard]] std::expected<void, StarException> init(Player& player, World& world);
  [[nodiscard]] std::expected<void, StarException> uninit();

  [[nodiscard]] std::expected<void, StarException> dismissCompanion(String const& category, Uuid const& podUuid);

  [[nodiscard]] MessageResponseResult receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});
  [[nodiscard]] std::expected<void, StarException> update(float dt);

private:
  Root& m_root;
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeCompanionsCallbacks();

  Json m_config;
  StringMap<List<std::shared_ptr<Companion>>> m_companions;

  LuaStoredEntityScript
      m_scriptComponent;
};

}

export module star.player_companions;

export namespace Star {
using ::Star::Companion;
using ::Star::PlayerCompanions;
}
