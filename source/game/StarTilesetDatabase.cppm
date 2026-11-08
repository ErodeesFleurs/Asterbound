module;
import std;
import star.string;
import star.list;
import star.format;
import star.json;
import star.image;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.random;
import star.thread;
import star.lexical_cast;
import star.ordered_map;
import star.lru_cache;
import star.set;
import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;

namespace Star {
class Root;
namespace Tiled {
  extern std::expected<EnumMap<TileLayer>, StarException> const LayerNames;

  // Tiled stores property values as strings, including encoded JSON values.
  class Properties {
  public:
    Properties();
    Properties(Json const& json);
    Json toJson() const;
    [[nodiscard]] std::expected<Properties, StarException> inherit(Json const& properties) const;
    [[nodiscard]] std::expected<Properties, StarException> inherit(Properties const& properties) const;
    [[nodiscard]] std::expected<bool, StarException> contains(String const& name) const;
    template <typename T>
    [[nodiscard]] std::expected<T, StarException> get(String const& name) const;
    template <typename T>
    [[nodiscard]] std::expected<std::optional<T>, StarException> opt(String const& name) const;
    template <typename T>
    [[nodiscard]] std::expected<void, StarException> set(String const& name, T const& value);
  private:
    Json m_properties;
  };

  class Tile : public Dungeon::Tile {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<Tile>, StarException> create(Root& root, Properties const& properties, TileLayer layer, bool flipX = false);
    Properties properties;
  private:
    explicit Tile(Properties const& properties) : properties(properties) {}
    [[nodiscard]] std::expected<void, StarException> init(Root& root, TileLayer layer, bool flipX);
  };

  class Tileset {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<Tileset>, StarException> create(Root& root, Json const& json);
    [[nodiscard]] std::expected<std::shared_ptr<Tile const>, StarException> getTile(std::size_t id, TileLayer layer) const;
    std::size_t size() const;
  private:
    Tileset() = default;
    List<std::shared_ptr<Tile const>> const& tiles(TileLayer layer) const;
    List<std::shared_ptr<Tile const>> m_tilesBack, m_tilesFront;
  };
}

class TilesetDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<TilesetDatabase>, StarException> create(Root& root);
  [[nodiscard]] std::expected<std::shared_ptr<Tiled::Tileset const>, StarException> get(String const& path) const;
private:
  explicit TilesetDatabase(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<std::shared_ptr<Tiled::Tileset const>, StarException> readTileset(String const& path) const;
  Root& m_root;
  mutable std::mutex m_cacheMutex;
  mutable HashLruCache<String, std::shared_ptr<Tiled::Tileset const>> m_tilesetCache;
};

namespace Tiled {
  template <typename T>
  struct PropertyConverter {
    [[nodiscard]] static std::expected<T, StarException> to(String const& propertyValue) {
      return lexicalCast<T>(propertyValue);
    }
    [[nodiscard]] static std::expected<String, StarException> from(T const& propertyValue) {
      return Star::to_string(propertyValue);
    }
  };
  template <>
  struct PropertyConverter<Json> {
    [[nodiscard]] static std::expected<Json, StarException> to(String const& propertyValue);
    [[nodiscard]] static std::expected<String, StarException> from(Json const& propertyValue);
  };
  template <>
  struct PropertyConverter<String> {
    [[nodiscard]] static std::expected<String, StarException> to(String const& propertyValue);
    [[nodiscard]] static std::expected<String, StarException> from(String const& propertyValue);
  };
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> getProperty(Json const& properties, String const& propertyName) {
    auto value = properties.get(propertyName);
    if (!value) return std::unexpected(std::move(value.error()));
    auto text = value->toString();
    if (!text) return std::unexpected(std::move(text.error()));
    return PropertyConverter<T>::to(*text);
  }
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> optProperty(Json const& properties, String const& propertyName) {
    auto value = properties.optString(propertyName);
    if (!value) return std::unexpected(std::move(value.error()));
    if (!*value) return std::optional<T>{};
    auto converted = PropertyConverter<T>::to(**value);
    if (!converted) return std::unexpected(std::move(converted.error()));
    return std::optional<T>(std::move(*converted));
  }
  template <typename T>
  [[nodiscard]] std::expected<Json, StarException> setProperty(Json const& properties, String const& propertyName, T const& propertyValue) {
    auto value = PropertyConverter<T>::from(propertyValue);
    if (!value) return std::unexpected(std::move(value.error()));
    return properties.set(propertyName, std::move(*value));
  }
  template <typename T>
  [[nodiscard]] std::expected<T, StarException> Properties::get(String const& name) const {
    return getProperty<T>(m_properties, name);
  }
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> Properties::opt(String const& name) const {
    return optProperty<T>(m_properties, name);
  }
  template <typename T>
  [[nodiscard]] std::expected<void, StarException> Properties::set(String const& name, T const& propertyValue) {
    auto updated = setProperty(m_properties, name, propertyValue);
    if (!updated) return std::unexpected(std::move(updated.error()));
    m_properties = std::move(*updated);
    return {};
  }
}
}

export module star.tileset_database;
export namespace Star {
  using ::Star::TilesetDatabase;
}
export namespace Star::Tiled {
  using ::Star::Tiled::LayerNames;
  using ::Star::Tiled::Properties;
  using ::Star::Tiled::Tile;
  using ::Star::Tiled::Tileset;
  using ::Star::Tiled::PropertyConverter;
  using ::Star::Tiled::getProperty;
  using ::Star::Tiled::optProperty;
  using ::Star::Tiled::setProperty;
}
