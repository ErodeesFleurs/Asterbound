import star.hash;
import star.text_formatter;
import star.json;
import star.data_stream;
import star.string;
import std;
import star.format;

import star.item_descriptor;
import star.item;

import star.versioning_database;

namespace Star {

namespace {

Json const& empty_parameters() {
  static Json const empty = JsonObject{};
  return empty;
}

StarException invalid_parameters(Json const& value) {
  return JsonException::format("Improper conversion to JsonObject from {}", value.type_name());
}

}

ItemDescriptor::ItemDescriptor() : m_count(0), m_parameters(empty_parameters()) {}

std::expected<ItemDescriptor, StarException> ItemDescriptor::create(String name, std::uint64_t count, Json parameters) {
  if (parameters.is_null())
    parameters = empty_parameters();
  if (!parameters.is_type(Json::Type::Object))
    return std::unexpected(StarException("Item parameters not map in ItemDescriptor constructor"));
  return ItemDescriptor(std::move(name), count, std::move(parameters));
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::create(Json const& spec) {
  if (spec.type() == Json::Type::Array) {
    auto name = spec.get_string(0);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto count = spec.get_int(1, 1);
    if (!count)
      return std::unexpected(std::move(count.error()));
    auto parameters = spec.find(2);
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto const& value = *parameters ? (**parameters).get() : empty_parameters();
    if (!value.is_type(Json::Type::Object))
      return std::unexpected(invalid_parameters(value));
    return ItemDescriptor(std::move(*name), *count, value);
  }
  if (spec.type() == Json::Type::Object) {
    auto object = spec.object_ptr();
    if (!object)
      return std::unexpected(std::move(object.error()));
    auto const& entries = **object;
    auto key = entries.find_ref("name");
    if (!key)
      key = entries.find_ref("item");
    if (!key)
      return std::unexpected(StarException("Item name missing."));
    auto name = key->get().to_string();
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto count = spec.get_int("count", 1);
    if (!count)
      return std::unexpected(std::move(count.error()));
    // Validate legacy data even when parameters replaces it. Named nulls use the fallback.
    auto data = entries.find_ref("data");
    if (data && !data->get().is_null() && !data->get().is_type(Json::Type::Object))
      return std::unexpected(invalid_parameters(data->get()));
    auto parameters = entries.find_ref("parameters");
    if (!parameters || parameters->get().is_null())
      parameters = data;
    if (!parameters || parameters->get().is_null())
      return ItemDescriptor(std::move(*name), *count, empty_parameters());
    if (!parameters->get().is_type(Json::Type::Object))
      return std::unexpected(invalid_parameters(parameters->get()));
    return ItemDescriptor(std::move(*name), *count, parameters->get());
  }
  if (spec.type() == Json::Type::String) {
    auto name = spec.to_string();
    if (!name)
      return std::unexpected(std::move(name.error()));
    return ItemDescriptor(std::move(*name), 1, empty_parameters());
  }
  if (spec.is_null())
    return ItemDescriptor();
  return std::unexpected(ItemException("ItemDescriptor spec variant not list, map, string, or null"));
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::load_store(VersioningDatabase const& versioning_database, Json const& spec) {
  auto versioned = VersionedJson::fromJson(spec);
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  auto content = versioning_database.loadVersionedJson(*versioned, "Item");
  if (!content)
    return std::unexpected(std::move(content.error()));
  return create(*content);
}

String const& ItemDescriptor::name() const& {
  return m_name;
}

String ItemDescriptor::name() && {
  return std::move(m_name);
}

std::uint64_t ItemDescriptor::count() const {
  return m_count;
}

Json const& ItemDescriptor::parameters() const& {
  return m_parameters;
}

Json ItemDescriptor::parameters() && {
  return std::move(m_parameters);
}

ItemDescriptor ItemDescriptor::singular() const {
  return with_count(1);
}

ItemDescriptor ItemDescriptor::with_count(std::uint64_t count) const& {
  return ItemDescriptor(m_name, count, m_parameters);
}

ItemDescriptor ItemDescriptor::with_count(std::uint64_t count) && {
  return ItemDescriptor(std::move(m_name), count, std::move(m_parameters));
}

std::expected<ItemDescriptor, StarException> ItemDescriptor::apply_parameters(JsonObject const& overrides) const {
  if (overrides.empty() && m_parameters.is_type(Json::Type::Object))
    return *this;
  auto merged = m_parameters.to_object();
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  for (auto const& [key, value] : overrides)
    (*merged)[key] = value;
  return ItemDescriptor(m_name, m_count, std::move(*merged));
}

bool ItemDescriptor::is_null() const {
  return m_name.empty();
}

ItemDescriptor::operator bool() const {
  return !is_null();
}

bool ItemDescriptor::is_empty() const {
  return m_name.empty() || m_count == 0;
}

bool ItemDescriptor::matches(ItemDescriptor const& other, bool exact_match) const {
  return m_name == other.m_name && (!exact_match || m_parameters == other.m_parameters);
}

bool ItemDescriptor::matches(Item const& other, bool exact_match) const {
  return other.matches(*this, exact_match);
}

Json ItemDescriptor::fields() const {
  return JsonObject{{"name", m_name}, {"count", m_count}, {"parameters", m_parameters}};
}

std::expected<Json, StarException> ItemDescriptor::disk_store(VersioningDatabase const& versioning_database) const {
  auto versioned = versioning_database.makeCurrentVersionedJson("Item", fields());
  if (!versioned)
    return std::unexpected(std::move(versioned.error()));
  return versioned->toJson();
}

Json ItemDescriptor::to_json() const {
  return is_null() ? Json() : fields();
}

ItemDescriptor::ItemDescriptor(String name, std::uint64_t count, Json parameters)
  : m_name(std::move(name)), m_count(count), m_parameters(std::move(parameters)) {}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, ItemDescriptor& descriptor) {
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto count = ds.read_vlq_u();
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto parameters = ds.read<Json>();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  // A fresh legacy descriptor decoded a wire null as empty parameters.
  descriptor = ItemDescriptor(std::move(*name), *count, parameters->is_null() ? empty_parameters() : std::move(*parameters));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, ItemDescriptor const& descriptor) {
  auto name = ds.write(descriptor.name());
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto count = ds.write_vlq_u(descriptor.count());
  if (!count)
    return std::unexpected(std::move(count.error()));
  auto parameters = ds.write(descriptor.parameters());
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return std::ref(ds);
}

std::expected<void, StarException> write_text(TextBuffer& output, ItemDescriptor const& descriptor) {
  auto parameters = descriptor.parameters().repr();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  output.push_back('[');
  append_text(output, descriptor.name());
  output.append(", ");
  append_text(output, descriptor.count());
  output.append(", ");
  append_text(output, *parameters);
  output.push_back(']');
  return {};
}

std::size_t ValueHash<ItemDescriptor>::operator()(ItemDescriptor const& v) const {
  return hash_of(v.name(), v.count(), v.parameters());
}

}
