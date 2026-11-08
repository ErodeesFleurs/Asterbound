module;
import std;
import star.format;
import star.text_formatter;
import star.string;
import star.hash;
import star.json;

import star.data_stream;
import star.bi_map;

namespace Star {

class Item;
struct VersionedJson;
class VersioningDatabase;

class ItemDescriptor {
public:
  // Loads ItemDescriptor from store format.
  [[nodiscard]]
  static std::expected<ItemDescriptor, StarException> loadStore(VersioningDatabase const& versioningDatabase, Json const& store);

  ItemDescriptor();
  [[nodiscard]]
  static std::expected<ItemDescriptor, StarException> create(String name, std::uint64_t count, Json parameters = Json());
  ItemDescriptor(ItemDescriptor const&) = default;
  ItemDescriptor& operator=(ItemDescriptor const&) = default;
  ItemDescriptor(ItemDescriptor&& rhs) noexcept(std::is_nothrow_move_constructible_v<String> && std::is_nothrow_move_constructible_v<Json>);
  ItemDescriptor& operator=(ItemDescriptor&& rhs) noexcept(std::is_nothrow_move_assignable_v<String> && std::is_nothrow_move_assignable_v<Json>);

  // Populate from a configuration JsonArray containing up to 3 elements, the
  // name, count, and then any item parameters.  If the json is a map, looks
  // for keys 'name', 'parameters', and 'count'.
  [[nodiscard]]
  static std::expected<ItemDescriptor, StarException> create(Json const& spec);

  String const& name() const;
  std::uint64_t count() const;
  Json const& parameters() const;

  ItemDescriptor singular() const;
  ItemDescriptor withCount(std::uint64_t count) const;
  ItemDescriptor multiply(std::uint64_t count) const;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> applyParameters(JsonObject const& parameters) const;

  // Descriptor is the default constructed ItemDescriptor()
  bool isNull() const;

  // Descriptor is not null
  explicit operator bool() const;

  // True if descriptor is null OR if descriptor is size 0
  bool isEmpty() const;

  bool operator==(ItemDescriptor const& rhs) const;
  bool operator!=(ItemDescriptor const& rhs) const;

  bool matches(ItemDescriptor const& other, bool exactMatch = false) const;
  bool matches(Item const& other, bool exactMatch = false) const;

  // Stores ItemDescriptor to versioned structure not meant for human reading / writing.
  [[nodiscard]]
  std::expected<Json, StarException> diskStore(VersioningDatabase const& versioningDatabase) const;

  // Converts ItemDescriptor to spec format
  Json toJson() const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& itemDescriptor);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& itemDescriptor);

  friend std::expected<void, StarException> write_text(TextBuffer& output, ItemDescriptor const& descriptor);

  friend struct hash<ItemDescriptor>;

private:
  friend class Item;
  friend class ItemDatabase;
  ItemDescriptor(String name, std::uint64_t count, Json parameters, std::optional<std::size_t> parametersHash);

  std::size_t parametersHash() const;

  String m_name;
  std::uint64_t m_count;
  Json m_parameters;
  mutable std::optional<std::size_t> m_parametersHash;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& itemDescriptor);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& itemDescriptor);
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, ItemDescriptor const& descriptor);

template <>
struct hash<ItemDescriptor> {
  std::size_t operator()(ItemDescriptor const& v) const;
};

}

export module star.item_descriptor;

export namespace Star {
  template <>
  struct hash<ItemDescriptor>;
}



export namespace Star {
  using ::Star::ItemDescriptor;
  using ::Star::write_text;
  using ::Star::Item;
  using ::Star::VersionedJson;
}
