module;
import std;
import star.string;
import star.map;
import star.format;
import star.json;

import star.color;
import star.bi_map;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.vector;
import star.tile_damage;
import star.list;
import star.hash;
import star.material_render_profile;

namespace Star {

enum class CollisionKind : std::uint8_t;
class ItemDescriptor;

class ParticleConfig;

struct MaterialExceptionTag {
  static constexpr std::string_view name() { return "MaterialException"; }
};
using MaterialException = StarError<MaterialExceptionTag, StarException>;

struct LiquidMaterialInteraction {
  float consume_liquid;
  MaterialId transform_to;
  bool top_only;
};

struct LiquidModInteraction {
  float consume_liquid;
  ModId transform_to;
  bool top_only;
};

struct MaterialAsset {
  String path;
  Json config;
};

class Assets;
class ParticleDatabase;
class Logger;
class RandomService;
class ImageMetadataDatabase;

class MaterialDatabase {
  struct ConstructionKey { explicit ConstructionKey() = default; };

public:
  explicit MaterialDatabase(ConstructionKey, RandomService& random) : m_random(random) {}

  [[nodiscard]]
  static std::expected<std::shared_ptr<MaterialDatabase>, StarException> create(
      Assets const& assets, ParticleDatabase const& particles, ImageMetadataDatabase const& image_metadata,
      Logger& logger, RandomService& random);

