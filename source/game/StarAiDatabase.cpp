import std;
import star.ai_database;
import star.ai_types;
import star.animation;
import star.asset_path;
import star.assets;
import star.color;
import star.format;
import star.json;
import star.lexical_cast;
import star.root;
import star.string;

namespace Star {

AiDatabase::AiDatabase(ConstructionKey, Animation&& static_animation, Animation&& scanline_animation)
  : m_animation_config(std::move(static_animation), std::move(scanline_animation)) {}

std::expected<std::shared_ptr<AiDatabase>, StarException> AiDatabase::create(Root& root) {
  auto default_static = Animation::create(root);
  if (!default_static)
    return std::unexpected(std::move(default_static.error()));
  auto default_scanline = Animation::create(root);
  if (!default_scanline)
    return std::unexpected(std::move(default_scanline.error()));
  auto result = std::make_shared<AiDatabase>(ConstructionKey{}, std::move(*default_static), std::move(*default_scanline));
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/ai/ai.config");
  if (!config)
    return std::unexpected((*assets)->with_asset_context(std::move(config.error()), "/ai/ai.config", "Loading AI configuration"));
  auto missions = (*assets)->scan_extension("aimission");
  if (!missions)
    return std::unexpected(std::move(missions.error()));
  auto queued = (*assets)->queue_jsons(missions->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& asset_file : missions->get()) {
    auto const& file = asset_file.string();
    auto mission_config = (*assets)->json(file);
    if (!mission_config)
      return std::unexpected((*assets)->with_asset_context(std::move(mission_config.error()), file, "Loading AI mission"));
    if (!*mission_config)
      continue;
    auto mission = parse_mission(*mission_config);
    if (!mission)
      return std::unexpected((*assets)->with_asset_context(std::move(mission.error()), file, "Loading AI mission"));
    result->m_missions.insert_or_assign(mission->mission_name, std::move(*mission));
  }
  auto configured = [&]() -> std::expected<void, StarException> {
    auto species = config->get("species").and_then([](Json const& value) { return value.iterate_object(); });
    if (!species)
      return std::unexpected(std::move(species.error()));
    for (auto const& pair : *species) {
      auto parameters = parse_species_parameters(pair.second);
      if (!parameters)
        return std::unexpected(std::move(parameters.error()));
      result->m_species_parameters.try_emplace(pair.first, std::move(*parameters));
    }
    auto statuses = config->get("shipStatus").and_then([](Json const& value) { return value.iterate_object(); });
    if (!statuses)
      return std::unexpected(std::move(statuses.error()));
    for (auto const& pair : *statuses) {
      auto level = lexical_cast<unsigned>(pair.first);
      if (!level)
        return std::unexpected(std::move(level.error()));
      auto speech = parse_speech(pair.second);
      if (!speech)
        return std::unexpected(std::move(speech.error()));
      result->m_ship_status.insert_or_assign(*level, std::move(*speech));
    }
    auto no_missions_config = config->get("noMissionsSpeech");
    if (!no_missions_config)
      return std::unexpected(std::move(no_missions_config.error()));
    auto no_missions = parse_speech(*no_missions_config);
    if (!no_missions)
      return std::unexpected(std::move(no_missions.error()));
    result->m_no_missions_speech = std::move(*no_missions);
    auto no_crew_config = config->get("noCrewSpeech");
    if (!no_crew_config)
      return std::unexpected(std::move(no_crew_config.error()));
    auto no_crew = parse_speech(*no_crew_config);
    if (!no_crew)
      return std::unexpected(std::move(no_crew.error()));
    result->m_no_crew_speech = std::move(*no_crew);
    auto characters_per_second = config->get_float("charactersPerSecond");
    if (!characters_per_second)
      return std::unexpected(std::move(characters_per_second.error()));
    result->m_animation_config.characters_per_second = *characters_per_second;
    auto default_animation = config->get_string("defaultAnimation");
    if (!default_animation)
      return std::unexpected(std::move(default_animation.error()));
    result->m_animation_config.default_animation = std::move(*default_animation);
    auto static_animation = Animation::create(root, "/ai/ai.config:staticAnimation");
    if (!static_animation)
      return std::unexpected(std::move(static_animation.error()));
    result->m_animation_config.static_animation = std::move(*static_animation);
    auto static_opacity = config->get_float("staticOpacity");
    if (!static_opacity)
      return std::unexpected(std::move(static_opacity.error()));
    result->m_animation_config.static_opacity = *static_opacity;
    auto scanline_animation = Animation::create(root, "/ai/ai.config:scanlineAnimation");
    if (!scanline_animation)
      return std::unexpected(std::move(scanline_animation.error()));
    result->m_animation_config.scanline_animation = std::move(*scanline_animation);
    auto scanline_opacity = config->get_float("scanlineOpacity");
    if (!scanline_opacity)
      return std::unexpected(std::move(scanline_opacity.error()));
    result->m_animation_config.scanline_opacity = *scanline_opacity;
    auto animations = config->get("aiAnimations").and_then([](Json const& value) { return value.iterate_object(); });
    if (!animations)
      return std::unexpected(std::move(animations.error()));
    for (auto const& pair : *animations) {
      // Preserve the old default construction's RNG draw before replacement.
      auto initial = Animation::create(root);
      if (!initial)
        return std::unexpected(std::move(initial.error()));
      auto animation = Animation::create(root, pair.second, "/ai/");
      if (!animation)
        return std::unexpected(std::move(animation.error()));
      result->m_animation_config.ai_animations.try_emplace(pair.first, std::move(*animation));
    }
    return {};
  }();
  if (!configured)
    return std::unexpected((*assets)->with_asset_context(std::move(configured.error()), "/ai/ai.config", "Loading AI configuration"));
  return result;
}

std::expected<std::reference_wrapper<AiMission const>, StarException> AiDatabase::mission(String const& mission_name) const {
  return m_missions.get(mission_name);
}

AiSpeciesMissionText const& AiDatabase::mission_text(AiMission const& mission, String const& species) const {
  if (auto text = mission.species_text.find_ref(species))
    return text->get();
  if (auto text = mission.species_text.find_ref("default"))
    return text->get();
  static AiSpeciesMissionText const empty{};
  return empty;
}

std::expected<AiSpeech, StarException> AiDatabase::ship_status(unsigned ship_level) const {
  // Find the first open speech set at this ship level or below.
  auto next = m_ship_status.upper_bound(ship_level);
  if (next != m_ship_status.begin())
    return std::prev(next)->second;

  return AiSpeech{};
}

AiSpeech AiDatabase::no_crew_speech() const {
  return m_no_crew_speech;
}

AiSpeech AiDatabase::no_missions_speech() const {
  return m_no_missions_speech;
}

std::expected<String, StarException> AiDatabase::portrait_image(String const& species, String const& frame) const {
  auto parameters = m_species_parameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return Star::format("/ai/{}:{}", parameters->get().portrait_frames, frame);
}

std::expected<Animation, StarException> AiDatabase::animation(String const& species, String const& animation_name) const {
  auto configured = m_animation_config.ai_animations.get(animation_name);
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  auto animation = configured->get();
  auto parameters = m_species_parameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  animation.setTag("image", parameters->get().ai_frames);
  return animation;
}

std::expected<Animation, StarException> AiDatabase::static_animation(String const& species) const {
  auto animation = m_animation_config.static_animation;
  auto parameters = m_species_parameters.get(species);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  animation.setTag("image", parameters->get().static_frames);
  animation.setColor(Color::rgba_f(1.0f, 1.0f, 1.0f, m_animation_config.static_opacity));
  return animation;
}

Animation AiDatabase::scanline_animation() const {
  auto animation = m_animation_config.scanline_animation;
  animation.setColor(Color::rgba_f(1.0f, 1.0f, 1.0f, m_animation_config.scanline_opacity));
  return animation;
}

float AiDatabase::characters_per_second() const {
  return m_animation_config.characters_per_second;
}

String AiDatabase::default_animation() const {
  return m_animation_config.default_animation;
}

std::expected<AiSpeech, StarException> AiDatabase::parse_speech(Json const& v) {
  auto animation = v.get_string("animation");
  if (!animation)
    return std::unexpected(std::move(animation.error()));
  auto text = v.get_string("text");
  if (!text)
    return std::unexpected(std::move(text.error()));
  auto speed = v.get_float("speedModifier", 1.0f);
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  return AiSpeech{std::move(*animation), std::move(*text), *speed};
}

std::expected<AiDatabase::AiSpeciesParameters, StarException> AiDatabase::parse_species_parameters(Json const& v) {
  auto ai_frames = v.get_string("aiFrames");
  if (!ai_frames)
    return std::unexpected(std::move(ai_frames.error()));
  auto portrait_frames = v.get_string("portraitFrames");
  if (!portrait_frames)
    return std::unexpected(std::move(portrait_frames.error()));
  auto static_frames = v.get_string("staticFrames");
  if (!static_frames)
    return std::unexpected(std::move(static_frames.error()));
  return AiSpeciesParameters{std::move(*ai_frames), std::move(*portrait_frames), std::move(*static_frames)};
}

std::expected<AiSpeciesMissionText, StarException> AiDatabase::parse_species_mission_text(Json const& vm) {
  auto button = vm.get_string("buttonText");
  if (!button)
    return std::unexpected(std::move(button.error()));
  auto repeat = vm.get_string("repeatButtonText");
  if (!repeat)
    return std::unexpected(std::move(repeat.error()));
  auto speech_config = vm.get("selectSpeech", {});
  if (!speech_config)
    return std::unexpected(std::move(speech_config.error()));
  auto speech = parse_speech(*speech_config);
  if (!speech)
    return std::unexpected(std::move(speech.error()));
  return AiSpeciesMissionText{std::move(*button), std::move(*repeat), std::move(*speech)};
}

std::expected<AiMission, StarException> AiDatabase::parse_mission(Json const& vm) {
  auto name = vm.get_string("missionName");
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto world = vm.get_string("missionWorld");
  if (!world)
    return std::unexpected(std::move(world.error()));
  auto warp_animation = vm.opt_string("warpAnimation");
  if (!warp_animation)
    return std::unexpected(std::move(warp_animation.error()));
  auto warp_deploy = vm.opt_bool("warpDeploy");
  if (!warp_deploy)
    return std::unexpected(std::move(warp_deploy.error()));
  auto icon = vm.get_string("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  AiMission mission;
  mission.mission_name = std::move(*name);
  mission.mission_unique_world = std::move(*world);
  mission.warp_animation = std::move(*warp_animation);
  mission.warp_deploy = *warp_deploy;
  mission.icon = AssetPath::relative_to("/ai/", *icon);
  auto texts = vm.get("speciesText").and_then([](Json const& value) { return value.iterate_object(); });
  if (!texts)
    return std::unexpected(std::move(texts.error()));
  for (auto const& pair : *texts) {
    auto text = parse_species_mission_text(pair.second);
    if (!text)
      return std::unexpected(std::move(text.error()));
    mission.species_text.try_emplace(pair.first, std::move(*text));
  }
  return mission;
}

}
