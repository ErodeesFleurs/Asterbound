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
import star.optional;
import star.string;
import star.format;
import star.version;

import star.codex;
import star.asset_source;
import star.assets;

import star.codex_database;

namespace Star {

std::expected<std::shared_ptr<CodexDatabase>, StarException> CodexDatabase::create(std::shared_ptr<Assets const> assets) {
  auto result = std::shared_ptr<CodexDatabase>(new CodexDatabase());
  auto files = assets->scanExtension("codex");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto codexConfig = assets->json("/codex.config");
  if (!codexConfig)
    return std::unexpected(assets->withAssetContext(std::move(codexConfig.error()), "/codex.config", "Loading codex defaults"));
  auto queued = assets->queueJsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& assetFile : files->get()) {
    auto const& file = assetFile.string();
    auto loaded = [&]() -> std::expected<std::shared_ptr<Codex>, StarException> {
      auto codexJson = assets->json(file);
      if (!codexJson)
        return std::unexpected(std::move(codexJson.error()));
      auto defaultIcon = codexConfig->getString("defaultIcon");
      if (!defaultIcon)
        return std::unexpected(assets->withAssetContext(std::move(defaultIcon.error()), "/codex.config", "Loading codex defaults"));
      auto icon = codexJson->getString("icon", *defaultIcon);
      if (!icon)
        return std::unexpected(std::move(icon.error()));
      auto resolved = codexJson->set("icon", AssetPath::relativeTo(AssetPath::directory(file), *icon));
      if (!resolved)
        return std::unexpected(std::move(resolved.error()));
      auto codex = Codex::create(*resolved, file);
      if (!codex)
        return std::unexpected(std::move(codex.error()));
      if (result->m_codexes.contains((*codex)->id()))
        return std::unexpected(CodexDatabaseException::format("Duplicate codex named '{}', config file '{}'", (*codex)->id(), file));
      return codex;
    }();
    if (!loaded)
      return std::unexpected(CodexDatabaseException(strf("Error reading codex config {}", file), assets->withAssetContext(std::move(loaded.error()), file, "Loading codex")));
    result->m_codexes[(*loaded)->id()] = std::move(*loaded);
  }
  return result;
}

StringMap<std::shared_ptr<Codex const>> CodexDatabase::codexes() const {
  return m_codexes;
}

std::shared_ptr<Codex const> CodexDatabase::codex(String const& codexId) const {
  if (auto codex = m_codexes.find_ref(codexId))
    return codex->get();
  return {};
}

}
