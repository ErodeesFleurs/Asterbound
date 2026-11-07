import star.json;
import star.json_extra;
import star.data_stream;
import star.ordered_map;
import star.bi_map;
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

import star.uuid;
import star.codex;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.codex_database;
import star.player_codexes;

import star.algorithm;
namespace Star {

PlayerCodexes::PlayerCodexes(Root& root) : m_root(root) {}

std::expected<std::shared_ptr<PlayerCodexes>, StarException> PlayerCodexes::create(Root& root, Json const& json) {
  auto playerCodexes = std::make_shared<PlayerCodexes>(root);
  if (!json)
    return playerCodexes;
  auto database = root.codexDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto entries = json.iterateObject();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& entry : *entries) {
    auto read = entry.second.toBool();
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (auto codex = (*database)->codex(entry.first)) {
      playerCodexes->m_codexes[entry.first] = CodexEntry{std::move(codex), *read};
    } else {
      auto logged = root.logger().debug("Failed to load missing codex '{}'", entry.first);
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    }
  }
  return playerCodexes;
}

Json PlayerCodexes::toJson() const {
  JsonObject entries;
  for (auto const& entry : m_codexes)
    entries[entry.first] = entry.second.second;
  return entries;
}

List<PlayerCodexes::CodexEntry> PlayerCodexes::codexes() const {
  List<CodexEntry> result;
  for (auto pair : m_codexes)
    result.append(pair.second);
  std::sort(result.begin(), result.end(),
      [](CodexEntry const& left, CodexEntry const& right) -> bool {
        return std::make_tuple(left.second, left.first->title()) < std::make_tuple(right.second, right.first->title());
      });
  return result;
}

std::shared_ptr<Codex const> PlayerCodexes::codex(String const& codexId) const {
  if (auto entry = m_codexes.find_ref(codexId))
    return entry->get().first;
  return {};
}

bool PlayerCodexes::codexKnown(String const& codexId) const {
  return m_codexes.contains(codexId);
}

std::expected<std::shared_ptr<Codex const>, StarException> PlayerCodexes::learnCodex(String const& codexId, bool markRead) {
  if (!codexKnown(codexId)) {
    auto database = m_root.codexDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    if (auto codex = (*database)->codex(codexId)) {
      m_codexes[codexId] = CodexEntry{codex, markRead};
      return codex;
    }
  }
  return std::shared_ptr<Codex const>();
}

bool PlayerCodexes::codexRead(String const& codexId) const {
  auto entry = m_codexes.find_ref(codexId);
  return entry && entry->get().second;
}

bool PlayerCodexes::markCodexRead(String const& codexId) {
  if (codexKnown(codexId) && !codexRead(codexId)) {
    m_codexes[codexId].second = true;
    return true;
  }
  return false;
}

bool PlayerCodexes::markCodexUnread(String const& codexId) {
  if (codexKnown(codexId) && codexRead(codexId)) {
    m_codexes[codexId].second = false;
    return true;
  }
  return false;
}

std::expected<void, StarException> PlayerCodexes::learnInitialCodexes(String const& playerSpecies) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(strf("/player.config:defaultCodexes.{}", playerSpecies));
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto codexes = jsonToStringList(*config);
  if (!codexes)
    return std::unexpected(std::move(codexes.error()));
  for (auto const& codexId : *codexes) {
    auto learned = learnCodex(codexId, true);
    if (!learned)
      return std::unexpected(std::move(learned.error()));
  }
  return {};
}

std::shared_ptr<Codex const> PlayerCodexes::firstNewCodex() const {
  for (auto const& pair : m_codexes) {
    if (!pair.second.second)
      return pair.second.first;
  }
  return {};
}

}
