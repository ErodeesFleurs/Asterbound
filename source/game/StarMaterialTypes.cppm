module;

import std;

import star.math_common;
namespace Star {

using MaterialId = std::uint16_t;
using MaterialHue = std::uint8_t;

// Empty and non-colliding.
inline constexpr MaterialId EmptyMaterialId = 65535;

// Empty and colliding. Also used as a placeholder in world generation, and to
// signal a block which has not yet been loaded
inline constexpr MaterialId NullMaterialId = 65534;

// Invisible colliding material used for pre-drawn world strucutres.
inline constexpr MaterialId StructureMaterialId = 65533;

// Placeholder material used in dungeon generation for biome native ground
// material.
inline constexpr MaterialId Biome5MaterialId = 65532;
inline constexpr MaterialId Biome4MaterialId = 65531;
inline constexpr MaterialId Biome3MaterialId = 65530;
inline constexpr MaterialId Biome2MaterialId = 65529;
inline constexpr MaterialId Biome1MaterialId = 65528;
inline constexpr MaterialId BiomeMaterialId = 65527;

// invisible walls that can't be connected to or grappled
inline constexpr MaterialId BoundaryMaterialId = 65526;

// default generic object metamaterials
inline constexpr MaterialId ObjectSolidMaterialId = 65500;
inline constexpr MaterialId ObjectPlatformMaterialId = 65501;

// Material IDs 65500 and above are reserved for engine-specified metamaterials
inline constexpr MaterialId FirstEngineMetaMaterialId = 65500;

// Material IDs 65000 - 65499 are reserved for configurable metamaterials
// to be used by tile entities or scripts
inline constexpr MaterialId FirstMetaMaterialId = 65000;

using MaterialColorVariant = std::uint8_t;
inline constexpr MaterialColorVariant DefaultMaterialColorVariant = 0;
inline constexpr MaterialColorVariant MaxMaterialColorVariant = 8;

using ModId = std::uint16_t;

// Tile has no tilemod
inline constexpr ModId NoModId = 65535;

// Placeholder mod used in dungeon generation for biome native ground mod.
inline constexpr ModId BiomeModId = 65534;
inline constexpr ModId UndergroundBiomeModId = 65533;

// The first mod id that is reserved for special hard-coded mod values.
inline constexpr ModId FirstMetaMod = 65520;

float materialHueToDegrees(MaterialHue hue);
MaterialHue materialHueFromDegrees(float degrees);

bool isRealMaterial(MaterialId material);
bool isConnectableMaterial(MaterialId material);
bool isBiomeMaterial(MaterialId material);

bool isRealMod(ModId mod);
bool isBiomeMod(ModId mod);

inline float materialHueToDegrees(MaterialHue hue) {
  return hue * 360.0f / 255.0f;
}

inline MaterialHue materialHueFromDegrees(float degrees) {
  // Truncate while signed; only the integer conversion wraps negative hues.
  return static_cast<MaterialHue>(static_cast<int>(std::fmod(degrees, 360.0f) * 255.0f / 360.0f));
}

inline bool isRealMaterial(MaterialId material) {
  return material < FirstMetaMaterialId;
}

// TODO: metamaterials need more flexibility to define whether they're connectable,
// but this is used in several performance intensive areas, some of which don't
// and probably shouldn't use the material database
inline bool isConnectableMaterial(MaterialId material) {
  return !(material == NullMaterialId || material == EmptyMaterialId || material == BoundaryMaterialId);
}

inline bool isBiomeMaterial(MaterialId material) {
  return (material == BiomeMaterialId) || ((material >= Biome1MaterialId) && (material <= Biome5MaterialId));
}

inline bool isRealMod(ModId mod) {
  return mod < FirstMetaMod;
}

inline bool isBiomeMod(ModId mod) {
  return mod == BiomeModId || mod == UndergroundBiomeModId;
}
}

export module star.material_types;

export namespace Star {
  using ::Star::MaterialId;
  using ::Star::MaterialHue;
  using ::Star::EmptyMaterialId;
  using ::Star::NullMaterialId;
  using ::Star::StructureMaterialId;
  using ::Star::Biome5MaterialId;
  using ::Star::Biome4MaterialId;
  using ::Star::Biome3MaterialId;
  using ::Star::Biome2MaterialId;
  using ::Star::Biome1MaterialId;
  using ::Star::BiomeMaterialId;
  using ::Star::BoundaryMaterialId;
  using ::Star::ObjectSolidMaterialId;
  using ::Star::ObjectPlatformMaterialId;
  using ::Star::FirstEngineMetaMaterialId;
  using ::Star::FirstMetaMaterialId;
  using ::Star::MaterialColorVariant;
  using ::Star::DefaultMaterialColorVariant;
  using ::Star::MaxMaterialColorVariant;
  using ::Star::ModId;
  using ::Star::NoModId;
  using ::Star::BiomeModId;
  using ::Star::UndergroundBiomeModId;
  using ::Star::FirstMetaMod;
  using ::Star::materialHueToDegrees;
  using ::Star::materialHueFromDegrees;
  using ::Star::isRealMaterial;
  using ::Star::isConnectableMaterial;
  using ::Star::isBiomeMaterial;
  using ::Star::isRealMod;
  using ::Star::isBiomeMod;
}
