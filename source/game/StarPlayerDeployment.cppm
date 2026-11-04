module;
import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.vector;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.list;
import star.rpc_promise;

import star.lua_components;
import star.lua_animation_component;
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

class RenderCallback;

class PlayerDeployment {
public:
  PlayerDeployment(Root& root, Json const& config);

  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& diskStore);
  [[nodiscard]] std::expected<Json, StarException> diskStore() const;

  [[nodiscard]] std::expected<bool, StarException> canDeploy();
  void setDeploying(bool deploying);
  bool isDeploying() const;
  bool isDeployed() const;

  [[nodiscard]] std::expected<void, StarException> init(Player& player, World& world);
  [[nodiscard]] std::expected<void, StarException> uninit();

  [[nodiscard]] std::expected<void, StarException> teleportOut();
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool localMessage, JsonArray const& args = {});
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  [[nodiscard]] std::expected<void, StarException> collectRenderEvents(RenderCallback& renderCallback, Vec2F const& position);

  [[nodiscard]] std::expected<void, StarException> render(RenderCallback& renderCallback, Vec2F const& position);

  [[nodiscard]] std::expected<void, StarException> renderLightSources(RenderCallback& renderCallback);
private:
  Root& m_root;
  Json m_config;

  bool m_deploying;
  bool m_deployed;
  LuaDeploymentScript m_scriptComponent;
};

}

export module star.player_deployment;

export namespace Star {
using ::Star::RenderCallback;
using ::Star::PlayerDeployment;
}
