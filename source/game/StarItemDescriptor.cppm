module;
import std;
import star.format;
import star.text_formatter;
import star.string;
import star.hash;
import star.json;

import star.data_stream;

namespace Star {

class Item;
class VersioningDatabase;

class ItemDescriptor {
public:
  [[nodiscard]] static std::expected<ItemDescriptor, StarException> load_store(VersioningDatabase const& versioning_database, Json const& store);

  ItemDescriptor();
  [[nodiscard]]
  static std::expected<ItemDescriptor, StarException> create(String name, std::uint64_t count, Json parameters = Json());

  // Populate from a configuration JsonArray containing up to 3 elements, the
  // name, count, and then any item parameters.  If the json is a map, looks
  // for keys 'name', 'parameters', and 'count'.
  [[nodiscard]]
  static std::expected<ItemDescriptor, StarException> create(Json const& spec);

  String const& name() const&;
  String name() &&;
  String name() const&& = delete;
  std::uint64_t count() const;
  Json const& parameters() const&;
  Json parameters() &&;
  Json parameters() const&& = delete;

  ItemDescriptor singular() const;
  ItemDescriptor with_count(std::uint64_t count) const&;
  ItemDescriptor with_count(std::uint64_t count) &&;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> apply_parameters(JsonObject const& overrides) const;

  // Nullness depends on the name, not the stack count.
  bool is_null() const;

  // Descriptor is not null
  explicit operator bool() const;

  // True if descriptor is null OR if descriptor is size 0
  bool is_empty() const;

  bool operator==(ItemDescriptor const&) const = default;

  bool matches(ItemDescriptor const& other, bool exact_match = false) const;
  bool matches(Item const& other, bool exact_match = false) const;

  // Stores ItemDescriptor to versioned structure not meant for human reading / writing.
  [[nodiscard]]
  std::expected<Json, StarException> disk_store(VersioningDatabase const& versioning_database) const;

  // Converts ItemDescriptor to spec format
  Json to_json() const;

  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& descriptor);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& descriptor);

private:
  friend class Item;
  ItemDescriptor(String name, std::uint64_t count, Json parameters);
  Json fields() const;

  String m_name;
  std::uint64_t m_count;
  Json m_parameters;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& descriptor);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& descriptor);
[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, ItemDescriptor const& descriptor);

template <>
struct ValueHash<ItemDescriptor> {
  std::size_t operator()(ItemDescriptor const& v) const;
};

}

export module star.item_descriptor;

export namespace Star {
  template <>
  struct ValueHash<ItemDescriptor>;
  using ::Star::ItemDescriptor;
  using ::Star::write_text;
  using ::Star::Item;
}
