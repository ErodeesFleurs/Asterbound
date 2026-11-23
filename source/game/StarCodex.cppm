module;

import std;
import star.string;
import star.list;
import star.json;
import star.format;

namespace Star {

class Codex {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  explicit Codex(ConstructionKey) {}
  Codex(Codex const&) = delete;
  Codex& operator=(Codex const&) = delete;
  [[nodiscard]] static std::expected<std::shared_ptr<Codex const>, StarException>
  create(Json const& config, String const& path, String const& default_icon);

  String const& id() const& { return m_id; }
  String const& species() const& { return m_species; }
  String const& title() const& { return m_title; }
  String const& description() const& { return m_description; }
  String const& icon() const& { return m_icon; }
  String const& page(std::size_t index) const&;
  List<String> const& pages() const& { return m_pages; }
  Json const& item_config() const& { return m_item_config; }
  String const& directory() const& { return m_directory; }
  String const& filename() const& { return m_filename; }

  String const& id() const&& = delete;
  String const& species() const&& = delete;
  String const& title() const&& = delete;
  String const& description() const&& = delete;
  String const& icon() const&& = delete;
  String const& page(std::size_t) const&& = delete;
  List<String> const& pages() const&& = delete;
  Json const& item_config() const&& = delete;
  String const& directory() const&& = delete;
  String const& filename() const&& = delete;

private:
  String m_id;
  String m_species;
  String m_title;
  String m_description;
  String m_icon;
  List<String> m_pages;
  Json m_item_config;
  String m_directory;
  String m_filename;
};

}

export module star.codex;

export namespace Star {
  using ::Star::Codex;
}
