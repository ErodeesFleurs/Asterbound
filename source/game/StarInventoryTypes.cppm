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
extern std::expected<EnumMap<EquipmentSlot>, StarException> const EquipmentSlotNames;

using BagSlot = std::pair<String, std::uint8_t>;

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

  [[nodiscard]] std::expected<InventorySlot, StarException> jsonToInventorySlot(Json const& json);
  [[nodiscard]] std::expected<Json, StarException> jsonFromInventorySlot(InventorySlot const& slot);

  [[nodiscard]] std::expected<void, StarException> write_text(TextBuffer& output, InventorySlot const& slot);

  // Special items in the player inventory that are not generally manageable
  enum class EssentialItem : std::uint8_t { BeamAxe = 0, WireTool = 1, PaintTool = 2, InspectionTool = 3 };
  extern std::expected<EnumMap<EssentialItem>, StarException> const EssentialItemNames;

  // A player's action bar is a collection of custom item shortcuts, and special
  // hard coded shortcuts to the essential items.  There is one location selected
  // at a time, which is either an entry on the custom bar, or one of the
  // essential items, or nothing.
  using CustomBarIndex = std::uint8_t;
  using SelectedActionBarLocation = std::variant<std::monostate, CustomBarIndex, EssentialItem>;

  [[nodiscard]] std::expected<SelectedActionBarLocation, StarException>
  jsonToSelectedActionBarLocation(Json const& json);
  [[nodiscard]] std::expected<Json, StarException>
  jsonFromSelectedActionBarLocation(SelectedActionBarLocation const& location);

  inline constexpr std::uint8_t EquipmentSize = 8;
  inline constexpr std::uint8_t EssentialItemCount = 4;
}

export module star.inventory_types;

export namespace Star {
  using ::Star::EquipmentSlot;
  using ::Star::EquipmentSlotNames;
  using ::Star::BagSlot;
  using ::Star::SwapSlot;
  using ::Star::TrashSlot;
  using ::Star::InventorySlot;
  using ::Star::jsonToInventorySlot;
  using ::Star::jsonFromInventorySlot;
  using ::Star::EssentialItem;
  using ::Star::EssentialItemNames;
  using ::Star::CustomBarIndex;
  using ::Star::SelectedActionBarLocation;
  using ::Star::jsonToSelectedActionBarLocation;
  using ::Star::jsonFromSelectedActionBarLocation;
  using ::Star::EquipmentSize;
  using ::Star::EssentialItemCount;
  using ::Star::write_text;
}
