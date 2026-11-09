import std;
import star.string;
import star.json;
import star.bi_map;
import star.strong_typedef;
import star.format;
import star.text_formatter;
import star.json_extra;
import star.inventory_types;

namespace Star {

extern std::expected<EnumMap<EquipmentSlot>, StarException> const EquipmentSlotNames = EnumMap<EquipmentSlot>::create({
  {EquipmentSlot::Head, "head"},
  {EquipmentSlot::Chest, "chest"},
  {EquipmentSlot::Legs, "legs"},
  {EquipmentSlot::Back, "back"},
  {EquipmentSlot::HeadCosmetic, "headCosmetic"},
  {EquipmentSlot::ChestCosmetic, "chestCosmetic"},
  {EquipmentSlot::LegsCosmetic, "legsCosmetic"},
  {EquipmentSlot::BackCosmetic, "backCosmetic"},
  {EquipmentSlot::Cosmetic1, "cosmetic1"},
  {EquipmentSlot::Cosmetic2, "cosmetic2"},
  {EquipmentSlot::Cosmetic3, "cosmetic3"},
  {EquipmentSlot::Cosmetic4, "cosmetic4"},
  {EquipmentSlot::Cosmetic5, "cosmetic5"},
  {EquipmentSlot::Cosmetic6, "cosmetic6"},
  {EquipmentSlot::Cosmetic7, "cosmetic7"},
  {EquipmentSlot::Cosmetic8, "cosmetic8"},
  {EquipmentSlot::Cosmetic9, "cosmetic9"},
  {EquipmentSlot::Cosmetic10, "cosmetic10"},
  {EquipmentSlot::Cosmetic11, "cosmetic11"},
  {EquipmentSlot::Cosmetic12, "cosmetic12"}
});

[[nodiscard]] std::expected<InventorySlot, StarException> jsonToInventorySlot(Json const& json) {
  auto type = json.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  auto location = json.get("location", Json());
  if (!location) return std::unexpected(std::move(location.error()));
  if (*type == "equipment") {
    auto name = location->toString();
    if (!name) return std::unexpected(std::move(name.error()));
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto slot = EquipmentSlotNames->get_left(*name);
    if (!slot) return std::unexpected(std::move(slot.error()));
    return InventorySlot(slot->get());
  } else if (*type == "swap") {
    return InventorySlot(SwapSlot());
  } else if (*type == "trash") {
    return InventorySlot(TrashSlot());
  } else {
    auto index = location->toUInt();
    if (!index) return std::unexpected(std::move(index.error()));
    return InventorySlot(BagSlot(*type, *index));
  }
}

[[nodiscard]] std::expected<Json, StarException> jsonFromInventorySlot(InventorySlot const& slot) {
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto name = EquipmentSlotNames->get_right(std::get<EquipmentSlot>(slot));
    if (!name) return std::unexpected(std::move(name.error()));
    return JsonObject{{"type", "equipment"}, {"location", name->get()}};
  } else if (std::holds_alternative<SwapSlot>(slot)) {
    return JsonObject{{"type", "swap"}};
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    return JsonObject{{"type", "trash"}};
  } else {
    auto const& bag = std::get<BagSlot>(slot);
    return JsonObject{{"type", bag.first}, {"location", bag.second}};
  }
}

[[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, InventorySlot const& slot) {
  if (std::holds_alternative<EquipmentSlot>(slot)) {
    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto name = EquipmentSlotNames->get_right(std::get<EquipmentSlot>(slot));
    if (!name) return std::unexpected(std::move(name.error()));
    output.append("InventorySlot{type: equipment, location: ");
    append_text(output, name->get());
    output.push_back('}');
  } else if (std::holds_alternative<SwapSlot>(slot)) {
    output.append("InventorySlot{type: swap}");
  } else if (std::holds_alternative<TrashSlot>(slot)) {
    output.append("InventorySlot{type: trash}");
  } else {
    auto const& bag = std::get<BagSlot>(slot);
    output.append("InventorySlot{type: ");
    append_text(output, bag.first);
    output.append(", location: ");
    std::format_to(std::back_inserter(output), "{}", bag.second);
    output.push_back('}');
  }
  return {};
}

extern std::expected<EnumMap<EssentialItem>, StarException> const EssentialItemNames = EnumMap<EssentialItem>::create({
  {EssentialItem::BeamAxe, "beamaxe"},
  {EssentialItem::WireTool, "wiretool"},
  {EssentialItem::PaintTool, "painttool"},
  {EssentialItem::InspectionTool, "inspectiontool"}
});

[[nodiscard]] std::expected<SelectedActionBarLocation, StarException> jsonToSelectedActionBarLocation(Json const& json) {
  if (json.isType(Json::Type::String)) {
    auto name = json.toString();
    if (!name) return std::unexpected(std::move(name.error()));
    if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto item = EssentialItemNames->get_left(*name);
    if (!item) return std::unexpected(std::move(item.error()));
    return SelectedActionBarLocation(item->get());
  } else if (json.isNull()) {
    return SelectedActionBarLocation();
  } else {
    auto index = json.toUInt();
    if (!index) return std::unexpected(std::move(index.error()));
    return SelectedActionBarLocation(static_cast<CustomBarIndex>(*index));
  }
}

[[nodiscard]] std::expected<Json, StarException> jsonFromSelectedActionBarLocation(SelectedActionBarLocation const& location) {
  if (std::holds_alternative<CustomBarIndex>(location))
    return Json(std::get<CustomBarIndex>(location));
  if (std::holds_alternative<EssentialItem>(location)) {
    if (!EssentialItemNames) return std::unexpected(EssentialItemNames.error());
    auto name = EssentialItemNames->get_right(std::get<EssentialItem>(location));
    if (!name) return std::unexpected(std::move(name.error()));
    return Json(name->get());
  }
  return Json();
}

}
