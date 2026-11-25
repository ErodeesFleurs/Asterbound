module;
import std;
import star.format;
import star.json;
import star.net_compatibility;
import star.byte_array;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.string;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.vector;
import star.audio;
import star.thread;
import star.list;
import star.map;
import star.set;
import star.color;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;


import star.item_descriptor;
import star.drawable;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.animation;
import star.particle;

import star.entity_splash;

namespace Star {

class Root;

class Rebuilder;
class Player;

struct PlayerExceptionTag {
  static constexpr std::string_view name() { return "PlayerException"; }
};
using PlayerException = StarError<PlayerExceptionTag, StarException>;

// The player has a large number of shared config states, so this is a shared
// config object to hold them.
struct PlayerConfig {
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerConfig>, StarException> create(Root& root, JsonObject const& cfg);

  HumanoidIdentity defaultIdentity;
  Humanoid::HumanoidTiming humanoidTiming;

  List<ItemDescriptor> defaultItems;
  List<ItemDescriptor> defaultBlueprints;

  RectF meta_bound_box;

  Json movementParameters;
  Json zeroGMovementParameters;
  Json statusControllerSettings;

  float footstepTiming;
  Vec2F footstepSensor;

  Vec2F underwaterSensor;
  float underwaterMinWaterLevel;

  String effectsAnimator;

  float teleportInTime;
  float teleportOutTime;

  float deployInTime;
  float deployOutTime;

  String bodyMaterialKind;

  EntitySplashConfig splash_config;

  Json companionsConfig;

  Json deploymentConfig;

  StringMap<String> genericScriptContexts;
private:
  explicit PlayerConfig(Humanoid::HumanoidTiming timing) : humanoidTiming(std::move(timing)) {}
};

class PlayerFactory {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<PlayerFactory>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<Player>, StarException> create() const;
  [[nodiscard]] std::expected<std::shared_ptr<Player>, StarException> diskLoadPlayer(Json const& diskStore) const;
  [[nodiscard]] std::expected<std::shared_ptr<Player>, StarException> net_load_player(ByteArray const& net_store, NetCompatibilityRules rules = {}) const;

private:
  explicit PlayerFactory(Root& root);
  Root& m_root;
  std::shared_ptr<PlayerConfig> m_config;

  std::shared_ptr<Rebuilder> m_rebuilder;
};

}

export module star.player_factory;

export namespace Star {
  using ::Star::Rebuilder;
  using ::Star::Player;
  using ::Star::PlayerConfig;
  using ::Star::PlayerFactory;
  using ::Star::PlayerExceptionTag;
  using ::Star::PlayerException;
}
