import std;
import star.asset_path;
import star.codex;
import star.format;
import star.string;
import star.json;
import star.json_extra;
import star.list;

namespace Star {

std::expected<std::shared_ptr<Codex const>, StarException>
Codex::create(Json const& config, String const& path, String const& default_icon) {
  auto result = std::make_shared<Codex>(ConstructionKey{});
  auto id = config.get_string("id");
  if (!id)
    return std::unexpected(std::move(id.error()));
  result->m_id = std::move(*id);
  auto species = config.get_string("species", "other");
  if (!species)
    return std::unexpected(std::move(species.error()));
  result->m_species = std::move(*species);
  auto title = config.get_string("title");
  if (!title)
    return std::unexpected(std::move(title.error()));
  result->m_title = std::move(*title);
  auto description = config.get_string("description", "");
  if (!description)
    return std::unexpected(std::move(description.error()));
  result->m_description = std::move(*description);
  auto icon = config.find("icon");
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  result->m_directory = AssetPath::directory(path);
  result->m_filename = AssetPath::filename(path);
  if (*icon && !(*icon)->get().is_null()) {
    auto text = (*icon)->get().string_ptr();
    if (!text)
      return std::unexpected(std::move(text.error()));
    result->m_icon = AssetPath::relative_to(result->m_directory, **text);
  } else {
    result->m_icon = AssetPath::relative_to(result->m_directory, default_icon);
  }
  auto pages = config.get("contentPages").and_then(json_to_string_list);
  if (!pages)
    return std::unexpected(std::move(pages.error()));
  result->m_pages = std::move(*pages);
  auto item_config = config.get("itemConfig", Json());
  if (!item_config)
    return std::unexpected(std::move(item_config.error()));
  result->m_item_config = std::move(*item_config);
  return result;
}

String const& Codex::page(std::size_t index) const& {
  if (index < m_pages.size())
    return m_pages[index];
  static String const empty;
  return empty;
}

}
