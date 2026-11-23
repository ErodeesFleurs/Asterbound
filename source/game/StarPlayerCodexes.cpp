import std;
import star.format;
import star.string;
import star.list;
import star.json;
import star.logging;
import star.assets;
import star.root_base;
import star.root;
import star.codex;
import star.codex_database;
import star.player_codexes;

namespace Star {

namespace {
std::atomic<std::uint64_t> next_domain_id{1};
}

PlayerCodexes::PlayerCodexes(Root& root)
  : m_root(root), m_domain_id(next_domain_id.fetch_add(1, std::memory_order_relaxed)) {}

std::expected<std::shared_ptr<PlayerCodexes>, StarException> PlayerCodexes::create(Root& root, Json const& json) {
  auto domain = std::make_shared<PlayerCodexes>(root);
  if (json.is_null())
    return domain;
  auto entries = json.object_ptr();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  if ((*entries)->empty())
    return domain;
  auto database = root.codex_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  for (auto const& [id, state] : **entries) {
    auto read = state.to_bool();
    if (!read)
      return std::unexpected(std::move(read.error()));
    if (auto codex = (*database)->find(id)) {
      domain->insert(id, std::move(codex), *read);
    } else if (auto logged = root.logger().debug("Failed to load missing codex '{}'", id); !logged) {
      return std::unexpected(std::move(logged.error()));
    }
  }
  return domain;
}

Json PlayerCodexes::to_json() const {
  JsonObject entries;
  for (auto const& [id, entry] : m_codexes)
    entries.emplace(id, entry.read);
  return entries;
}

List<CodexEntry> PlayerCodexes::entries() const {
  List<CodexEntry> result;
  result.reserve(m_codexes.size());
  for (auto const& entry : m_codexes)
    result.push_back(entry.second);
  std::sort(result.begin(), result.end(), [](CodexEntry const& left, CodexEntry const& right) {
    if (left.read != right.read)
      return !left.read;
    return left.codex->title() < right.codex->title();
  });
  return result;
}

std::shared_ptr<Codex const> PlayerCodexes::find(String const& id) const {
  if (auto entry = m_codexes.find_ref(id))
    return entry->get().codex;
  return {};
}

bool PlayerCodexes::contains(String const& id) const {
  return m_codexes.contains(id);
}

CodexLearnResult PlayerCodexes::insert(String const& id, std::shared_ptr<Codex const> codex, bool read) {
  if (!m_codexes.try_emplace(id, std::move(codex), read).second)
    return CodexLearnResult::AlreadyKnown;
  if (!read)
    ++m_unread_count;
  ++m_revision;
  return CodexLearnResult::Learned;
}

std::expected<CodexLearnResult, StarException> PlayerCodexes::learn(String const& id, bool read) {
  if (contains(id))
    return CodexLearnResult::AlreadyKnown;
  auto database = m_root.codex_database();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto codex = (*database)->find(id);
  if (!codex)
    return CodexLearnResult::NotFound;
  return insert(id, std::move(codex), read);
}

bool PlayerCodexes::is_read(String const& id) const {
  auto entry = m_codexes.find_ref(id);
  return entry && entry->get().read;
}

bool PlayerCodexes::set_read(String const& id, bool read) {
  auto entry = m_codexes.find_ref(id);
  if (!entry || entry->get().read == read)
    return false;
  entry->get().read = read;
  if (read)
    --m_unread_count;
  else
    ++m_unread_count;
  ++m_revision;
  return true;
}

std::expected<void, StarException> PlayerCodexes::learn_initial(String const& species) {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(Star::format("/player.config:defaultCodexes.{}", species));
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto defaults = config->array_ptr();
  if (!defaults)
    return std::unexpected(std::move(defaults.error()));
  // Validate the complete list before modifying this domain.
  for (auto const& value : **defaults) {
    if (!value.is_type(Json::Type::String))
      return std::unexpected(value.to_string().error());
  }
  std::shared_ptr<CodexDatabase const> database;
  for (auto const& value : **defaults) {
    auto id = value.string_ptr();
    if (contains(**id))
      continue;
    if (!database) {
      auto loaded = m_root.codex_database();
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      database = std::move(*loaded);
    }
    if (auto codex = database->find(**id))
      insert(**id, std::move(codex), true);
  }
  return {};
}

std::shared_ptr<Codex const> PlayerCodexes::first_unread() const {
  if (m_unread_count == 0)
    return {};
  for (auto const& entry : m_codexes) {
    if (!entry.second.read)
      return entry.second.codex;
  }
  return {};
}

}
