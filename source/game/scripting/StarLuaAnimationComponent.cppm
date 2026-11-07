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
  List<LightSource> const& lightSources();
  List<Particle> pullNewParticles();
  List<std::shared_ptr<AudioInstance>> pullNewAudios();
  [[nodiscard]] std::expected<void, StarException> contextSetup();
  void contextShutdown();
private:
  LuaBaseComponent& m_host;
  List<Particle> m_pendingParticles;
  List<std::shared_ptr<AudioInstance>> m_pendingAudios;
  List<std::shared_ptr<AudioInstance>> m_activeAudio;
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> m_drawables;
  List<LightSource> m_lightSources;
};

[[nodiscard]] inline std::expected<void, StarException> LuaScriptAnimation::contextSetup() {
  if (m_host.hasCallbacks("localAnimator"))
    return {};
  LuaCallbacks callbacks;
  if (auto result = callbacks.registerCallback("playAudio", [&self = *this](String const& sound, std::optional<int> loops, std::optional<float> volume) -> std::expected<std::shared_ptr<AudioInstance>, StarException> {
    auto assets = self.m_host.root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto source = (*assets)->audio(sound);
    if (!source)
      return std::unexpected(std::move(source.error()));
    auto audio = AudioInstance::create(**source);
    if (!audio)
      return std::unexpected(std::move(audio.error()));
    (*audio)->setLoops(loops.value_or(0));
    (*audio)->setVolume(volume.value_or(1.0f));
    self.m_pendingAudios.append(*audio);
    self.m_activeAudio.append(*audio);
    return audio;
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("spawnParticle", [&self = *this](Json const& config, std::optional<Vec2F> const& position) -> std::expected<void, StarException> {
    auto database = self.m_host.root().particleDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto particle = (*database)->particle(config);
    if (!particle)
      return std::unexpected(std::move(particle.error()));
    particle->translate(position.value_or(Vec2F{}));
    self.m_pendingParticles.append(std::move(*particle));
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("clearDrawables", [&self = *this]() { self.m_drawables.clear(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("addDrawable", [&self = *this](Drawable drawable, std::optional<String> layerName) -> std::expected<void, StarException> {
    std::optional<EntityRenderLayer> layer;
    if (layerName) {
      auto parsed = parseRenderLayer(*layerName);
      if (!parsed)
        return std::unexpected(std::move(parsed.error()));
      layer = *parsed;
    }
    if (std::holds_alternative<Drawable::ImagePart>(drawable.part))
      std::get<Drawable::ImagePart>(drawable.part).transformation.scale(0.125f);
    self.m_drawables.append({std::move(drawable), layer});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("addJsonDrawable", [&self = *this](Json config, std::optional<String> layerName) -> std::expected<void, StarException> {
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
    self.m_drawables.append({std::move(*drawable), layer});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("clearLightSources", [&self = *this]() { self.m_lightSources.clear(); }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("addLightSource", [&self = *this](LuaTable const& table) -> std::expected<void, StarException> {
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
    self.m_lightSources.append({.position = *position,
                                .color = color->toRgbF(),
                                .type = static_cast<LightType>(point->value_or(false)),
                                .pointBeam = beam->value_or(float{}),
                                .beamAngle = angle->value_or(float{}),
                                .beamAmbience = ambience->value_or(float{})});
    return {};
  }); !result)
    return std::unexpected(std::move(result.error()));
  return m_host.addCallbacks("localAnimator", std::move(callbacks));
}

inline List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& LuaScriptAnimation::drawables() { return m_drawables; }

inline List<LightSource> const& LuaScriptAnimation::lightSources() { return m_lightSources; }

inline List<Particle> LuaScriptAnimation::pullNewParticles() { return std::exchange(m_pendingParticles, {}); }

inline List<std::shared_ptr<AudioInstance>> LuaScriptAnimation::pullNewAudios() {
  m_activeAudio.filter([](std::shared_ptr<AudioInstance> const& audio) { return !audio->finished(); });
  return std::exchange(m_pendingAudios, {});
}

inline void LuaScriptAnimation::contextShutdown() {
  for (auto const& audio : m_activeAudio)
    audio->setLoops(0);
  m_activeAudio.clear();
}

// Animation-only contexts do not carry message handlers or persistent storage.
class LuaAnimationScript final : public LuaBaseComponent {
public:
  explicit LuaAnimationScript(Root& root) : LuaBaseComponent(root), m_world(*this), m_update(*this), m_animation(*this) {}
  ~LuaAnimationScript() override { m_update.destroy(); }
  [[nodiscard]] std::expected<void, StarException> init(World& world) { return m_world.init(world); }
  [[nodiscard]] std::expected<void, StarException> uninit() { return m_world.uninit(); }
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& drawables() { return m_animation.drawables(); }
  List<LightSource> const& lightSources() { return m_animation.lightSources(); }
  List<Particle> pullNewParticles() { return m_animation.pullNewParticles(); }
  List<std::shared_ptr<AudioInstance>> pullNewAudios() { return m_animation.pullNewAudios(); }
protected:
  using LuaBaseComponent::setLuaRoot;
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_animation.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    m_animation.contextShutdown();
    auto stopped = m_update.contextShutdown();
    return LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
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
  unsigned updateDelta() const { return m_update.updateDelta(); }
  float updateDt(float dt) const { return m_update.updateDt(dt); }
  float updateDt() const { return m_update.updateDt(); }
  void setUpdateDelta(unsigned delta) { m_update.setUpdateDelta(delta); }
  bool updateReady() const { return m_update.updateReady(); }
  template <typename Ret = LuaValue, typename... V>
  [[nodiscard]] std::expected<std::optional<Ret>, StarException> update(V&&... args) {
    return m_update.template update<Ret>(std::forward<V>(args)...);
  }
  RpcPromise<Json> threadPassMessage(String const& thread, String const& message, JsonArray const& args) override {
    return m_update.threadPassMessage(thread, message, args);
  }
  [[nodiscard]] std::expected<JsonObject, StarException> getScriptStorage() const { return m_storage.getScriptStorage(); }
  [[nodiscard]] std::expected<void, StarException> setScriptStorage(JsonObject storage) { return m_storage.setScriptStorage(std::move(storage)); }
  [[nodiscard]] std::expected<std::optional<ChainableJsonMessageResponse>, StarException> handleMessage(String const& message, bool localMessage, JsonArray const& args = {}) {
    return m_messages.handleMessage(message, localMessage, args);
  }
  List<std::pair<Drawable, std::optional<EntityRenderLayer>>> const& drawables() { return m_animation.drawables(); }
  List<LightSource> const& lightSources() { return m_animation.lightSources(); }
  List<Particle> pullNewParticles() { return m_animation.pullNewParticles(); }
  List<std::shared_ptr<AudioInstance>> pullNewAudios() { return m_animation.pullNewAudios(); }
protected:
  using LuaBaseComponent::setLuaRoot;
  [[nodiscard]] std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_update.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_storage.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_messages.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = m_animation.contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] std::expected<void, StarException> contextShutdown() override {
    m_animation.contextShutdown();
    m_messages.contextShutdown();
    auto storage = m_storage.readStorage();
    auto stopped = m_update.contextShutdown();
    auto shutdown = LuaScriptUpdate::finishShutdown(std::move(stopped), LuaBaseComponent::contextShutdown());
    return m_storage.finishShutdown(std::move(storage), std::move(shutdown));
  }
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeThreadsCallbacks() override {
    auto callbacks = LuaBaseComponent::makeThreadsCallbacks();
    if (!callbacks)
      return std::unexpected(std::move(callbacks.error()));
    if (auto result = m_update.addThreadsCallbacks(*callbacks); !result)
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
