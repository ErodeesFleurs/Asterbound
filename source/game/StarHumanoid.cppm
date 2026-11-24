module;
import star.format;
import std;
import star.json;
import star.data_stream;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.vector;
import star.audio;
import star.map;
import star.set;

import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;

import star.net_element_containers;
import star.net_element_sync_group;

import star.net_compatibility;
import star.array;
namespace Star {
class Root;
class ItemDatabase;

// Required for render_dummy
class ArmorItem;
class HeadArmor;
class ChestArmor;
class LegsArmor;
class BackArmor;

struct Dance;
struct DanceStep;

template <typename T>
concept humanoid_pose_animator = std::same_as<T, NetworkedAnimator> || std::same_as<T, NetworkedAnimator::Sample>;

// Row order is shared by NPC wire ordinals and humanoid.config timing arrays.
enum class HumanoidEmote {
  Idle, Blabbering, Shouting, Happy, Sad, Neutral, Laugh, Annoyed, Oh, Oooh, Blink, Wink, Eat, Sleep
};

// Keep the distinct native type for modules that overload on HumanoidHand.
enum class HumanoidHand {
  Idle, Blabbering, Shouting, Happy, Sad, Neutral, Laugh, Annoyed, Oh, Oooh, Blink, Wink, Eat, Sleep
};

struct EmoteDescription {
  HumanoidEmote state;
  std::string_view name;
  std::string_view frame;
  bool cyclic;
};
inline constexpr std::array emote_descriptions{
  EmoteDescription{HumanoidEmote::Idle, "Idle", "idle", false},
  EmoteDescription{HumanoidEmote::Blabbering, "Blabbering", "blabber", true},
  EmoteDescription{HumanoidEmote::Shouting, "Shouting", "shout", true},
  EmoteDescription{HumanoidEmote::Happy, "Happy", "happy", false},
  EmoteDescription{HumanoidEmote::Sad, "Sad", "sad", true},
  EmoteDescription{HumanoidEmote::Neutral, "NEUTRAL", "neutral", false},
  EmoteDescription{HumanoidEmote::Laugh, "Laugh", "laugh", true},
  EmoteDescription{HumanoidEmote::Annoyed, "Annoyed", "annoyed", false},
  EmoteDescription{HumanoidEmote::Oh, "Oh", "oh", false},
  EmoteDescription{HumanoidEmote::Oooh, "OOOH", "oooh", false},
  EmoteDescription{HumanoidEmote::Blink, "Blink", "blink", false},
  EmoteDescription{HumanoidEmote::Wink, "Wink", "wink", false},
  EmoteDescription{HumanoidEmote::Eat, "Eat", "eat", true},
  EmoteDescription{HumanoidEmote::Sleep, "Sleep", "sleep", true}
};
inline constexpr std::size_t emote_size = emote_descriptions.size();
static_assert([] {
  for (std::size_t i = 0; i < emote_size; ++i)
    if (static_cast<std::size_t>(emote_descriptions[i].state) != i)
      return false;
  return emote_size == static_cast<std::size_t>(HumanoidEmote::Sleep) + 1;
}());
extern std::expected<EnumMap<HumanoidEmote>, StarException> const humanoid_emote_names;

struct Personality {
  String idle = "idle.1";
  String arm_idle = "idle.1";
  Vec2F head_offset = Vec2F();
  Vec2F arm_offset = Vec2F();
};

[[nodiscard]] std::expected<Personality, StarException> parse_personality_array(Json const& config);

[[nodiscard]] std::expected<std::reference_wrapper<Personality>, StarException> parse_personality(Personality& personality, Json const& config);
[[nodiscard]] std::expected<Personality, StarException> parse_personality(Json const& config);

Json json_from_personality(Personality const& personality);

struct HumanoidIdentity {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, HumanoidIdentity& identity);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, HumanoidIdentity const& identity);
  explicit HumanoidIdentity();
  [[nodiscard]] static std::expected<HumanoidIdentity, StarException> create(Json config);

  [[nodiscard]] std::expected<Json, StarException> to_json() const;

  String name;
  // Must have :idle[1-5], :sit, :duck, :walk[1-8], :run[1-8], :jump[1-4], and
  // :fall[1-4]
  String species;
  Gender gender;

  String hair_group;
  // Must have :normal and :climb
  String hair_type;
  Directives hair_directives;
  Directives body_directives;
  Directives emote_directives;
  String facial_hair_group;
  String facial_hair_type;
  Directives facial_hair_directives;
  String facial_mask_group;
  String facial_mask_type;
  Directives facial_mask_directives;

  Personality personality;
  Vec4B color;

  std::optional<String> image_path;

};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, HumanoidIdentity& identity);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, HumanoidIdentity const& identity);

