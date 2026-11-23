import std;
import star.assets;
import star.codex;
import star.codex_database;
import star.format;
import star.json;
import star.string;

namespace Star {

std::expected<std::shared_ptr<CodexDatabase>, StarException> CodexDatabase::create(Assets const& assets) {
  auto files = assets.scan_extension("codex");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto config = assets.json("/codex.config");
  if (!config)
    return std::unexpected(assets.with_asset_context(std::move(config.error()), "/codex.config", "Loading codex defaults"));
  auto result = std::make_shared<CodexDatabase>(ConstructionKey{});
  if (files->get().empty())
    return result;
  auto queued = assets.queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  auto default_icon = config->get("defaultIcon").and_then([](Json const& value) { return value.string_ptr(); });
  result->m_entries.reserve(files->get().size());
  for (auto const& asset_file : files->get()) {
    auto const& file = asset_file.string();
    auto file_error = [&](StarException error) {
      return CodexDatabaseException(Star::format("Error reading codex config {}", file),
                                    assets.with_asset_context(std::move(error), file, "Loading codex"));
    };
    auto book = assets.json(file).and_then([&](Json const& value) -> std::expected<std::shared_ptr<Codex const>, StarException> {
      if (!default_icon)
        return std::unexpected(assets.with_asset_context(std::move(default_icon.error()), "/codex.config", "Loading codex defaults"));
      return Codex::create(value, file, **default_icon);
    });
    if (!book)
      return std::unexpected(file_error(std::move(book.error())));
    auto const& id = (*book)->id();
    if (!result->m_entries.try_emplace(id, std::move(*book)).second)
      return std::unexpected(file_error(CodexDatabaseException::format("Duplicate codex named '{}', config file '{}'", id, file)));
  }
  return result;
}

std::shared_ptr<Codex const> CodexDatabase::find(String const& id) const {
  if (auto book = m_entries.find_ref(id))
    return book->get();
  return {};
}

}
