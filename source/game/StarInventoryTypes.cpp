import std;
import star.string;
import star.json;
import star.bi_map;
import star.data_stream;
import star.data_stream_extra;
import star.format;
import star.text_formatter;
import star.inventory_types;

namespace Star {

bool is_reserved_bag_name(String const& name) {
  return name == "equipment" || name == "swap" || name == "trash";
}

std::optional<std::uint8_t> inventory_index(double value) {
  // Inspect the representation so fast-math cannot accept a NaN or flush a fraction to zero.
  static_assert(std::numeric_limits<double>::is_iec559);
  auto bits = std::bit_cast<std::uint64_t>(value);
  if ((bits << 1) == 0) return 0;
  if (bits < std::bit_cast<std::uint64_t>(1.0) || bits > std::bit_cast<std::uint64_t>(255.0)) return {};
  auto fractional_bits = 52 - ((bits >> 52) - 1023);
  if (bits & ((std::uint64_t{1} << fractional_bits) - 1)) return {};
  return static_cast<std::uint8_t>(value);
}

namespace {
std::expected<std::uint8_t, StarException> json_index(Json const& json) {
  auto number = json.to_double();
  if (!number) return std::unexpected(std::move(number.error()));
  if (auto index = inventory_index(*number)) return *index;
  return std::unexpected(JsonException("Inventory index must be an integer between 0 and 255"));
}
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, BagSlot const& slot) {
  return ds << slot.bag_name << slot.index;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, BagSlot& slot) {
  BagSlot decoded;
  auto result = ds >> decoded.bag_name >> decoded.index;
  if (result) slot = std::move(decoded);
  return result;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, CustomBarLink const& link) {
  return ds << link.primary << link.secondary;
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, CustomBarLink& link) {
  CustomBarLink decoded;
  auto result = ds >> decoded.primary >> decoded.secondary;
  if (result) link = std::move(decoded);
  return result;
}

std::expected<InventorySlot, StarException> json_to_inventory_slot(Json const& json) {
  auto type = json.get("type").and_then([](Json const& value) { return value.string_ptr(); });
  if (!type) return std::unexpected(std::move(type.error()));
  if (**type == "swap") return SwapSlot{};
  if (**type == "trash") return TrashSlot{};
  auto location = json.get("location");
  if (!location) return std::unexpected(std::move(location.error()));
  if (**type == "equipment") {
    auto name = location->string_ptr();
    if (!name) return std::unexpected(std::move(name.error()));
    return enum_value<equipment_slot_names>(**name);
  }
  auto index = json_index(*location);
  if (!index) return std::unexpected(std::move(index.error()));
  return BagSlot{**type, *index};
}

[[nodiscard]] std::expected<Json, StarException> json_from_inventory_slot(InventorySlot const& slot) {
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    auto name = enum_name<equipment_slot_names>(std::get<EquipmentSlot>(slot));
    if (!name) return std::unexpected(std::move(name.error()));
    return JsonObject{{"type", "equipment"}, {"location", *name}};
  } else if (std::holds_alternative<SwapSlot>(slot)) {
    return JsonObject{{"type", "swap"}};
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    return JsonObject{{"type", "trash"}};
  } else {
    auto const& bag = std::get<BagSlot>(slot);
    if (is_reserved_bag_name(bag.bag_name)) return std::unexpected(JsonException("Reserved inventory bag name"));
    return JsonObject{{"type", bag.bag_name}, {"location", bag.index}};
  }
}

[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, InventorySlot const& slot) {
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    auto name = enum_name<equipment_slot_names>(std::get<EquipmentSlot>(slot));
    if (!name) return std::unexpected(std::move(name.error()));
    output.append("InventorySlot{type: equipment, location: ");
    output.append(*name);
    output.push_back('}');
  } else if (std::holds_alternative<SwapSlot>(slot)) {
    output.append("InventorySlot{type: swap}");
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    output.append("InventorySlot{type: trash}");
  } else {
    auto const& bag = std::get<BagSlot>(slot);
    output.append("InventorySlot{type: ");
    append_text(output, bag.bag_name);
    output.append(", location: ");
    std::format_to(std::back_inserter(output), "{}", bag.index);
    output.push_back('}');
  }
  return {};
}

[[nodiscard]] std::expected<SelectedActionBarLocation, StarException> json_to_selected_action_bar_location(Json const& json) {
  if (json.is_type(Json::Type::String)) {
    auto name = json.string_ptr();
    if (!name) return std::unexpected(std::move(name.error()));
    auto item = enum_value<essential_item_names>(**name);
    if (!item) return std::unexpected(std::move(item.error()));
    return SelectedActionBarLocation(*item);
  } else if (json.is_null()) {
    return SelectedActionBarLocation();
  } else {
    auto index = json_index(json);
    if (!index) return std::unexpected(std::move(index.error()));
    return SelectedActionBarLocation(*index);
  }
}

[[nodiscard]] std::expected<Json, StarException> json_from_selected_action_bar_location(SelectedActionBarLocation const& location) {
  if (std::holds_alternative<CustomBarIndex>(location))
    return Json(std::get<CustomBarIndex>(location));
  if (std::holds_alternative<EssentialItem>(location)) {
    auto name = enum_name<essential_item_names>(std::get<EssentialItem>(location));
    if (!name) return std::unexpected(std::move(name.error()));
    return Json(*name);
  }
  return Json();
}

}
