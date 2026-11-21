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
public:
  [[nodiscard]] static std::expected<std::shared_ptr<ArmorWearer>, StarException> create(Root& root);

  // returns true if movement parameters changed
  [[nodiscard]] std::expected<bool, StarException> setupHumanoid(Humanoid& humanoid, bool forceNude);
  [[nodiscard]] std::expected<void, StarException> effects(EffectEmitter& effectEmitter);
  [[nodiscard]] std::expected<List<PersistentStatusEffect>, StarException> statusEffects(bool cosmeticOnly = false) const;

  void reset();

  [[nodiscard]] std::expected<Json, StarException> diskStore() const;
  [[nodiscard]] std::expected<void, StarException> diskLoad(Json const& diskStore);

  bool setItem(std::uint8_t slot, std::shared_ptr<ArmorItem> item, bool visible = true);
  void setHeadItem(std::shared_ptr<HeadArmor> headItem);
  void setChestItem(std::shared_ptr<ChestArmor> chestItem);
  void setLegsItem(std::shared_ptr<LegsArmor> legsItem);
  void setBackItem(std::shared_ptr<BackArmor> backItem);
  void setHeadCosmeticItem(std::shared_ptr<HeadArmor> headCosmeticItem);
  void setChestCosmeticItem(std::shared_ptr<ChestArmor> chestCosmeticItem);
  void setLegsCosmeticItem(std::shared_ptr<LegsArmor> legsCosmeticItem);
  void setBackCosmeticItem(std::shared_ptr<BackArmor> backCosmeticItem);

  std::shared_ptr<ArmorItem> item(std::uint8_t slot) const;
  std::shared_ptr<HeadArmor> headItem() const;
  std::shared_ptr<ChestArmor> chestItem() const;
  std::shared_ptr<LegsArmor> legsItem() const;
  std::shared_ptr<BackArmor> backItem() const;
  std::shared_ptr<HeadArmor> headCosmeticItem() const;
  std::shared_ptr<ChestArmor> chestCosmeticItem() const;
  std::shared_ptr<LegsArmor> legsCosmeticItem() const;
  std::shared_ptr<BackArmor> backCosmeticItem() const;

  ItemDescriptor itemDescriptor(std::uint8_t slot) const;
  ItemDescriptor headItemDescriptor() const;
  ItemDescriptor chestItemDescriptor() const;
  ItemDescriptor legsItemDescriptor() const;
  ItemDescriptor backItemDescriptor() const;
  ItemDescriptor headCosmeticItemDescriptor() const;
  ItemDescriptor chestCosmeticItemDescriptor() const;
  ItemDescriptor legsCosmeticItemDescriptor() const;
  ItemDescriptor backCosmeticItemDescriptor() const;

  // slot is automatically offset
  bool setCosmeticItem(std::uint8_t slot, std::shared_ptr<ArmorItem> cosmeticItem);
  std::shared_ptr<ArmorItem> cosmeticItem(std::uint8_t slot) const;
  ItemDescriptor cosmeticItemDescriptor(std::uint8_t slot) const;
private:
  explicit ArmorWearer(Root& root);
  Root& m_root;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_load(bool full) override;
  [[nodiscard]] std::expected<void, StarException> net_elements_need_store() override;

  struct Armor {
    std::shared_ptr<ArmorItem> item;
    bool visible = true;
    bool needsSync = true;
    bool needsStore = true;
    bool isCosmetic = false;
    bool isCurrentlyVisible = false;
    NetElementData<ItemDescriptor> netState;
  };

  Array<Armor, 20> m_armors;
  Array<std::uint8_t, 4> m_wornCosmeticTypes;
  // only works under the assumption that this ArmorWearer
  // will only ever touch one Humanoid (which is true!)
  std::optional<Gender> m_lastGender;
  std::optional<Direction> m_lastDirection;
  bool m_lastNude;
};

// Commit armor before querying tools; failures preserve earlier category updates.
[[nodiscard]] std::expected<void, StarException> synchronizeEquipmentStatusEffects(
    ArmorWearer const& armor, ToolUser const& tools, StatusController& statusController, bool cosmeticOnly = false);

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
  using ::Star::synchronizeEquipmentStatusEffects;
}
