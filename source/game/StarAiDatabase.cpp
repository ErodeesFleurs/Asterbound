import star.json;
import star.json_extra;
import star.ordered_set;
import star.data_stream;
import star.bi_map;
import star.color;
import star.game_types;
import star.strong_typedef;
import star.lexical_cast;
import star.ordered_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.item_descriptor;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.ai_types;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.ai_database;
import star.animation;

namespace Star {

AiDatabase::AiDatabase(Animation staticAnimation, Animation scanlineAnimation)
  : m_animationConfig(std::move(staticAnimation), std::move(scanlineAnimation)) {}

std::expected<std::shared_ptr<AiDatabase>, StarException> AiDatabase::create(Root& root) {
  auto defaultStatic = Animation::create(root);
  if (!defaultStatic)
    return std::unexpected(std::move(defaultStatic.error()));
  auto defaultScanline = Animation::create(root);
  if (!defaultScanline)
    return std::unexpected(std::move(defaultScanline.error()));
  auto result = std::shared_ptr<AiDatabase>(new AiDatabase(std::move(*defaultStatic), std::move(*defaultScanline)));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/ai/ai.config");
  if (!config)
    return std::unexpected((*assets)->withAssetContext(std::move(config.error()), "/ai/ai.config", "Loading AI configuration"));
  auto missions = (*assets)->scanExtension("aimission");
  if (!missions)
    return std::unexpected(std::move(missions.error()));
  auto queued = (*assets)->queueJsons(missions->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : missions->get()) {
    auto const& file = assetFile.string();
    auto missionConfig = (*assets)->json(file);
    if (!missionConfig)
      return std::unexpected((*assets)->withAssetContext(std::move(missionConfig.error()), file, "Loading AI mission"));
    if (!*missionConfig)
      continue;
    auto mission = parseMission(*missionConfig);
    if (!mission)
      return std::unexpected((*assets)->withAssetContext(std::move(mission.error()), file, "Loading AI mission"));
    result->m_missions[mission->missionName] = std::move(*mission);
  }
  auto configured = [&]() -> std::expected<void, StarException> {
    auto species = config->get("species").and_then([](Json const& value) { return value.iterateObject(); });
    if (!species)
      return std::unexpected(std::move(species.error()));
    for (auto const& pair : *species) {
      auto parameters = parseSpeciesParameters(pair.second);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      result->m_speciesParameters[pair.first] = std::move(*parameters);
    }
    auto statuses = config->get("shipStatus").and_then([](Json const& value) { return value.iterateObject(); });
    if (!statuses)
      return std::unexpected(std::move(statuses.error()));
    for (auto const& pair : *statuses) {
      auto level = lexicalCast<unsigned>(pair.first);
      if (!level)
        return std::unexpected(std::move(level.error()));
      auto speech = parseSpeech(pair.second);
      if (!speech)
        return std::unexpected(std::move(speech.error()));
      result->m_shipStatus[*level] = std::move(*speech);
    }
    auto noMissionsConfig = config->get("noMissionsSpeech");
    if (!noMissionsConfig)
      return std::unexpected(std::move(noMissionsConfig.error()));
    auto noMissions = parseSpeech(*noMissionsConfig);
    if (!noMissions)
      return std::unexpected(std::move(noMissions.error()));
    result->m_noMissionsSpeech = std::move(*noMissions);
    auto noCrewConfig = config->get("noCrewSpeech");
    if (!noCrewConfig)
      return std::unexpected(std::move(noCrewConfig.error()));
    auto noCrew = parseSpeech(*noCrewConfig);
    if (!noCrew)
      return std::unexpected(std::move(noCrew.error()));
    result->m_noCrewSpeech = std::move(*noCrew);
    auto charactersPerSecond = config->getFloat("charactersPerSecond");
    if (!charactersPerSecond)
      return std::unexpected(std::move(charactersPerSecond.error()));
    result->m_animationConfig.charactersPerSecond = *charactersPerSecond;
    auto defaultAnimation = config->getString("defaultAnimation");
    if (!defaultAnimation)
      return std::unexpected(std::move(defaultAnimation.error()));
    result->m_animationConfig.defaultAnimation = std::move(*defaultAnimation);
    auto staticAnimation = Animation::create(root, "/ai/ai.config:staticAnimation");
    if (!staticAnimation)
      return std::unexpected(std::move(staticAnimation.error()));
    result->m_animationConfig.staticAnimation = std::move(*staticAnimation);
    auto staticOpacity = config->getFloat("staticOpacity");
    if (!staticOpacity)
      return std::unexpected(std::move(staticOpacity.error()));
    result->m_animationConfig.staticOpacity = *staticOpacity;
    auto scanlineAnimation = Animation::create(root, "/ai/ai.config:scanlineAnimation");
    if (!scanlineAnimation)
      return std::unexpected(std::move(scanlineAnimation.error()));
    result->m_animationConfig.scanlineAnimation = std::move(*scanlineAnimation);
    auto scanlineOpacity = config->getFloat("scanlineOpacity");
    if (!scanlineOpacity)
      return std::unexpected(std::move(scanlineOpacity.error()));
    result->m_animationConfig.scanlineOpacity = *scanlineOpacity;
    auto animations = config->get("aiAnimations").and_then([](Json const& value) { return value.iterateObject(); });
    if (!animations)
      return std::unexpected(std::move(animations.error()));
    for (auto const& pair : *animations) {
      // Preserve the old default construction's RNG draw before replacement.
      if (!result->m_animationConfig.aiAnimations.contains(pair.first)) {
        auto initial = Animation::create(root);
        if (!initial)
          return std::unexpected(std::move(initial.error()));
        result->m_animationConfig.aiAnimations.try_emplace(pair.first, std::move(*initial));
      }
      auto animation = Animation::create(root, pair.second, "/ai/");
      if (!animation)
        return std::unexpected(std::move(animation.error()));
      auto existing = result->m_animationConfig.aiAnimations.maybeRef(pair.first);
      if (!existing)
        return std::unexpected(AiException("AI animation publication failed"));
      existing->get() = std::move(*animation);
    }
    return {};
  }();
  if (!configured)
    return std::unexpected((*assets)->withAssetContext(std::move(configured.error()), "/ai/ai.config", "Loading AI configuration"));
  return result;
}

std::expected<AiMission, StarException> AiDatabase::mission(String const& missionName) const {
  auto mission = m_missions.get(missionName);
  if (!mission)
    return std::unexpected(std::move(mission.error()));
  return mission->get();
}

std::expected<AiSpeech, StarException> AiDatabase::shipStatus(unsigned shipLevel) const {
  // Find the first open speech set at this ship level or below.
  auto i = m_shipStatus.upper_bound(shipLevel);
  if (i != m_shipStatus.begin() && (--i)->first <= shipLevel)
    return i->second;

  return AiSpeech{};
}

AiSpeech AiDatabase::noCrewSpeech() const {
  return m_noCrewSpeech;
}

AiSpeech AiDatabase::noMissionsSpeech() const {
  return m_noMissionsSpeech;
}

std::expected<String, StarException> AiDatabase::portraitImage(String const& species, String const& frame) const {
  auto parameters = m_speciesParameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return strf("/ai/{}:{}", parameters->get().portraitFrames, frame);
}

std::expected<Animation, StarException> AiDatabase::animation(String const& species, String const& animationName) const {
  auto configured = m_animationConfig.aiAnimations.get(animationName);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  auto animation = configured->get();
  auto parameters = m_speciesParameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  animation.setTag("image", parameters->get().aiFrames);
  return animation;
}

std::expected<Animation, StarException> AiDatabase::staticAnimation(String const& species) const {
  auto animation = m_animationConfig.staticAnimation;
  auto parameters = m_speciesParameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  animation.setTag("image", parameters->get().staticFrames);
  animation.setColor(Color::rgbaf(1.0f, 1.0f, 1.0f, m_animationConfig.staticOpacity));
  return animation;
}

Animation AiDatabase::scanlineAnimation() const {
  auto animation = m_animationConfig.scanlineAnimation;
  animation.setColor(Color::rgbaf(1.0f, 1.0f, 1.0f, m_animationConfig.scanlineOpacity));
  return animation;
}

float AiDatabase::charactersPerSecond() const {
  return m_animationConfig.charactersPerSecond;
}

String AiDatabase::defaultAnimation() const {
  return m_animationConfig.defaultAnimation;
}

std::expected<AiSpeech, StarException> AiDatabase::parseSpeech(Json const& v) {
  auto animation = v.getString("animation");
  if (!animation)
    return std::unexpected(std::move(animation.error()));
  auto text = v.getString("text");
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto speed = v.getFloat("speedModifier", 1.0f);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  return AiSpeech{*animation, *text, *speed};
}

std::expected<AiDatabase::AiSpeciesParameters, StarException> AiDatabase::parseSpeciesParameters(Json const& v) {
  auto aiFrames = v.getString("aiFrames");
  if (!aiFrames)
    return std::unexpected(std::move(aiFrames.error()));
  auto portraitFrames = v.getString("portraitFrames");
  if (!portraitFrames)
    return std::unexpected(std::move(portraitFrames.error()));
  auto staticFrames = v.getString("staticFrames");
  if (!staticFrames)
    return std::unexpected(std::move(staticFrames.error()));
  return AiSpeciesParameters{*aiFrames, *portraitFrames, *staticFrames};
}

std::expected<AiSpeciesMissionText, StarException> AiDatabase::parseSpeciesMissionText(Json const& vm) {
  auto button = vm.getString("buttonText");
  if (!button)
    return std::unexpected(std::move(button.error()));
  auto repeat = vm.getString("repeatButtonText");
  if (!repeat)
    return std::unexpected(std::move(repeat.error()));
  auto speechConfig = vm.get("selectSpeech", {});
  if (!speechConfig)
    return std::unexpected(std::move(speechConfig.error()));
  auto speech = parseSpeech(*speechConfig);
  if (!speech)
    return std::unexpected(std::move(speech.error()));
  return AiSpeciesMissionText{*button, *repeat, *speech};
}

std::expected<AiMission, StarException> AiDatabase::parseMission(Json const& vm) {
  auto name = vm.getString("missionName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto world = vm.getString("missionWorld");
  if (!world)
    return std::unexpected(std::move(world.error()));
  auto warpAnimation = vm.optString("warpAnimation");
  if (!warpAnimation)
    return std::unexpected(std::move(warpAnimation.error()));
  auto warpDeploy = vm.optBool("warpDeploy");
  if (!warpDeploy)
    return std::unexpected(std::move(warpDeploy.error()));
  auto icon = vm.getString("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  AiMission mission;
  mission.missionName = std::move(*name);
  mission.missionUniqueWorld = std::move(*world);
  mission.warpAnimation = std::move(*warpAnimation);
  mission.warpDeploy = *warpDeploy;
  mission.icon = AssetPath::relativeTo("/ai/", *icon);
  auto texts = vm.get("speciesText").and_then([](Json const& value) { return value.iterateObject(); });
  if (!texts)
    return std::unexpected(std::move(texts.error()));
  for (auto const& pair : *texts) {
    auto text = parseSpeciesMissionText(pair.second);
    if (!text)
      return std::unexpected(std::move(text.error()));
    mission.speciesText[pair.first] = std::move(*text);
  }
  return mission;
}

}
