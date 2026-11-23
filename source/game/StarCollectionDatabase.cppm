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
inline constexpr auto collection_type_names = sorted_enum_names(std::to_array<std::pair<CollectionType, std::string_view>>(
  {{CollectionType::Generic, "generic"}, {CollectionType::Item, "item"}, {CollectionType::Monster, "monster"}}));

struct Collectable {
  String name;
  int order = 0;
  String title;
  String description;
  String icon;
};

struct Collection {
  String name;
  String title;
  CollectionType type = CollectionType::Generic;
};

class CollectionDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };
  using ItemCatalog = std::optional<std::reference_wrapper<ItemDatabase const>>;
  struct Slot {
    Json config;
    mutable std::optional<Collectable> published;
  };
  struct LazyEntries {
    String source;
    StringMap<Slot> slots;
    mutable std::mutex mutex;
    mutable std::size_t ready = 0;
  };
  struct Group {
    Collection collection;
    std::variant<StringMap<Collectable>, std::unique_ptr<LazyEntries>> entries;
  };
public:
  CollectionDatabase(ConstructionKey, Root& root) : m_root(root) {}
  [[nodiscard]] static std::expected<std::shared_ptr<CollectionDatabase>, StarException> create(Root& root);
  List<Collection> collections() const;
  [[nodiscard]] std::expected<Collection, StarException> collection(String const& name) const;
  [[nodiscard]] std::expected<List<Collectable>, StarException> collectables(String const& name, ItemCatalog catalog = {}) const;
  [[nodiscard]] std::expected<Collectable, StarException> collectable(String const& name, String const& id, ItemCatalog catalog = {}) const;
  bool contains(String const& name, String const& id) const;

private:
  std::expected<Collectable, StarException> resolve(Collection const& collection, LazyEntries const& entries, String const& id, Slot const& slot, ItemCatalog catalog) const;
  std::expected<Collectable, StarException> parse_monster(String const& name, Json const& config) const;
  std::expected<Collectable, StarException> parse_item(String const& name, Json const& config, ItemCatalog catalog) const;
  Root& m_root;
  StringMap<Group> m_groups;
};


}

export module star.collection_database;

export namespace Star {
  using ::Star::CollectionDatabaseExceptionTag;
  using ::Star::CollectionDatabaseException;
  using ::Star::CollectionDatabase;
  using ::Star::CollectionType;
  using ::Star::collection_type_names;
  using ::Star::Collectable;
  using ::Star::Collection;
}
