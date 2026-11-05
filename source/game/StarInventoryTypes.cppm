module;
import std;
import star.format;
import star.string;
import star.json;

import star.bi_map;
import star.strong_typedef;

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

typedef std::pair<String, std::uint8_t> BagSlot;

// Keep native wrappers so SwapSlot can declare precise stream ADL.
template <typename BaseType> struct TrashSlotWrapper;
  template <typename BaseType>
  struct SwapSlotWrapper : BaseType {
    using BaseType::BaseType;

    SwapSlotWrapper() : BaseType() {}

    SwapSlotWrapper(SwapSlotWrapper const& nt) : BaseType(nt) {}

    SwapSlotWrapper(SwapSlotWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit SwapSlotWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit SwapSlotWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    SwapSlotWrapper& operator=(SwapSlotWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    SwapSlotWrapper& operator=(SwapSlotWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    SwapSlotWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of<BaseType, typename std::decay<Arg>::type>::value == false
              || std::is_same<SwapSlotWrapper, typename std::decay<Arg>::type>::value,
          "SwapSlot can not implicitly be assigned from std::monostate-derived classes or strong std::monostate typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
    friend std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& ostream,
        std::variant<EquipmentSlot, BagSlot, SwapSlotWrapper<std::monostate>, TrashSlotWrapper<std::monostate>> const& slot);
  };
  typedef SwapSlotWrapper<std::monostate> SwapSlot;
template <typename BaseType>
struct TrashSlotWrapper : BaseType {
  using BaseType::BaseType;

  TrashSlotWrapper() : BaseType() {}

  TrashSlotWrapper(TrashSlotWrapper const& nt) : BaseType(nt) {}

  TrashSlotWrapper(TrashSlotWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit TrashSlotWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit TrashSlotWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  TrashSlotWrapper& operator=(TrashSlotWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  TrashSlotWrapper& operator=(TrashSlotWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  TrashSlotWrapper& operator=(Arg&& other) {
    static_assert(std::is_base_of<BaseType, typename std::decay<Arg>::type>::value == false
            || std::is_same<TrashSlotWrapper, typename std::decay<Arg>::type>::value,
        "TrashSlot can not implicitly be assigned from std::monostate-derived classes or strong std::monostate typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
typedef TrashSlotWrapper<std::monostate> TrashSlot;

// Any manageable location in the player inventory can be pointed to by an
// InventorySlot
typedef std::variant<EquipmentSlot, BagSlot, SwapSlot, TrashSlot> InventorySlot;

[[nodiscard]] std::expected<InventorySlot, StarException> jsonToInventorySlot(Json const& json);
[[nodiscard]] std::expected<Json, StarException> jsonFromInventorySlot(InventorySlot const& slot);

[[nodiscard]] std::expected<std::reference_wrapper<std::ostream>, StarException> operator<<(std::ostream& ostream, InventorySlot const& slot);

// Special items in the player inventory that are not generally manageable
enum class EssentialItem : std::uint8_t {
  BeamAxe = 0,
  WireTool = 1,
  PaintTool = 2,
  InspectionTool = 3
};
extern std::expected<EnumMap<EssentialItem>, StarException> const EssentialItemNames;

// A player's action bar is a collection of custom item shortcuts, and special
// hard coded shortcuts to the essential items.  There is one location selected
// at a time, which is either an entry on the custom bar, or one of the
// essential items, or nothing.
typedef std::uint8_t CustomBarIndex;
typedef std::variant<std::monostate, CustomBarIndex, EssentialItem> SelectedActionBarLocation;

[[nodiscard]] std::expected<SelectedActionBarLocation, StarException> jsonToSelectedActionBarLocation(Json const& json);
[[nodiscard]] std::expected<Json, StarException> jsonFromSelectedActionBarLocation(SelectedActionBarLocation const& location);

inline constexpr std::uint8_t EquipmentSize = 8;
inline constexpr std::uint8_t EssentialItemCount = 4;

}

export module star.inventory_types;

export namespace Star {
  using ::Star::EquipmentSlot;
  using ::Star::EquipmentSlotNames;
  using ::Star::BagSlot;
  using ::Star::SwapSlotWrapper;
  using ::Star::SwapSlot;
  using ::Star::TrashSlotWrapper;
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
}
