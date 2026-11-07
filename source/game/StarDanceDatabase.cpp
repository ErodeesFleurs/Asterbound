import star.output_proxy;
import star.json;
import star.json_extra;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
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

import star.asset_source;
import star.assets;

import star.dance_database;

import star.vector;
namespace Star {

std::expected<std::shared_ptr<DanceDatabase>, StarException> DanceDatabase::create(std::shared_ptr<Assets const> assets, Logger& logger) {
  auto database = std::shared_ptr<DanceDatabase>(new DanceDatabase(logger));
  auto files = assets->scanExtension("dance");
  if (!files)
    return std::unexpected(std::move(files.error()));
  for (auto const& file : files->get()) {
    auto dance = readDance(*assets, file.string());
    if (!dance) {
      auto logged = logger.errorException(strf("Error loading dance file {}", file.string()), ExceptionDiagnostic(dance.error()));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
      continue;
    }
    database->m_dances[(*dance)->name] = std::move(*dance);
  }
  return database;
}

std::expected<std::shared_ptr<Dance>, StarException> DanceDatabase::getDance(String const& name) const {
  if (auto dance = m_dances.find_ref(name))
    return dance->get();
  auto logged = m_logger.error("Invalid dance '{}', using default", name);
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  if (auto fallback = m_dances.find_ref("assetmissing"))
    return fallback->get();
  return std::unexpected(StarException("Missing fallback dance 'assetmissing'"));
}

std::expected<std::shared_ptr<Dance>, StarException> DanceDatabase::readDance(Assets const& assets, String const& path) {
  auto loaded = [&]() -> std::expected<std::shared_ptr<Dance>, StarException> {
  auto config = assets.json(path);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto dance = std::make_shared<Dance>();
  auto name = config->getString("name");
  if (!name)
    return std::unexpected(std::move(name.error()));
  dance->name = std::move(*name);
  auto states = config->getArray("states");
  if (!states)
    return std::unexpected(std::move(states.error()));
  for (auto const& state : *states) {
    auto value = state.toString();
    if (!value)
      return std::unexpected(std::move(value.error()));
    dance->states.append(std::move(*value));
  }
  auto cycle = config->getFloat("cycle");
  if (!cycle)
    return std::unexpected(std::move(cycle.error()));
  dance->cycle = *cycle;
  auto cyclic = config->getBool("cyclic");
  if (!cyclic)
    return std::unexpected(std::move(cyclic.error()));
  dance->cyclic = *cyclic;
  auto duration = config->getFloat("duration");
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  dance->duration = *duration;
  auto steps = config->getArray("steps");
  if (!steps)
    return std::unexpected(std::move(steps.error()));
  for (auto const& step : *steps) {
    DanceStep result{};
    auto readFrame = [&](String const& key, std::size_t index) -> std::expected<std::optional<String>, StarException> {
      if (step.isType(Json::Type::Object))
        return step.optString(key);
      auto entry = step.get(index);
      if (!entry)
        return std::unexpected(std::move(entry.error()));
      return entry->optString();
    };
    auto readOffset = [&](String const& key, std::size_t index) -> std::expected<Vec2F, StarException> {
      std::expected<std::optional<Json>, StarException> entry;
      if (step.isType(Json::Type::Object)) {
        entry = step.opt(key);
      } else {
        auto value = step.get(index);
        if (!value)
          return std::unexpected(std::move(value.error()));
        entry = value->opt();
      }
      if (!entry)
        return std::unexpected(std::move(entry.error()));
      if (!*entry)
        return Vec2F();
      return jsonToVec2F(**entry);
    };
    auto body = readFrame("bodyFrame", 0);
    if (!body)
      return std::unexpected(std::move(body.error()));
    result.bodyFrame = std::move(*body);
    auto front = readFrame("frontArmFrame", 1);
    if (!front)
      return std::unexpected(std::move(front.error()));
    result.frontArmFrame = std::move(*front);
    auto back = readFrame("backArmFrame", 2);
    if (!back)
      return std::unexpected(std::move(back.error()));
    result.backArmFrame = std::move(*back);
    auto headOffset = readOffset("headOffset", 3);
    if (!headOffset)
      return std::unexpected(std::move(headOffset.error()));
    result.headOffset = *headOffset;
    auto frontOffset = readOffset("frontArmOffset", 4);
    if (!frontOffset)
      return std::unexpected(std::move(frontOffset.error()));
    result.frontArmOffset = *frontOffset;
    auto backOffset = readOffset("backArmOffset", 5);
    if (!backOffset)
      return std::unexpected(std::move(backOffset.error()));
    result.backArmOffset = *backOffset;
    if (step.isType(Json::Type::Object)) {
      auto rotation = step.optFloat("frontArmRotation");
      if (!rotation)
        return std::unexpected(std::move(rotation.error()));
      result.frontArmRotation = rotation->value_or(0.0f);
      result.backArmRotation = rotation->value_or(0.0f);
    }
    dance->steps.append(std::move(result));
  }
  return dance;
  }();
  if (!loaded)
    return std::unexpected(assets.withAssetContext(std::move(loaded.error()), path, "Loading dance"));
  return loaded;
}

}
