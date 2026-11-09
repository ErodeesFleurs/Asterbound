import star.format;
import std;
import star.string;
import star.json;
import star.json_extra;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.codex;

namespace Star {

std::expected<std::shared_ptr<Codex>, StarException> Codex::create(Json const& config, String const& path) {
  auto result = std::shared_ptr<Codex>(new Codex());
  result->m_directory = AssetPath::directory(path);
  result->m_filename = AssetPath::filename(path);
  auto id = config.getString("id");
  if (!id)
    return std::unexpected(std::move(id.error()));
  result->m_id = std::move(*id);
  auto species = config.getString("species", "other");
  if (!species)
    return std::unexpected(std::move(species.error()));
  result->m_species = std::move(*species);
  auto title = config.getString("title");
  if (!title)
    return std::unexpected(std::move(title.error()));
  result->m_title = std::move(*title);
  auto description = config.getString("description", "");
  if (!description)
    return std::unexpected(std::move(description.error()));
  result->m_description = std::move(*description);
  auto icon = config.getString("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  result->m_icon = std::move(*icon);
  auto pages = config.get("contentPages").and_then([](Json const& value) { return value.iterateArray(); });
  if (!pages)
    return std::unexpected(std::move(pages.error()));
  for (auto const& page : *pages) {
    auto text = page.toString();
    if (!text)
      return std::unexpected(std::move(text.error()));
    result->m_pages.push_back(std::move(*text));
  }
  auto itemConfig = config.get("itemConfig", Json());
  if (!itemConfig)
    return std::unexpected(std::move(itemConfig.error()));
  result->m_itemConfig = std::move(*itemConfig);
  return result;
}

Json Codex::toJson() const {
  auto result = JsonObject{
      {"id", m_id},
      {"species", m_species},
      {"title", m_title},
      {"description", m_description},
      {"icon", m_icon},
      {"contentPages", jsonFromStringList(m_pages)},
      {"itemConfig", m_itemConfig}};
  return result;
}

String Codex::id() const {
  return m_id;
}

String Codex::species() const {
  return m_species;
}

String Codex::title() const {
  return m_title;
}

String Codex::description() const {
  return m_description;
}

String Codex::icon() const {
  return m_icon;
}

String Codex::page(std::size_t pageNum) const {
  if (pageNum < m_pages.size())
    return m_pages[pageNum];
  return "";
}

List<String> Codex::pages() const {
  return m_pages;
}

std::size_t Codex::pageCount() const {
  return m_pages.size();
}

Json Codex::itemConfig() const {
  return m_itemConfig;
}

String Codex::directory() const {
  return m_directory;
}

String Codex::filename() const {
  return m_filename;
}

}
