module;
import std;
import star.format;
import star.type_identity;
import star.json;
import star.id_map;
import star.game_types;
import star.strong_typedef;
import star.data_stream;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.bi_map;
import star.rpc_promise;
import star.a_star;
import star.set;

namespace Star {
class ArmorItem;
class HeadArmor;
class ChestArmor;
class LegsArmor;
class BackArmor;
}

import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.status_types;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;
import star.game_timers;
import star.actor_movement_controller;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;

namespace Star {

enum class ArmorType : std::uint8_t {
  Head,
  Chest,
  Legs,
  Back
};
extern std::expected<EnumMap<ArmorType>, StarException> const ArmorTypeNames;


class ArmorItem : public Item, public EffectSourceItem, public SwingableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ArmorItem, Item, EffectSourceItem, SwingableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ArmorItem>();
  }

  ~ArmorItem() override = default;

  [[nodiscard]]
  std::expected<List<PersistentStatusEffect>, StarException> statusEffects() const override;
  bool statusEffectsInCosmeticSlot() const;
  List<PersistentStatusEffect> cosmeticStatusEffects() const;

  [[nodiscard]]
  std::expected<StringSet, StarException> effectSources() const override;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> drawables() const override;

  [[nodiscard]]
  std::expected<float, StarException> getAngle(float aimAngle) override;

  [[nodiscard]]
  std::expected<void, StarException> fire(FireMode mode, bool shifting, bool edgeTriggered) override;
  [[nodiscard]]
  std::expected<void, StarException> fireTriggered() override;

  virtual ArmorType armorType() const = 0;

  List<String> const& colorOptions();

  Directives const& directives(bool flip = false) const;
  bool fullbright() const;
  bool flipping() const;
  bool visible(bool extraCosmetics = false) const;
  HashSet<ArmorType> const& armorTypesToHide();
  bool hideBody() const;
  bool bypassNude() const;

  std::optional<String> const& techModule() const;

protected:
  explicit ArmorItem(Root& root) : Item(root) {}
  [[nodiscard]]
  std::expected<void, StarException> initializeArmor(Json const& config, String const& directory, Json const& data);

private:
  [[nodiscard]]
  std::expected<void, StarException> refreshIconDrawables();
  [[nodiscard]]
  std::expected<void, StarException> refreshStatusEffects();

  List<String> m_colorOptions;
  List<PersistentStatusEffect> m_statusEffects;
  StringSet m_effectSources;
  Directives m_directives;
  std::optional<Directives> m_flipDirectives;
  bool m_hideBody;
  bool m_bypassNude;
  bool m_hideInVanillaSlots;
  bool m_statusEffectsInCosmeticSlot;
  bool m_fullbright;
  List<PersistentStatusEffect> m_cosmeticStatusEffects;
  std::optional<HashSet<ArmorType>> m_armorTypesToHide;
  std::optional<String> m_techModule;
};

class HeadArmor : public ArmorItem, public PreviewableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<HeadArmor, ArmorItem, PreviewableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<HeadArmor>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<HeadArmor>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
  ~HeadArmor() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  ArmorType armorType() const override;

  String const& frameset(Gender gender) const;
  Directives const& maskDirectives() const;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

private:
  explicit HeadArmor(Root& root) : ArmorItem(root) {}
  String m_maleImage;
  String m_femaleImage;
  Directives m_maskDirectives;
};

class ChestArmor : public ArmorItem, public PreviewableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<ChestArmor, ArmorItem, PreviewableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<ChestArmor>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<ChestArmor>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
  ~ChestArmor() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  ArmorType armorType() const override;

  // Will have :run, :normal, :duck, and :portrait
  String const& bodyFrameset(Gender gender) const;
  // Will have :idle[1-5], :duck, :rotation, :walk[1-5], :run[1-5], :jump[1-4],
  // :fall[1-4]
  String const& frontSleeveFrameset(Gender gender) const;
  // Same as FSleeve
  String const& backSleeveFrameset(Gender gender) const;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

private:
  explicit ChestArmor(Root& root) : ArmorItem(root) {}
  String m_maleBodyImage;
  String m_maleFrontSleeveImage;
  String m_maleBackSleeveImage;

  String m_femaleBodyImage;
  String m_femaleFrontSleeveImage;
  String m_femaleBackSleeveImage;
};

class LegsArmor : public ArmorItem, public PreviewableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<LegsArmor, ArmorItem, PreviewableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<LegsArmor>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<LegsArmor>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
  ~LegsArmor() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  ArmorType armorType() const override;

  // Will have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  String const& frameset(Gender gender) const;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

private:
  explicit LegsArmor(Root& root) : ArmorItem(root) {}
  String m_maleImage;
  String m_femaleImage;
};

class BackArmor : public ArmorItem, public PreviewableItem {
public:
  void const* queryType(TypeKey key) const noexcept override {
    return queryTypeOf<BackArmor, ArmorItem, PreviewableItem>(this, key);
  }
  std::string_view dynamicTypeName() const noexcept override {
    return Star::typeName<BackArmor>();
  }

  [[nodiscard]]
  static std::expected<std::shared_ptr<BackArmor>, StarException> create(Root& root, Json const& config, String const& directory, Json const& data);
  ~BackArmor() override = default;

  [[nodiscard]]
  std::expected<std::shared_ptr<Item>, StarException> clone() const override;

  ArmorType armorType() const override;

  // Will have :idle, :duck, :walk[1-8], :run[1-8], :jump[1-4], :fall[1-4]
  String const& frameset(Gender gender) const;

  [[nodiscard]]
  std::expected<List<Drawable>, StarException> preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer = {}) const override;

private:
  explicit BackArmor(Root& root) : ArmorItem(root) {}
  String m_maleImage;
  String m_femaleImage;
};

}


export module star.armors;

export namespace Star {
  using ::Star::ArmorType;
  using ::Star::ArmorTypeNames;
  using ::Star::ArmorItem;
  using ::Star::HeadArmor;
  using ::Star::ChestArmor;
  using ::Star::LegsArmor;
  using ::Star::BackArmor;
}