class Humanoid {
public:
  enum State {
    Idle, // 1 idle frame
    Walk, // 8 walking frames
    Run, // 8 run frames
    Jump, // 4 jump frames
    Fall, // 4 fall frames
    Swim, // 7 swim frames
    SwimIdle, // 2 swim idle frame
    Duck, // 1 ducking frame
    Sit, // 1 sitting frame
    Lay, // 1 laying frame
    state_size
  };
  static std::expected<EnumMap<State>, StarException> const state_names;

  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root, Json const& config);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Root& root, HumanoidIdentity const& identity, JsonObject parameters = JsonObject(), Json config = Json(), std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt);
  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> create(Humanoid const& humanoid);
  Humanoid& operator=(Humanoid const&) = delete;

  struct HumanoidTiming {
    HumanoidTiming() = default;
    [[nodiscard]] static std::expected<HumanoidTiming, StarException> create(Root& root, Json config = {});

    static bool cyclic_state(State state);
    static bool cyclic_emote_state(HumanoidEmote state);

    int state_seq(float timer, State state) const;
    int emote_state_seq(float timer, HumanoidEmote state) const;
    int generic_seq(float timer, float cycle, unsigned frames, bool cyclic) const;

    Array<float, state_size> state_cycle;
    Array<unsigned, state_size> state_frames;

    Array<float, emote_size> emote_cycle;
    Array<unsigned, emote_size> emote_frames;
  };

  [[nodiscard]] std::expected<void, StarException> set_identity(HumanoidIdentity const& identity);
  HumanoidIdentity const& identity() const;

  [[nodiscard]] std::expected<bool, StarException> load_config(Json merger = JsonObject(), std::optional<bool> unchanged_body_hidden = {});
  [[nodiscard]] std::expected<void, StarException> load_animation();
  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameters(JsonObject parameters);

  // All of the image identifiers here are meant to be image *base* names, with
  // a collection of frames specific to each piece.  If an image is set to
  // empty string, it is disabled.
  struct WornAny {
    Directives directives;
    String frameset;
    bool rotate_with_head = false;
    bool bypass_nude = false;
    bool fullbright = false;
    HashMap<String, String> animation_tags = {};
  };

  // Must have :normal, climb
  struct WornHead : WornAny {
    Directives mask_directives;
  };
  // Will have :run, :normal, and :duck
  struct WornChest : WornAny {
    String front_sleeve_frameset;
    String back_sleeve_frameset;
  };
  // Must have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  struct WornLegs : WornAny {};
  // Must have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  struct WornBack : WornAny {};

  using Wearable = std::variant<std::monostate, WornHead, WornChest, WornLegs, WornBack>;

  struct Fashion {
    // 8 vanilla + 12 extra slots
    Array<Wearable, 20> wearables;
    // below 3 are recalculated when rendering updated wearables, null-terminated
    Array<std::uint8_t, 20> worn_heads;
    // chests and leg layering is interchangeable
    Array<std::uint8_t, 20> worn_chests_legs;
    Array<std::uint8_t, 20> worn_backs;
    bool worn_heads_changed = true;
    bool worn_chests_legs_changed = true;
    bool worn_backs_changed = true;
    DirectivesGroup helmet_mask_directives_group;
    bool helmet_masks_changed = true;
  };

  template <typename T>
    requires (std::same_as<T, WornHead> || std::same_as<T, WornChest> || std::same_as<T, WornLegs> || std::same_as<T, WornBack>)
  std::optional<std::reference_wrapper<T const>> get_last_wearable_of_type() const {
    for (std::size_t i = m_fashion->wearables.size(); i != 0; --i) {
      auto const& wearable = m_fashion->wearables[i - 1];
      if (std::holds_alternative<T>(wearable))
        return std::cref(std::get<T>(wearable));
    }
    return std::nullopt;
  }

  [[nodiscard]] std::expected<void, StarException> remove_wearable(std::uint8_t slot);
  [[nodiscard]] std::expected<void, StarException> set_wearable_from_head(std::uint8_t slot, HeadArmor const& head, Gender gender);
  [[nodiscard]] std::expected<void, StarException> set_wearable_from_chest(std::uint8_t slot, ChestArmor const& chest, Gender gender);
  [[nodiscard]] std::expected<void, StarException> set_wearable_from_legs(std::uint8_t slot, LegsArmor const& legs, Gender gender);
  [[nodiscard]] std::expected<void, StarException> set_wearable_from_back(std::uint8_t slot, BackArmor const& back, Gender gender);

  // Legacy getters for all of the above, returns last found
  Directives const& head_armor_directives() const;
  String const& head_armor_frameset() const;
  Directives const& chest_armor_directives() const;
  String const& chest_armor_frameset() const;
  String const& back_sleeve_frameset() const;
  String const& front_sleeve_frameset() const;
  Directives const& legs_armor_directives() const;
  String const& legs_armor_frameset() const;
  Directives const& back_armor_directives() const;
  String const& back_armor_frameset() const;


  [[nodiscard]] std::expected<void, StarException> set_state(State state);
  [[nodiscard]] std::expected<void, StarException> set_emote_state(HumanoidEmote state);
  [[nodiscard]] std::expected<void, StarException> set_dance(std::optional<String> const& name);
  void set_facing_direction(Direction facing_direction);
  [[nodiscard]] std::expected<void, StarException> set_moving_backwards(bool backwards);
  void set_head_rotation(float head_rotation);
  void set_rotation(float rotation);
  void set_scale(Vec2F scale);

  void set_vapor_trail(bool enabled);

  State state() const;
  HumanoidEmote emote_state() const;
  std::optional<String> const& dance() const;
  [[nodiscard]] std::expected<bool, StarException> dance_cyclic_or_ended() const;
  Direction facing_direction() const;
  bool moving_backwards() const;

  // If not rotating, then the arms follow normal movement animation.  The
  // angle parameter should be in the range [-pi/2, pi/2] (the facing direction
  // should not be included in the angle).
  void set_hand_parameters(ToolHand hand, bool holding_item, float angle, float item_angle, bool two_handed,
      bool recoil, bool outside_of_hand);
  void set_hand_frame_overrides(ToolHand hand, StringView back, StringView front);
  void set_hand_drawables(ToolHand hand, List<Drawable> drawables);
  void set_hand_non_rotated_drawables(ToolHand hand, List<Drawable> drawables);
  bool hand_holding_item(ToolHand hand) const;

  // Updates the animation based on whatever the current animation state is,
  // wrapping or clamping animation time as appropriate.
  [[nodiscard]] std::expected<void, StarException> animate(float dt, std::optional<std::reference_wrapper<NetworkedAnimator::DynamicTarget>> dynamic_target);

  // Logical pose is committed by ticks; visual queries use an explicit sampled view.
  [[nodiscard]] std::expected<void, StarException> prepare_animator_pose();
  [[nodiscard]] std::expected<void, StarException> prepare_animator_pose(NetworkedAnimator::Sample& sample, bool with_items = true);

  // Reset animation time to 0.0f
  [[nodiscard]] std::expected<void, StarException> reset_animation();

  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in world space, not pixels)
  [[nodiscard]] std::expected<List<Drawable>, StarException> render(bool with_items = true, bool with_rotation_and_scale = true, float interpolation = 1.f);
  [[nodiscard]] std::expected<List<Drawable>, StarException> render(NetworkedAnimator::Sample& sample, bool with_items = true, bool with_rotation_and_scale = true);

  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in pixels, not world space)
  [[nodiscard]] std::expected<List<Drawable>, StarException> render_portrait(PortraitMode mode) const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> render_skull() const;

  [[nodiscard]] static std::expected<std::shared_ptr<Humanoid>, StarException> make_dummy(Root& root, Gender gender);
  // Renders to centered drawables (centered on the normal image center for the
  // player graphics), (in pixels, not world space)
  [[nodiscard]] std::expected<List<Drawable>, StarException> render_dummy(Gender gender, std::optional<std::reference_wrapper<HeadArmor const>> head = std::nullopt, std::optional<std::reference_wrapper<ChestArmor const>> chest = std::nullopt,
      std::optional<std::reference_wrapper<LegsArmor const>> legs = std::nullopt, std::optional<std::reference_wrapper<BackArmor const>> back = std::nullopt);

  [[nodiscard]] std::expected<Vec2F, StarException> hand_position(ToolHand hand, Vec2F const& offset) const;

  // Finds the arm position in world space if the humanoid was facing the given
  // direction and applying the given arm angle.  The offset given is from the
  // rotation center of the arm.
  [[nodiscard]] std::expected<Vec2F, StarException> arm_position(ToolHand hand, Direction facing_direction, float arm_angle, Vec2F const& offset) const;

  // Gives the offset of the hand from the arm rotation center
  [[nodiscard]] std::expected<Vec2F, StarException> hand_offset(ToolHand hand, Direction facing_direction) const;

  [[nodiscard]] std::expected<Vec2F, StarException> arm_adjustment() const;

  [[nodiscard]] std::expected<Vec2F, StarException> mouth_offset(bool ignore_adjustments = false) const;
  [[nodiscard]] std::expected<float, StarException> bob_y_offset(float interpolation = 1.f) const;
  [[nodiscard]] std::expected<Vec2F, StarException> feet_offset() const;

  [[nodiscard]] std::expected<Vec2F, StarException> head_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> chest_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> legs_armor_offset() const;
  [[nodiscard]] std::expected<Vec2F, StarException> back_armor_offset() const;

  String default_death_particles() const;
  [[nodiscard]] std::expected<List<Particle>, StarException> particles(String const& name) const;

  Json const& default_movement_parameters() const;
  std::optional<Json> const& player_movement_parameters() const;

  String get_head_from_identity() const;
  String get_body_from_identity() const;
  String get_body_mask_from_identity() const;
  String get_body_head_mask_from_identity() const;
  String get_facial_emotes_from_identity() const;
  String get_hair_from_identity() const;
  String get_facial_hair_from_identity() const;
  String get_facial_mask_from_identity() const;
  String get_back_arm_from_identity() const;
  String get_front_arm_from_identity() const;
  String get_vapor_trail_frameset() const;

  std::optional<std::reference_wrapper<NetworkedAnimator>> networked_animator();
  std::optional<std::reference_wrapper<NetworkedAnimator const>> networked_animator() const;
  List<String> animation_scripts() const;

  [[nodiscard]] std::expected<Json, StarException> humanoid_config(bool with_overrides = true);

  // Extracts scalenearest from directives and returns the combined scale and
  // a new Directives without those scalenearest directives.
  [[nodiscard]] static std::expected<std::pair<Vec2F, Directives>, StarException> extract_scale_from_directives(Directives const& directives);

