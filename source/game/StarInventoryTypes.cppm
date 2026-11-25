module;
import std;
import star.format;
import star.text_formatter;
import star.string;
import star.json;

import star.bi_map;
import star.data_stream;

import star.algorithm;
namespace Star {

enum class EquipmentSlot : std::uint8_t {
  Head = 0,
  Chest = 1,
  Legs = 2,
  Back = 3,
  HeadCosmetic = 4,
  ChestCosmetic = 5,
  LegsCosmetic = 6,
  BackCosmetic = 7,
  Cosmetic1,
  Cosmetic2,
  Cosmetic3,
  Cosmetic4,
  Cosmetic5,
  Cosmetic6,
  Cosmetic7,
  Cosmetic8,
  Cosmetic9,
  Cosmetic10,
  Cosmetic11,
  Cosmetic12
};
inline constexpr auto equipment_slot_names = sorted_enum_names(std::to_array<std::pair<EquipmentSlot, std::string_view>>({
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
}));

struct BagSlot {
  String bag_name;
  std::uint8_t index{};
  auto operator<=>(BagSlot const&) const = default;
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, BagSlot const&);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, BagSlot&);
};

bool is_reserved_bag_name(String const& name);
std::optional<std::uint8_t> inventory_index(double value);

struct TrashSlot {
  auto operator<=>(TrashSlot const&) const = default;
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, TrashSlot const&) { return std::ref(ds); }
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, TrashSlot&) { return std::ref(ds); }
};

struct SwapSlot {
  auto operator<=>(SwapSlot const&) const = default;
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, SwapSlot const&) { return std::ref(ds); }
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, SwapSlot&) { return std::ref(ds); }
  friend std::expected<void, StarException> write_text(TextBuffer&, std::variant<EquipmentSlot, BagSlot, SwapSlot, TrashSlot> const&);
};

  // Any manageable location in the player inventory can be pointed to by an
  // InventorySlot
  using InventorySlot = std::variant<EquipmentSlot, BagSlot, SwapSlot, TrashSlot>;

  [[nodiscard]] std::expected<InventorySlot, StarException> json_to_inventory_slot(Json const& json);
  [[nodiscard]] std::expected<Json, StarException> json_from_inventory_slot(InventorySlot const& slot);

  [[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, InventorySlot const& slot);

  struct CustomBarLink {
    std::optional<InventorySlot> primary;
    std::optional<InventorySlot> secondary;
    auto operator<=>(CustomBarLink const&) const = default;
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream&, CustomBarLink const&);
    friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream&, CustomBarLink&);
  };

  // Special items in the player inventory that are not generally manageable
  enum class EssentialItem : std::uint8_t { BeamAxe = 0, WireTool = 1, PaintTool = 2, InspectionTool = 3 };
  inline constexpr auto essential_item_names = sorted_enum_names(std::to_array<std::pair<EssentialItem, std::string_view>>({
  {EssentialItem::BeamAxe, "beamaxe"},
  {EssentialItem::WireTool, "wiretool"},
  {EssentialItem::PaintTool, "painttool"},
  {EssentialItem::InspectionTool, "inspectiontool"}
}));

  // A player's action bar is a collection of custom item shortcuts, and special
  // hard coded shortcuts to the essential items.  There is one location selected
  // at a time, which is either an entry on the custom bar, or one of the
  // essential items, or nothing.
  using CustomBarIndex = std::uint8_t;
  using SelectedActionBarLocation = std::variant<std::monostate, CustomBarIndex, EssentialItem>;

  [[nodiscard]] std::expected<SelectedActionBarLocation, StarException>
  json_to_selected_action_bar_location(Json const& json);
  [[nodiscard]] std::expected<Json, StarException>
  json_from_selected_action_bar_location(SelectedActionBarLocation const& location);

  inline constexpr auto essential_item_count = essential_item_names.size();
}

export module star.inventory_types;

export namespace Star {
  using ::Star::EquipmentSlot;
  using ::Star::equipment_slot_names;
  using ::Star::BagSlot;
  using ::Star::SwapSlot;
  using ::Star::TrashSlot;
  using ::Star::InventorySlot;
  using ::Star::json_to_inventory_slot;
  using ::Star::json_from_inventory_slot;
  using ::Star::EssentialItem;
  using ::Star::essential_item_names;
  using ::Star::CustomBarIndex;
  using ::Star::SelectedActionBarLocation;
  using ::Star::json_to_selected_action_bar_location;
  using ::Star::json_from_selected_action_bar_location;
  using ::Star::CustomBarLink;
  using ::Star::inventory_index;
  using ::Star::is_reserved_bag_name;
  using ::Star::essential_item_count;
  using ::Star::write_text;
}