  StringList material_names() const;
  bool is_meta_material_name(String const& name) const;
  bool is_material_name(String const& name) const;
  bool is_valid_material_id(MaterialId id) const;
  [[nodiscard]]
  std::expected<MaterialId, StarException> material_id(String const& name) const;
  [[nodiscard]]
  std::expected<String, StarException> material_name(MaterialId id) const;
  [[nodiscard]]
  std::expected<String, StarException> material_description(MaterialId id, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> material_description(MaterialId id) const;
  [[nodiscard]]
  std::expected<String, StarException> material_short_description(MaterialId id) const;
  [[nodiscard]]
  std::expected<String, StarException> material_category(MaterialId id) const;

  StringList mod_names() const;
  bool is_mod_name(String const& name) const;
  bool is_valid_mod_id(ModId id) const;
  [[nodiscard]]
  std::expected<ModId, StarException> mod_id(String const& name) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<String const>, StarException> mod_name(ModId id) const;
  [[nodiscard]]
  std::expected<String, StarException> mod_description(ModId id, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> mod_description(ModId id) const;
  [[nodiscard]]
  std::expected<String, StarException> mod_short_description(ModId id) const;

  // These references borrow from this immutable database. Meta-range IDs have
  // no asset, while an unregistered real ID is an error.
  [[nodiscard]]
  std::expected<std::optional<std::reference_wrapper<MaterialAsset const>>, StarException> material_asset(MaterialId id) const;
  [[nodiscard]]
  std::expected<std::optional<std::reference_wrapper<MaterialAsset const>>, StarException> mod_asset(ModId id) const;

  // Missing IDs and records without a rendering profile return no profile.
  std::optional<std::reference_wrapper<MaterialRenderProfile const>> material_render_profile(MaterialId id) const;
  std::optional<std::reference_wrapper<MaterialRenderProfile const>> mod_render_profile(ModId id) const;

  [[nodiscard]]
  std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> material_damage_parameters(MaterialId id) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<TileDamageParameters const>, StarException> mod_damage_parameters(ModId id) const;

  [[nodiscard]]
  std::expected<bool, StarException> mod_breaks_with_tile(ModId id) const;

  [[nodiscard]]
  std::expected<CollisionKind, StarException> material_collision_kind(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> can_place_in_layer(MaterialId id, TileLayer layer) const;

  // Returned ItemDescriptor may be null.
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> material_item_drop(MaterialId id) const;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> mod_item_drop(ModId id) const;

  [[nodiscard]]
  std::expected<MaterialColorVariant, StarException> material_color_variants(MaterialId id) const;
  [[nodiscard]]
  std::expected<MaterialColorVariant, StarException> mod_color_variants(ModId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> is_multi_color(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> foreground_light_transparent(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> background_light_transparent(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> occludes_behind(MaterialId id) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<ParticleConfig>, StarException> mining_particle(MaterialId material_id, ModId mod_id = NoModId) const;
  [[nodiscard]]
  std::expected<String, StarException> mining_sound(MaterialId material_id, ModId mod_id = NoModId) const;
  [[nodiscard]]
  std::expected<String, StarException> footstep_sound(MaterialId material_id, ModId mod_id = NoModId) const;
  String default_footstep_sound() const;

  [[nodiscard]]
  std::expected<Color, StarException> material_particle_color(MaterialId id, MaterialHue hue_shift) const;
  Vec3F radiant_light(MaterialId material_id, ModId mod_id) const;

  [[nodiscard]]
  std::expected<bool, StarException> supports_mod(MaterialId material_id, ModId mod_id) const;
  [[nodiscard]]
  std::expected<ModId, StarException> tilled_mod_for(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> is_tilled_mod(ModId id) const;

  [[nodiscard]]
  std::expected<bool, StarException> is_soil(MaterialId id) const;
  [[nodiscard]]
  std::expected<FallingBehavior, StarException> falling_behavior(MaterialId id) const;
  [[nodiscard]]
  std::expected<bool, StarException> blocks_liquid_flow(MaterialId id) const;

  // A transform target may be NullMaterialId or NoModId.
  std::optional<LiquidMaterialInteraction> liquid_material_interaction(LiquidId liquid, MaterialId id) const;
  std::optional<LiquidModInteraction> liquid_mod_interaction(LiquidId liquid, ModId id) const;

private:
  struct MetaMaterialInfo {
    String name;
    CollisionKind collision_kind;
    bool blocks_liquid_flow;
    // One-based index in metamaterials.config, or zero for a built-in declaration.
    std::size_t source_entry = 0;
  };

  struct AssetInfo {
    AssetInfo() = default;
    AssetInfo(AssetInfo const&) = delete;

    String name;
    MaterialAsset asset;
    String item_drop;
    String description;
    String short_description;
    std::shared_ptr<ParticleConfig> mining_particle;
    StringList mining_sounds;
    String footstep_sound;
    std::optional<MaterialRenderProfile> render_profile;
    TileDamageParameters damage_parameters;

    [[nodiscard]]
    std::expected<void, StarException> load_descriptions();
    [[nodiscard]]
    std::expected<Color, StarException> load_media(ParticleDatabase const& particles);
    [[nodiscard]]
    std::expected<void, StarException> load_render_and_damage(
        Assets const& assets, ImageMetadataDatabase const& image_metadata,
        String const& template_base, String const& harvest_key);
    [[nodiscard]]
    std::expected<String, StarException> description_for(String const& species) const;
  };

  struct MaterialInfo : AssetInfo {
    String category;
    Color particle_color;
    ModId tillable_mod;
    CollisionKind collision_kind;
    bool foreground_only;
    bool supports_mods;
    bool soil;
    FallingBehavior falling_behavior = FallingBehavior::Stationary;
    bool blocks_liquid_flow;
  };

  struct ModInfo : AssetInfo {
    bool tilled;
    bool breaks_with_tile;
  };

  [[nodiscard]]
  std::expected<void, StarException> load(Assets const& assets, ParticleDatabase const& particles,
      ImageMetadataDatabase const& image_metadata, Logger& logger);
  [[nodiscard]]
  std::expected<void, StarException> load_meta_materials(Assets const& assets, Logger& logger);

  static std::size_t meta_material_slot(MaterialId id);
  bool contains_meta_material(MaterialId id) const;
  void set_meta_material(MaterialId id, MetaMaterialInfo info);
  bool contains_material(MaterialId id) const { return id < m_materials.size() && m_materials[id]; }
  bool contains_mod(ModId id) const { return id < m_mods.size() && m_mods[id]; }
  [[nodiscard]]
  std::expected<void, StarException> register_material(Assets const& assets, MaterialId id, std::unique_ptr<MaterialInfo> info);
  [[nodiscard]]
  std::expected<void, StarException> register_mod(Assets const& assets, ModId id, std::unique_ptr<ModInfo> info);

  [[nodiscard]]
  std::expected<std::reference_wrapper<MetaMaterialInfo const>, StarException> get_meta_material_info(MaterialId id) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<MaterialInfo const>, StarException> get_material_info(MaterialId id) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<ModInfo const>, StarException> get_mod_info(ModId id) const;

  RandomService& m_random;
  List<std::unique_ptr<MetaMaterialInfo const>> m_meta_materials;
  StringMap<MaterialId> m_meta_material_index;
  List<std::unique_ptr<MaterialInfo const>> m_materials;
  StringMap<MaterialId> m_material_index;
  List<std::unique_ptr<ModInfo const>> m_mods;
  StringMap<ModId> m_mod_index;
  BiMap<String, ModId> m_meta_mod_index;
  String m_default_footstep_sound;
  HashMap<std::pair<LiquidId, MaterialId>, LiquidMaterialInteraction> m_liquid_material_interactions;
  HashMap<std::pair<LiquidId, ModId>, LiquidModInteraction> m_liquid_mod_interactions;
};

inline std::optional<std::reference_wrapper<MaterialRenderProfile const>> MaterialDatabase::material_render_profile(MaterialId id) const {
  if (contains_material(id)) {
    auto const& profile = m_materials[id]->render_profile;
    if (profile)
      return std::cref(*profile);
  }
  return {};
}

inline std::optional<std::reference_wrapper<MaterialRenderProfile const>> MaterialDatabase::mod_render_profile(ModId id) const {
  if (contains_mod(id)) {
    auto const& profile = m_mods[id]->render_profile;
    if (profile)
      return std::cref(*profile);
  }
  return {};
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::foreground_light_transparent(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& profile = info->get().render_profile)
      return profile->foregroundLightTransparent;
  }
  return id != StructureMaterialId;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::background_light_transparent(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& profile = info->get().render_profile)
      return profile->backgroundLightTransparent;
  }
  return id != StructureMaterialId;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::occludes_behind(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    if (auto const& profile = info->get().render_profile)
      return profile->occludesBehind;
  }
  return false;
}

inline Vec3F MaterialDatabase::radiant_light(MaterialId material_id, ModId mod_id) const {
  Vec3F light;
  if (auto profile = material_render_profile(material_id))
    light += profile->get().radiantLight;
  if (auto profile = mod_render_profile(mod_id))
    light += profile->get().radiantLight;
  return light;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::blocks_liquid_flow(MaterialId id) const {
  if (isRealMaterial(id)) {
    auto info = get_material_info(id);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().blocks_liquid_flow;
  }
  auto info = get_meta_material_info(id);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().blocks_liquid_flow;
}

inline std::optional<LiquidMaterialInteraction> MaterialDatabase::liquid_material_interaction(LiquidId liquid, MaterialId id) const {
  return m_liquid_material_interactions.maybe({liquid, id});
}

inline std::optional<LiquidModInteraction> MaterialDatabase::liquid_mod_interaction(LiquidId liquid, ModId id) const {
  return m_liquid_mod_interactions.maybe({liquid, id});
}

}

export module star.material_database;

export namespace Star {
  using ::Star::MaterialExceptionTag;
  using ::Star::MaterialException;
  using ::Star::LiquidMaterialInteraction;
  using ::Star::LiquidModInteraction;
  using ::Star::MaterialAsset;
  using ::Star::ParticleConfig;
  using ::Star::MaterialDatabase;
}
