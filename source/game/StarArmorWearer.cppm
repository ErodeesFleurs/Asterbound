module;
import star.format;
import std;
import star.list;
import star.json;

import star.array;
import star.game_types;
import star.data_stream;
import star.bi_map;
import star.item_descriptor;
import star.strong_typedef;
import star.id_map;
import star.status_types;


import star.net_element_basic_fields;
import star.net_element_sync_group;

namespace Star {

class Root;

class Humanoid;

class ArmorItem;
class HeadArmor;
class ChestArmor;
class LegsArmor;
class BackArmor;
class ToolUserEntity;
class Item;
class World;
class EffectEmitter;
class ToolUser;
class StatusController;

class ArmorWearer : public NetElementSyncGroup {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  ArmorWearer(ConstructionKey, Root& root);
  [[nodiscard]] static std::expected<std::shared_ptr<ArmorWearer>, StarException> create(Root& root);

  // returns true if movement parameters changed
  [[nodiscard]] std::expected<bool, StarException> setup_humanoid(Humanoid& humanoid, bool force_nude);
  [[nodiscard]] std::expected<void, StarException> effects(EffectEmitter& effect_emitter) const;
  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> status_effects(bool cosmetic_only = false) const;

  void reset();

  [[nodiscard]] std::expected<Json, StarException> disk_store() const;
  [[nodiscard]] std::expected<void, StarException> disk_load(Json const& store);

  bool set_item(std::size_t slot, std::shared_ptr<ArmorItem> item, bool visible = true);
  [[nodiscard]] ItemDescriptor item_descriptor(std::size_t slot) const;

  // Relative slots 0–11 correspond to physical slots 8–19.
  bool set_cosmetic_item(std::size_t slot, std::shared_ptr<ArmorItem> item);

private:
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool full) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  struct Armor {
    std::shared_ptr<ArmorItem> item;
    bool visible = true;
    bool needs_sync = true;
    bool needs_store = true;
    NetElementData<ItemDescriptor> net_state;
  };

  static constexpr std::uint8_t cosmetic_begin = 4;
  static constexpr std::uint8_t extra_cosmetic_begin = 8;
  static constexpr std::uint8_t slot_count = 20;
  static constexpr std::uint32_t all_slots = (std::uint32_t{1} << slot_count) - 1;

  Array<Armor, slot_count> m_armors;
  Array<std::uint8_t, 4> m_worn_cosmetic_types;
  // Derived from equipment; pending writes target the wearer's single Humanoid.
  std::uint32_t m_visible_slots = 0;
  std::uint32_t m_flipping_slots = 0;
  std::uint32_t m_pending_wearables = all_slots;
  bool m_appearance_dirty = true;
  bool m_config_dirty = true;
  Gender m_last_gender{};
  Direction m_last_direction{};
  bool m_last_nude = false;
};

// Commit armor before querying tools; failures preserve earlier category updates.
[[nodiscard]] std::expected<void, StarException> synchronize_equipment_status_effects(
    ArmorWearer const& armor, ToolUser const& tools, StatusController& status_controller, bool cosmetic_only = false);

}

export module star.armor_wearer;

export namespace Star {
  using ::Star::ArmorItem;
  using ::Star::HeadArmor;
  using ::Star::ChestArmor;
  using ::Star::LegsArmor;
  using ::Star::BackArmor;
  using ::Star::ToolUserEntity;
  using ::Star::Item;
  using ::Star::World;
  using ::Star::EffectEmitter;
  using ::Star::ArmorWearer;
  using ::Star::synchronize_equipment_status_effects;
}
