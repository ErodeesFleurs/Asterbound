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
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
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
  float consumeLiquid;
  MaterialId transformTo;
  bool topOnly;
};

struct LiquidModInteraction {
  float consumeLiquid;
  ModId transformTo;
  bool topOnly;
};

class Assets;
class ParticleDatabase;
class Logger;
class RandomService;
class ImageMetadataDatabase;

class MaterialDatabase {
public:
  [[nodiscard]]
  static std::expected<std::shared_ptr<MaterialDatabase>, StarException> create(std::shared_ptr<Assets const> const& assets, std::shared_ptr<ParticleDatabase const> const& particles, ImageMetadataDatabase const& imageMetadata, Logger& logger, RandomService& random);

  StringList materialNames() const;
  bool isMetaMaterialName(String const& name) const;
  bool isMaterialName(String const& name) const;
  bool isValidMaterialId(MaterialId material) const;
  [[nodiscard]]
  std::expected<MaterialId, StarException> materialId(String const& materialName) const;
  [[nodiscard]]
  std::expected<String, StarException> materialName(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> materialPath(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<std::optional<Json>, StarException> materialConfig(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<String, StarException> materialDescription(MaterialId materialId, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> materialDescription(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<String, StarException> materialShortDescription(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<String, StarException> materialCategory(MaterialId materialId) const;

  StringList modNames() const;
  bool isModName(String const& name) const;
  bool isValidModId(ModId mod) const;
  [[nodiscard]]
  std::expected<ModId, StarException> modId(String const& modName) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<String const>, StarException> modName(ModId modId) const;
  [[nodiscard]]
  std::expected<std::optional<String>, StarException> modPath(ModId modId) const;
  [[nodiscard]]
  std::expected<std::optional<Json>, StarException> modConfig(ModId modId) const;
  [[nodiscard]]
  std::expected<String, StarException> modDescription(ModId modId, String const& species) const;
  [[nodiscard]]
  std::expected<String, StarException> modDescription(ModId modId) const;
  [[nodiscard]]
  std::expected<String, StarException> modShortDescription(ModId modId) const;

  // Will return nullptr if no rendering profile is available
  std::shared_ptr<MaterialRenderProfile const> materialRenderProfile(MaterialId modId) const;
  std::shared_ptr<MaterialRenderProfile const> modRenderProfile(ModId modId) const;

  [[nodiscard]]
  std::expected<TileDamageParameters, StarException> materialDamageParameters(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<TileDamageParameters, StarException> modDamageParameters(ModId modId) const;

  [[nodiscard]]
  std::expected<bool, StarException> modBreaksWithTile(ModId modId) const;

  [[nodiscard]]
  std::expected<CollisionKind, StarException> materialCollisionKind(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<bool, StarException> canPlaceInLayer(MaterialId materialId, TileLayer layer) const;

  // Returned ItemDescriptor may be null
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> materialItemDrop(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<ItemDescriptor, StarException> modItemDrop(ModId modId) const;

  [[nodiscard]]
  std::expected<MaterialColorVariant, StarException> materialColorVariants(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<MaterialColorVariant, StarException> modColorVariants(ModId modId) const;
  [[nodiscard]]
  std::expected<bool, StarException> isMultiColor(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<bool, StarException> foregroundLightTransparent(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<bool, StarException> backgroundLightTransparent(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<bool, StarException> occludesBehind(MaterialId materialId) const;

  [[nodiscard]]
  std::expected<std::shared_ptr<ParticleConfig>, StarException> miningParticle(MaterialId materialId, ModId modId = NoModId) const;
  [[nodiscard]]
  std::expected<String, StarException> miningSound(MaterialId materialId, ModId modId = NoModId) const;
  [[nodiscard]]
  std::expected<String, StarException> footstepSound(MaterialId materialId, ModId modId = NoModId) const;
  String defaultFootstepSound() const;

  [[nodiscard]]
  std::expected<Color, StarException> materialParticleColor(MaterialId materialId, MaterialHue hueShift) const;
  Vec3F radiantLight(MaterialId materialId, ModId modId) const;

  [[nodiscard]]
  std::expected<bool, StarException> supportsMod(MaterialId materialId, ModId modId) const;
  [[nodiscard]]
  std::expected<ModId, StarException> tilledModFor(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<bool, StarException> isTilledMod(ModId modId) const;

  [[nodiscard]]
  std::expected<bool, StarException> isSoil(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<FallingBehavior, StarException> falling_behavior(MaterialId material_id) const;
  [[nodiscard]]
  std::expected<bool, StarException> blocksLiquidFlow(MaterialId materialId) const;

  // Returns the amount of liquid to consume, and optionally the material / mod
  // to transform to (may be NullMaterialId / NullModId)
  std::optional<LiquidMaterialInteraction> liquidMaterialInteraction(LiquidId liquid, MaterialId materialId) const;
  std::optional<LiquidModInteraction> liquidModInteraction(LiquidId liquid, ModId modId) const;

private:
  explicit MaterialDatabase(RandomService& random) : m_random(random) {}
  [[nodiscard]]
  std::expected<void, StarException> load(std::shared_ptr<Assets const> const& assets, std::shared_ptr<ParticleDatabase const> const& particles, ImageMetadataDatabase const& imageMetadata, Logger& logger);
  RandomService& m_random;
  struct MetaMaterialInfo {
    MetaMaterialInfo(String name, MaterialId id, CollisionKind collisionKind, bool blocksLiquidFlow);

    String name;
    MaterialId id;
    CollisionKind collisionKind;
    bool blocksLiquidFlow;
  };

  struct MaterialInfo {
    MaterialInfo();

    String name;
    MaterialId id;
    String path;
    Json config;

    String itemDrop;
    Json descriptions;
    String category;
    Color particleColor;
    std::shared_ptr<ParticleConfig> miningParticle;
    StringList miningSounds;
    String footstepSound;
    ModId tillableMod;
    CollisionKind collisionKind;
    bool foregroundOnly;
    bool supportsMods;
    bool soil;
    FallingBehavior falling_behavior = FallingBehavior::Stationary;
    bool blocksLiquidFlow;

    std::shared_ptr<MaterialRenderProfile const> materialRenderProfile;

    TileDamageParameters damageParameters;
  };

  struct ModInfo {
    ModInfo();

    String name;
    ModId id;
    String path;
    Json config;

    String itemDrop;
    Json descriptions;
    Color particleColor;
    std::shared_ptr<ParticleConfig> miningParticle;
    StringList miningSounds;
    String footstepSound;
    bool tilled;
    bool breaksWithTile;

    std::shared_ptr<MaterialRenderProfile const> modRenderProfile;

    TileDamageParameters damageParameters;
  };

  std::size_t metaMaterialIndex(MaterialId materialId) const;
  bool containsMetaMaterial(MaterialId materialId) const;
  void setMetaMaterial(MaterialId materialId, MetaMaterialInfo info);

  bool containsMaterial(MaterialId materialId) const;
  void setMaterial(MaterialId materialId, MaterialInfo info);

  bool containsMod(ModId modId) const;
  void setMod(ModId modId, ModInfo info);

  [[nodiscard]]
  std::expected<std::reference_wrapper<MetaMaterialInfo const>, StarException> getMetaMaterialInfo(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<MaterialInfo const>, StarException> getMaterialInfo(MaterialId materialId) const;
  [[nodiscard]]
  std::expected<std::reference_wrapper<ModInfo const>, StarException> getModInfo(ModId modId) const;

  List<std::shared_ptr<MetaMaterialInfo const>> m_metaMaterials;
  StringMap<MaterialId> m_metaMaterialIndex;

  List<std::shared_ptr<MaterialInfo const>> m_materials;
  StringMap<MaterialId> m_materialIndex;

  List<std::shared_ptr<ModInfo const>> m_mods;
  StringMap<ModId> m_modIndex;
  BiMap<String, ModId> m_metaModIndex;

  String m_defaultFootstepSound;

  HashMap<std::pair<LiquidId, MaterialId>, LiquidMaterialInteraction> m_liquidMaterialInteractions;
  HashMap<std::pair<LiquidId, ModId>, LiquidModInteraction> m_liquidModInteractions;
};

inline std::shared_ptr<MaterialRenderProfile const> MaterialDatabase::materialRenderProfile(MaterialId materialId) const {
  if (materialId < m_materials.size()) {
    if (auto const& mat = m_materials[materialId])
      return mat->materialRenderProfile;
  }
  
  return {};
}

inline std::shared_ptr<MaterialRenderProfile const> MaterialDatabase::modRenderProfile(ModId modId) const {
  if (modId < m_mods.size()) {
    if (auto const& mod = m_mods[modId])
      return mod->modRenderProfile;
  }
  
  return {};
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::foregroundLightTransparent(MaterialId materialId) const {
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().materialRenderProfile) return info->get().materialRenderProfile->foregroundLightTransparent;
  }
  return materialId != StructureMaterialId;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::backgroundLightTransparent(MaterialId materialId) const {
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().materialRenderProfile) return info->get().materialRenderProfile->backgroundLightTransparent;
  }
  return materialId != StructureMaterialId;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::occludesBehind(MaterialId materialId) const {
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    if (info->get().materialRenderProfile) return info->get().materialRenderProfile->occludesBehind;
  }
  return false;
}

inline Vec3F MaterialDatabase::radiantLight(MaterialId materialId, ModId modId) const {
  Vec3F radiantLight;
  if (materialId < m_materials.size()) {
    auto const& mat = m_materials[materialId];
    if (mat && mat->materialRenderProfile)
      radiantLight += mat->materialRenderProfile->radiantLight;
  }
  if (modId < m_mods.size()) {
    auto const& mod = m_mods[modId];
    if (mod && mod->modRenderProfile)
      radiantLight += mod->modRenderProfile->radiantLight;
  }
  return radiantLight;
}

[[nodiscard]]
inline std::expected<bool, StarException> MaterialDatabase::blocksLiquidFlow(MaterialId materialId) const {
  if (isRealMaterial(materialId)) {
    auto info = getMaterialInfo(materialId);
    if (!info) return std::unexpected(std::move(info.error()));
    return info->get().blocksLiquidFlow;
  }
  auto info = getMetaMaterialInfo(materialId);
  if (!info) return std::unexpected(std::move(info.error()));
  return info->get().blocksLiquidFlow;
}

inline std::optional<LiquidMaterialInteraction> MaterialDatabase::liquidMaterialInteraction(
    LiquidId liquid, MaterialId materialId) const {
  return m_liquidMaterialInteractions.maybe({liquid, materialId});
}

inline std::optional<LiquidModInteraction> MaterialDatabase::liquidModInteraction(LiquidId liquid, ModId modId) const {
  return m_liquidModInteractions.maybe({liquid, modId});
}
}

export module star.material_database;

export namespace Star {
  using ::Star::MaterialExceptionTag;
  using ::Star::MaterialException;
  using ::Star::LiquidMaterialInteraction;
  using ::Star::LiquidModInteraction;
  using ::Star::ParticleConfig;
  using ::Star::MaterialDatabase;
}