private:
  explicit Humanoid(Root& root);
  Humanoid(Humanoid const&) = default;
  Root& m_root;
  String m_gender_name = "male";
  struct HandDrawingInfo {
    List<Drawable> item_drawables;
    List<Drawable> non_rotated_drawables;
    bool holding_item = false;
    float angle = 0.0f;
    float item_angle = 0.0f;
    String back_frame;
    String front_frame;
    Directives back_directives;
    Directives front_directives;
    float frame_angle_adjust = 0.0f;
    bool recoil = false;
    bool outside_of_hand = false;
  };

  HandDrawingInfo const& get_hand(ToolHand hand) const;

  void wearable_removed(Wearable const& wearable);

  [[nodiscard]] std::expected<std::string_view, StarException> frame_base(State state) const;

  Directives const& get_body_directives() const;
  Directives const& get_hair_directives() const;
  Directives const& get_emote_directives() const;
  Directives const& get_facial_hair_directives() const;
  Directives const& get_facial_mask_directives() const;
  DirectivesGroup const& get_helmet_mask_directives_group() const;

  [[nodiscard]] std::expected<int, StarException> arm_frame_for(int base_frame) const;
  int body_frame_for(int base_frame) const;
  [[nodiscard]] std::expected<float, StarException> bob_for_body_frame(int body_frame) const;

  struct CachedDance {
    std::shared_ptr<Dance const> definition;
    std::uint64_t generation = 0;
  };
  [[nodiscard]] std::expected<CachedDance, StarException> load_dance(String const& name) const;
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<Dance const>>, StarException> current_dance() const;
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<DanceStep const>>, StarException> sample_dance_step(float interpolation) const;
  [[nodiscard]] std::expected<std::optional<std::reference_wrapper<DanceStep const>>, StarException> dance_step_at(float timer) const;

  template <humanoid_pose_animator Animator>
  void refresh_wearables(Fashion& fashion, Animator& animator) const;
  Vec2F base_head_offset() const;
  template <humanoid_pose_animator Animator>
  [[nodiscard]] std::expected<void, StarException> apply_animator_pose(Animator& animator, HandDrawingInfo const& front_hand,
      HandDrawingInfo const& back_hand, float front_angle, float back_angle, float head_rotation,
      std::optional<std::reference_wrapper<DanceStep const>> dance_step, bool with_items);

  template <humanoid_pose_animator Animator>
  [[nodiscard]] std::expected<void, StarException> prepare_animator_pose(Animator& animator, float interpolation, bool with_items);

  struct Config {
    HumanoidTiming timing;
    Vec2F global_offset;
    Vec2F head_run_offset;
    Vec2F head_swim_offset;
    Vec2F head_duck_offset;
    Vec2F head_sit_offset;
    Vec2F head_lay_offset;
    float run_fall_offset = 0.0f;
    float duck_offset = 0.0f;
    float sit_offset = 0.0f;
    float lay_offset = 0.0f;
    Vec2F recoil_offset;
    Vec2F mouth_offset;
    Vec2F feet_offset;

    Vec2F head_armor_offset;
    Vec2F chest_armor_offset;
    Vec2F legs_armor_offset;
    Vec2F back_armor_offset;

    bool use_body_mask = false;
    bool use_body_head_mask = false;

    bool body_hidden = false;

    List<int> arm_walk_seq;
    List<int> arm_run_seq;
    List<float> walk_bob;
    List<float> run_bob;
    List<float> swim_bob;
    float jump_bob = 0.0f;
    Vec2F front_arm_rotation_center;
    Vec2F back_arm_rotation_center;
    Vec2F front_hand_position;
    Vec2F back_arm_offset;

    Vec2F head_rotation_center;
    bool body_fullbright = false;
    unsigned vapor_trail_frames = 0;
    float vapor_trail_cycle = 0.0f;
    Json particle_emitters;
    String default_death_particles;
    Json default_movement_parameters;
    std::optional<Json> player_movement_parameters;
  };

  [[nodiscard]] std::expected<bool, StarException> apply_config(Json const& config);
  Config m_config;
  Json m_base_config;
  Json m_merge_config;

  String m_head_frameset;
  String m_body_frameset;
  String m_body_mask_frameset;
  String m_body_head_mask_frameset;
  String m_back_arm_frameset;
  String m_front_arm_frameset;
  String m_emote_frameset;
  String m_hair_frameset;
  String m_facial_hair_frameset;
  String m_facial_mask_frameset;


  String m_vapor_trail_frameset;

  std::shared_ptr<Fashion> m_fashion;

  State m_state;
  HumanoidEmote m_emote_state;
  std::optional<String> m_dance;
  mutable CachedDance m_cached_dance;
  Direction m_facing_direction;
  bool m_moving_backwards = false;
  float m_head_rotation = 0.0f;
  float m_head_rotation_target = 0.0f;
  float m_rotation = 0.0f;
  Vec2F m_scale;
  bool m_draw_vapor_trail = false;

  HandDrawingInfo m_primary_hand;
  HandDrawingInfo m_alt_hand;

  bool m_two_handed = false;

  HumanoidIdentity m_identity;

  float m_animation_timer = 0.0f;
  float m_emote_animation_timer = 0.0f;
  float m_dance_timer = 0;
  float m_previous_animation_timer = 0;
  float m_previous_emote_animation_timer = 0;
  float m_previous_dance_timer = 0;
  struct PresentationPose {
    float head_rotation = 0;
    float rotation = 0;
    Vec2F scale = Vec2F::filled(1.f);
    float primary_angle = 0;
    float primary_item_angle = 0;
    float alt_angle = 0;
    float alt_item_angle = 0;
    bool primary_holding = false;
    bool alt_holding = false;
    Direction facing = Direction::Left;
    bool two_handed = false;
  };
  PresentationPose m_previous_pose;
  PresentationPose m_current_pose;
  bool m_presentation_history = false;

  bool m_use_animation = false;

  std::shared_ptr<NetworkedAnimator> m_networked_animator;

  List<String> m_animation_scripts;

  struct AnimationStateArgs {
    String state;
    bool start_new;
    bool reverse;
  };
  using AnimationGroup = HashMap<String, AnimationStateArgs>;
  using AnimationGroupRef = std::optional<std::reference_wrapper<AnimationGroup const>>;
  AnimationGroupRef movement_animation_group(State state, bool backwards) const;
  template <humanoid_pose_animator Animator>
  [[nodiscard]] static std::expected<void, StarException> apply_animation_group(Animator& animator, AnimationGroupRef group, bool restart);
  HashMap<Humanoid::State, HashMap<String,AnimationStateArgs>> m_animation_states;
  HashMap<Humanoid::State, HashMap<String,AnimationStateArgs>> m_animation_states_backwards;
  HashMap<HumanoidEmote, HashMap<String,AnimationStateArgs>> m_emote_animation_states;
  HashMap<PortraitMode, HashMap<String,AnimationStateArgs>> m_portrait_animation_states;

  HashMap<String, String> m_identity_frameset_tags;

  std::pair<String, String> m_head_rotation_point;
  std::pair<String, String> m_front_arm_rotation_point;
  std::pair<String, String> m_back_arm_rotation_point;

  String m_front_item_part;
  String m_back_item_part;

  std::pair<String, String> m_mouth_offset_point;
  std::pair<String, String> m_head_armor_offset_point;
  std::pair<String, String> m_chest_armor_offset_point;
  std::pair<String, String> m_legs_armor_offset_point;
  std::pair<String, String> m_back_armor_offset_point;
  std::pair<String, String> m_feet_offset_point;
  std::pair<String, String> m_throw_point;
  std::pair<String, String> m_interact_point;

};


