import star.json;
import star.json_extra;
import star.id_map;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.bi_map;
import star.lua;
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

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.lua_components;
import star.scripted_entity;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.stagehand;

import star.stagehand_database;

namespace Star {

std::expected<std::shared_ptr<StagehandDatabase>, StarException> StagehandDatabase::create(Root& root) {
  struct MakeShared final : StagehandDatabase {
    explicit MakeShared(Root& root) : StagehandDatabase(root) {}
  };
  auto database = std::make_shared<MakeShared>(root);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("stagehand");
  if (!files)
    return std::unexpected(std::move(files.error()));
  if (auto queued = (*assets)->queueJsons(files->get()); !queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& scannedFile : files->get()) {
    String file = scannedFile.string();
    auto load = [&]() -> std::expected<void, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto typeName = config->getString("type");
      if (!typeName)
        return std::unexpected(std::move(typeName.error()));
      if (database->m_stagehandTypes.contains(*typeName))
        return std::unexpected(StagehandDatabaseException(Star::format("Repeat stagehand type name '{}'", *typeName)));
      database->m_stagehandTypes[*typeName] = std::move(*config);
      return {};
    }();
    if (!load)
      return std::unexpected(StagehandDatabaseException(Star::format("Error loading stagehand type '{}'", file), (*assets)->withAssetContext(std::move(load.error()), file, "Loading stagehand type")));
  }
  return database;
}

std::expected<std::shared_ptr<Stagehand>, StarException> StagehandDatabase::createStagehand(String const& stagehandType, Json const& extraConfig) const {
  auto config = m_stagehandTypes.get(stagehandType);
  if (!config)
    return std::unexpected(std::move(config.error()));
  return Stagehand::create(m_root, jsonMerge(config->get(), extraConfig));
}

}
