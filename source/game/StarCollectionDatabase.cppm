module;

import std;
import star.string;
import star.list;
import star.format;
import star.bi_map;
import star.json;
import star.thread;

namespace Star {

class Json;
class Root;
class ItemDatabase;

struct CollectionDatabaseExceptionTag {
  static constexpr std::string_view name() { return "CollectionDatabaseException"; }
};
using CollectionDatabaseException = StarError<CollectionDatabaseExceptionTag, StarException>;

enum class CollectionType : std::uint16_t {
  Generic,
  Item,
  Monster
};
inline constexpr auto CollectionTypeNames = sortedEnumNames(std::to_array<std::pair<CollectionType, std::string_view>>(
  {{CollectionType::Generic, "generic"}, {CollectionType::Item, "item"}, {CollectionType::Monster, "monster"}}));

struct Collectable {
  Collectable();
  Collectable(String const& name, int order, String const& title, String const& description, String const& icon);

  String name;
  int order;
  String title;
  String description;
  String icon;
};

struct Collection {
  Collection();
  Collection(String const& name, CollectionType type, String const& icon);

  String name;
  String title;
  CollectionType type;
};

class CollectionDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<CollectionDatabase>, StarException> create(Root& root);

  List<Collection> collections() const;
  [[nodiscard]] std::expected<Collection, StarException> collection(String const& collectionName) const;
  [[nodiscard]] std::expected<List<Collectable>, StarException> collectables(String const& collectionName) const;
  [[nodiscard]] std::expected<List<Collectable>, StarException> collectables(String const& collectionName, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;
  [[nodiscard]] std::expected<Collectable, StarException> collectable(String const& collectionName, String const& collectableName) const;
  [[nodiscard]] std::expected<Collectable, StarException> collectable(String const& collectionName, String const& collectableName, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  bool hasCollectable(String const& collectionName, String const& collectableName) const;

private:
  explicit CollectionDatabase(Root& root);
  [[nodiscard]] std::expected<Collectable, StarException> parseGenericCollectable(String const& name, Json const& config) const;
  [[nodiscard]] std::expected<Collectable, StarException> parseMonsterCollectable(String const& name, Json const& config) const;
  [[nodiscard]] std::expected<Collectable, StarException> parseItemCollectable(String const& name, Json const& config, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  Root& m_root;
  StringMap<Collection> m_collections;
  StringMap<StringMap<Json>> m_collectableConfigs;
  mutable std::mutex m_collectablesMutex;
  mutable StringMap<StringMap<Collectable>> m_collectables;
};


}

export module star.collection_database;

export namespace Star {
  using ::Star::CollectionDatabaseExceptionTag;
  using ::Star::CollectionDatabaseException;
  using ::Star::CollectionDatabase;
  using ::Star::CollectionType;
  using ::Star::CollectionTypeNames;
  using ::Star::Collectable;
  using ::Star::Collection;
}