// this is because species can be changed on the fly and therefore the humanoid needs to re-initialize as the new species when it changes
// therefore we need to have these in a dynamic group in players and NPCs for the sake of the networked animator not breaking the game
class NetHumanoid : public NetElementSyncGroup {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<NetHumanoid>, StarException> create(Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<NetHumanoid>, StarException> create(Root& root, HumanoidIdentity identity, JsonObject parameters = JsonObject(), Json config = Json());

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool full) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  std::shared_ptr<Humanoid> humanoid();
  [[nodiscard]] std::expected<void, StarException> set_humanoid_parameters(JsonObject parameters);
  JsonObject humanoid_parameters();

private:
  explicit NetHumanoid(Root& root);
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> setup_net_elements();

  Json m_config;
  NetElementHashMap<String,Json> m_humanoid_parameters;
  std::shared_ptr<Humanoid> m_humanoid;
};


}

export module star.humanoid;

export namespace Star {
  using ::Star::ArmorItem;
  using ::Star::HeadArmor;
  using ::Star::ChestArmor;
  using ::Star::LegsArmor;
  using ::Star::BackArmor;
  using ::Star::Humanoid;
  using ::Star::humanoid_pose_animator;
  using ::Star::Dance;
  using ::Star::HumanoidEmote;
  using ::Star::EmoteDescription;
  using ::Star::emote_descriptions;
  using ::Star::humanoid_emote_names;
  using ::Star::emote_size;
  using ::Star::HumanoidHand;
  using ::Star::Personality;
  using ::Star::parse_personality_array;
  using ::Star::parse_personality;
  using ::Star::json_from_personality;
  using ::Star::HumanoidIdentity;
  using ::Star::NetHumanoid;
}
