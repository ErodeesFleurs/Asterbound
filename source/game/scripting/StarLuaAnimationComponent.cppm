module;
import std;
import star.json_extra;
import star.json;
import star.vector;
import star.string;
import star.color;
import star.list;
import star.lua;
import star.format;
import star.lua_converters;
import star.lua_game_converters;
import star.lua_components;
import star.world;
import star.game_types;
import star.rpc_promise;
import star.drawable;
import star.entity_rendering_types;
import star.mixer;
import star.particle;
import star.assets;
import star.root;
import star.particle_database;
import star.light_source;
import star.algorithm;

namespace Star {

struct LuaAnimationComponentExceptionTag {
  static constexpr std::string_view name() { return "LuaAnimationComponentException"; }
};
using LuaAnimationComponentException = StarError<LuaAnimationComponentExceptionTag, LuaComponentException>;


// Local animation output is a value owned by its script host, not a context base.
class LuaScriptAnimation {
public:
  explicit LuaScriptAnimation(LuaBaseComponent& host) : m_host(host) {}
  LuaScriptAnimation(LuaScriptAnimation const&) = delete;
  LuaScriptAnimation& operator=(LuaScriptAnimation const&) = delete;
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& drawables();
  List<LightSource> const& light_sources();
  List<Particle> pull_new_particles();
  List<std::shared_ptr<AudioInstance>> pull_new_audios();
  [[nodiscard]] std::expected<void, StarException> context_setup();
  void context_shutdown();
private:
  LuaBaseComponent& m_host;
  List<Particle> m_pendingParticles;
  List<std::shared_ptr<AudioInstance>> m_pendingAudios;
  List<std::shared_ptr<AudioInstance>> m_activeAudio;
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> m_drawables;
  List<LightSource> m_lightSources;
};

[[nodiscard]] inline std::expected<void, StarException> LuaScriptAnimation::context_setup() {
  if (m_host.has_callbacks("localAnimator"))
    return {};
  LuaCallbacks callbacks;
  if (auto result = callbacks.register_callback("playAudio", [&self = *this](String const& sound, std::optional<int> loops, std::optional<float> volume) -> std::expected<std::shared_ptr<AudioInstance>, StarException> {
    auto assets = self.m_host.root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto source = (*assets)->audio(sound);
    if (!source)
      return std::unexpected(std::move(source.error()));
    auto audio = AudioInstance::create(**source);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    (*audio)->set_loops(loops.value_or(0));
    (*audio)->set_volume(volume.value_or(1.0f));
    self.m_pendingAudios.push_back(*audio);
    self.m_activeAudio.push_back(*audio);
    return audio;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("spawnParticle", [&self = *this](Json const& config, std::optional<Vec2F> const& position) -> std::expected<void, StarException> {
    auto database = self.m_host.root().particleDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto particle = (*database)->particle(config);
    if (!particle)
      return std::unexpected(std::move(particle.error()));
    particle->translate(position.value_or(Vec2F{}));
    self.m_pendingParticles.push_back(std::move(*particle));
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("clearDrawables", [&self = *this]() { self.m_drawables.clear(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("addDrawable", [&self = *this](Drawable drawable, std::optional<String> layerName) -> std::expected<void, StarException> {
    std::optional<EntityRenderLayer> layer;
    if (layerName) {
      auto parsed = parseRenderLayer(*layerName);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      layer = *parsed;
    }
    if (std::holds_alternative<Drawable::ImagePart>(drawable.part))
      std::get<Drawable::ImagePart>(drawable.part).transformation.scale(0.125f);
    self.m_drawables.push_back({std::move(drawable), layer});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("addJsonDrawable", [&self = *this](Json config, std::optional<String> layerName) -> std::expected<void, StarException> {
    auto metadata = self.m_host.root().imageMetadataDatabase();
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    auto drawable = Drawable::create(**metadata, config);
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    std::optional<EntityRenderLayer> layer;
    if (layerName) {
      auto parsed = parseRenderLayer(*layerName);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      layer = *parsed;
    }
    if (std::holds_alternative<Drawable::ImagePart>(drawable->part))
      std::get<Drawable::ImagePart>(drawable->part).transformation.scale(0.125f);
    self.m_drawables.push_back({std::move(*drawable), layer});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("clearLightSources", [&self = *this]() { self.m_lightSources.clear(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.register_callback("addLightSource", [&self = *this](LuaTable const& table) -> std::expected<void, StarException> {
    auto position = table.get<Vec2F>("position");
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto color = table.get<Color>("color");
    if (!color)
      return std::unexpected(std::move(color.error()));
    auto point = table.get<std::optional<bool>>("pointLight");
    if (!point)
      return std::unexpected(std::move(point.error()));
    auto beam = table.get<std::optional<float>>("pointBeam");
    if (!beam)
      return std::unexpected(std::move(beam.error()));
    auto angle = table.get<std::optional<float>>("beamAngle");
    if (!angle)
      return std::unexpected(std::move(angle.error()));
    auto ambience = table.get<std::optional<float>>("beamAmbience");
    if (!ambience)
      return std::unexpected(std::move(ambience.error()));
    self.m_lightSources.push_back({.position = *position,
                                   .color = color->to_rgb_f(),
                                   .type = static_cast<LightType>(point->value_or(false)),
                                   .pointBeam = beam->value_or(float{}),
                                   .beamAngle = angle->value_or(float{}),
                                   .beamAmbience = ambience->value_or(float{})});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  return m_host.add_callbacks("localAnimator", std::move(callbacks));
}

inline List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& LuaScriptAnimation::drawables() { return m_drawables; }

inline List<LightSource> const& LuaScriptAnimation::light_sources() { return m_lightSources; }

inline List<Particle> LuaScriptAnimation::pull_new_particles() { return std::exchange(m_pendingParticles, {}); }

inline List<std::shared_ptr<AudioInstance>> LuaScriptAnimation::pull_new_audios() {
  m_activeAudio.filter([](std::shared_ptr<AudioInstance> const& audio) { return !audio->finished(); });
  return std::exchange(m_pendingAudios, {});
}

inline void LuaScriptAnimation::context_shutdown() {
  for (auto const& audio : m_activeAudio)
    audio->set_loops(0);
  m_activeAudio.clear();
}

// Animation-only contexts do not carry message handlers or persistent storage.
class LuaAnimationScript final : public LuaBaseComponent {
public:
  explicit LuaAnimationScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_animation(*this) {}
  ~LuaAnimationScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned update_delta() const { return m_update.update_delta(); }
  float update_dt(float dt) const { return m_update.update_dt(dt); }
  float update_dt() const { return m_update.update_dt(); }
  void set_update_delta(unsigned delta) { m_update.set_update_delta(delta); }
  bool update_ready() const { return m_update.update_ready(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> thread_pass_message(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.thread_pass_message(thread, message, args);
  }
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& drawables() { return m_animation.drawables(); }
  List<LightSource> const& light_sources() { return m_animation.light_sources(); }
  List<Particle> pull_new_particles() { return m_animation.pull_new_particles(); }
  List<std::shared_ptr<AudioInstance>> pull_new_audios() { return m_animation.pull_new_audios(); }
protected:
  using LuaBaseComponent::set_lua_root;
  [[nodiscard]] std::expected<void, StarException> context_setup() override {
    if (auto result = LuaBaseComponent::context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_animation.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> context_shutdown() override {
    m_animation.context_shutdown();
    auto stopped = m_update.context_shutdown();
    return LuaScriptUpdate::finish_shutdown(std::move(stopped), LuaBaseComponent::context_shutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_threads_callbacks() override {
    auto callbacks = LuaBaseComponent::make_threads_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.add_threads_callbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptAnimation m_animation;
};

// Deployment scripts combine persistent entity behavior with local animation.
class LuaDeploymentScript final : public LuaBaseComponent {
public:
  explicit LuaDeploymentScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_storage(*this), m_messages(*this), m_animation(*this) {}
  ~LuaDeploymentScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned update_delta() const { return m_update.update_delta(); }
  float update_dt(float dt) const { return m_update.update_dt(dt); }
  float update_dt() const { return m_update.update_dt(); }
  void set_update_delta(unsigned delta) { m_update.set_update_delta(delta); }
  bool update_ready() const { return m_update.update_ready(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> thread_pass_message(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.thread_pass_message(thread, message, args);
  }
  [[nodiscard]] std::expected<JsonObject, StarException> get_script_storage() const { return m_storage.get_script_storage(); }
  [[nodiscard]] std::expected<void, StarException> set_script_storage(JsonObject storage) { return m_storage.set_script_storage(std::move(storage)); }
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handle_message(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handle_message(message, localMessage, args);
  }
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& drawables() { return m_animation.drawables(); }
  List<LightSource> const& light_sources() { return m_animation.light_sources(); }
  List<Particle> pull_new_particles() { return m_animation.pull_new_particles(); }
  List<std::shared_ptr<AudioInstance>> pull_new_audios() { return m_animation.pull_new_audios(); }
protected:
  using LuaBaseComponent::set_lua_root;
  [[nodiscard]] std::expected<void, StarException> context_setup() override {
    if (auto result = LuaBaseComponent::context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_animation.context_setup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> context_shutdown() override {
    m_animation.context_shutdown();
    m_messages.context_shutdown();
    auto storage = m_storage.read_storage();
    auto stopped = m_update.context_shutdown();
    auto shutdown = LuaScriptUpdate::finish_shutdown(std::move(stopped), LuaBaseComponent::context_shutdown());
    return m_storage.finish_shutdown(std::move(storage), std::move(shutdown));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_threads_callbacks() override {
    auto callbacks = LuaBaseComponent::make_threads_callbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.add_threads_callbacks(*callbacks); !result)
      return std::unexpected(std::move(result.error()));
    return callbacks;
  }
private:
  LuaScriptWorld m_world;
  LuaScriptUpdate m_update;
  LuaScriptStorage m_storage;
  LuaScriptMessages m_messages;
  LuaScriptAnimation m_animation;
};

}

export module star.lua_animation_component;

export namespace Star {
  using ::Star::LuaAnimationComponentExceptionTag;
  using ::Star::LuaAnimationComponentException;
  using ::Star::LuaScriptAnimation;
  using ::Star::LuaAnimationScript;
  using ::Star::LuaDeploymentScript;
}
