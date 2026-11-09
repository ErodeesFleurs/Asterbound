import star.hash;
import star.text_formatter;
import star.json;
import star.data_stream;
import star.bi_map;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.game_types;
import star.strong_typedef;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;

import star.item_descriptor;
import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;

import star.versioning_database;

namespace Star {

ItemDescriptor::ItemDescriptor() : m_count(0), m_parameters(JsonObject()) {}

std::expected<ItemDescriptor, StarException> ItemDescriptor::create(String name, std::uint64_t count, Json parameters) {
  if (parameters.isNull())
    parameters = JsonObject();
  if (!parameters.isType(Json::Type::Object))
    return std::unexpected(StarException("Item parameters not map in ItemDescriptor constructor"));
  return ItemDescriptor(std::move(name), count, std::move(parameters), {});
}

ItemDescriptor::ItemDescriptor(ItemDescriptor&& rhs) noexcept(std::is_nothrow_move_constructible_v<String> && std::is_nothrow_move_constructible_v<Json>)
  : m_name(std::move(rhs.m_name)), m_count(rhs.m_count), m_parameters(std::move(rhs.m_parameters)),
    m_parametersHash(rhs.m_parametersHash) {
  rhs.m_parametersHash.reset();
}

ItemDescriptor& ItemDescriptor::operator=(ItemDescriptor&& rhs) noexcept(std::is_nothrow_move_assignable_v<String> && std::is_nothrow_move_assignable_v<Json>) {
  if (this != &rhs) {
    m_name = std::move(rhs.m_name);
    m_count = rhs.m_count;
    m_parameters = std::move(rhs.m_parameters);
    m_parametersHash = rhs.m_parametersHash;
    rhs.m_parametersHash.reset();
  }
  return *this;
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::create(Json const& spec) {
  ItemDescriptor descriptor;
  if (spec.type() == Json::Type::Array) {
    auto name = spec.getString(0);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto count = spec.getInt(1, 1);
    if (!count)
      return std::unexpected(std::move(count.error()));
    auto parameters = spec.getObject(2, {});
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    descriptor.m_name = std::move(*name);
    descriptor.m_count = *count;
    descriptor.m_parameters = std::move(*parameters);
  } else if (spec.type() == Json::Type::Object) {
    auto hasName = spec.contains("name");
    if (!hasName)
      return std::unexpected(std::move(hasName.error()));
    auto hasItem = spec.contains("item");
    if (!hasItem)
      return std::unexpected(std::move(hasItem.error()));
    if (!*hasName && !*hasItem)
      return std::unexpected(StarException("Item name missing."));
    auto name = spec.getString(*hasName ? "name" : "item");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto count = spec.getInt("count", 1);
    if (!count)
      return std::unexpected(std::move(count.error()));
    auto data = spec.getObject("data", JsonObject{});
    if (!data)
      return std::unexpected(std::move(data.error()));
    auto parameters = spec.getObject("parameters", std::move(*data));
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    descriptor.m_name = std::move(*name);
    descriptor.m_count = *count;
    descriptor.m_parameters = std::move(*parameters);
  } else if (spec.type() == Json::Type::String) {
    auto name = spec.toString();
    if (!name)
      return std::unexpected(std::move(name.error()));
    descriptor.m_name = std::move(*name);
    descriptor.m_count = 1;
  } else if (!spec.isNull()) {
    return std::unexpected(ItemException("ItemDescriptor spec variant not list, map, string, or null"));
  }
  return descriptor;
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::loadStore(VersioningDatabase const& versioningDatabase, Json const& spec) {
  auto versioned = VersionedJson::fromJson(spec);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  auto content = versioningDatabase.loadVersionedJson(*versioned, "Item");
  if (!content)
    return std::unexpected(std::move(content.error()));
  return create(*content);
}

String const& ItemDescriptor::name() const {
  return m_name;
}

std::uint64_t ItemDescriptor::count() const {
  return m_count;
}

Json const& ItemDescriptor::parameters() const {
  return m_parameters;
}

ItemDescriptor ItemDescriptor::singular() const {
  return ItemDescriptor(name(), 1, parameters(), m_parametersHash);
}

ItemDescriptor ItemDescriptor::withCount(std::uint64_t count) const {
  return ItemDescriptor(name(), count, parameters(), m_parametersHash);
}

ItemDescriptor ItemDescriptor::multiply(std::uint64_t count) const {
  return ItemDescriptor(name(), this->count() * count, parameters(), m_parametersHash);
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::applyParameters(JsonObject const& parameters) const {
  auto merged = this->parameters().setAll(parameters);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  return create(name(), count(), std::move(*merged));
}

bool ItemDescriptor::isNull() const {
  return m_name.empty();
}

ItemDescriptor::operator bool() const {
  return !isNull();
}

bool ItemDescriptor::isEmpty() const {
  return m_name.empty() || m_count == 0;
}

bool ItemDescriptor::operator==(ItemDescriptor const& rhs) const {
  return std::tie(m_name, m_count, m_parameters) == std::tie(rhs.m_name, rhs.m_count, rhs.m_parameters);
}

bool ItemDescriptor::operator!=(ItemDescriptor const& rhs) const {
  return std::tie(m_name, m_count, m_parameters) != std::tie(rhs.m_name, rhs.m_count, rhs.m_parameters);
}

bool ItemDescriptor::matches(ItemDescriptor const& other, bool exactMatch) const {
  return other.name() == m_name && (!exactMatch || other.parameters() == m_parameters);
}

bool ItemDescriptor::matches(Item const& other, bool exactMatch) const {
  return other.name() == m_name && (!exactMatch || other.parameters() == m_parameters);
}

std::expected<Json, StarException> ItemDescriptor::diskStore(VersioningDatabase const& versioningDatabase) const {
  auto res = JsonObject{
      {"name", m_name}, {"count", m_count}, {"parameters", m_parameters},
  };
  auto versioned = versioningDatabase.makeCurrentVersionedJson("Item", res);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  return versioned->toJson();
}

Json ItemDescriptor::toJson() const {
  if (isNull()) {
    return Json();
  } else {
    return JsonObject{
        {"name", m_name}, {"count", m_count}, {"parameters", m_parameters},
    };
  }
}

ItemDescriptor::ItemDescriptor(String name, std::uint64_t count, Json parameters, std::optional<std::size_t> parametersHash)
  : m_name(std::move(name)), m_count(count), m_parameters(std::move(parameters)), m_parametersHash(parametersHash) {}

std::size_t ItemDescriptor::parametersHash() const {
  if (!m_parametersHash)
    m_parametersHash = hash<Json>()(m_parameters);
  return *m_parametersHash;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& itemDescriptor) {
  auto name = ds.read(itemDescriptor.m_name);
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto count = ds.read_vlq_u(itemDescriptor.m_count);
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto parameters = ds.read(itemDescriptor.m_parameters);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  itemDescriptor.m_parametersHash.reset();
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& itemDescriptor) {
  auto name = ds.write(itemDescriptor.m_name);
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto count = ds.write_vlq_u(itemDescriptor.m_count);
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto parameters = ds.write(itemDescriptor.m_parameters);
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return std::ref(ds);
}

std::expected<void, StarException> write_text(TextBuffer& output, ItemDescriptor const& descriptor) {
  auto parameters = descriptor.m_parameters.repr();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  output.push_back('[');
  append_text(output, descriptor.m_name);
  output.append(", ");
  append_text(output, descriptor.m_count);
  output.append(", ");
  append_text(output, *parameters);
  output.push_back(']');
  return {};
}

std::size_t hash<ItemDescriptor>::operator()(ItemDescriptor const& v) const {
  return hash_of(v.m_name, v.m_count, v.m_parametersHash);
}

}
