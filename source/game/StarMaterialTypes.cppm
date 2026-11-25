module;

import std;

import star.format;
import star.json;
import star.math_common;
namespace Star {

using MaterialId = std::uint16_t;
using MaterialHue = std::uint8_t;
enum class FallingBehavior : std::uint8_t { Stationary, Falling, Cascading };

// Empty and non-colliding.
inline constexpr MaterialId empty_material_id = 65535;

// Empty and colliding. Also used as a placeholder in world generation, and to
// signal a block which has not yet been loaded
inline constexpr MaterialId null_material_id = 65534;

// Invisible colliding material used for pre-drawn world structures.
inline constexpr MaterialId structure_material_id = 65533;

// Placeholder material used in dungeon generation for biome native ground
// material.
inline constexpr MaterialId biome5_material_id = 65532;
inline constexpr MaterialId biome4_material_id = 65531;
inline constexpr MaterialId biome3_material_id = 65530;
inline constexpr MaterialId biome2_material_id = 65529;
inline constexpr MaterialId biome1_material_id = 65528;
inline constexpr MaterialId biome_material_id = 65527;

// invisible walls that can't be connected to or grappled
inline constexpr MaterialId boundary_material_id = 65526;

// default generic object metamaterials
inline constexpr MaterialId object_solid_material_id = 65500;
inline constexpr MaterialId object_platform_material_id = 65501;

// Material IDs 65500 and above are reserved for engine-specified metamaterials
inline constexpr MaterialId first_engine_meta_material_id = 65500;

// Material IDs 65000 - 65499 are reserved for configurable metamaterials
// to be used by tile entities or scripts
inline constexpr MaterialId first_meta_material_id = 65000;

using MaterialColorVariant = std::uint8_t;
inline constexpr MaterialColorVariant default_material_color_variant = 0;
inline constexpr MaterialColorVariant max_material_color_variant = 8;

using ModId = std::uint16_t;

// Tile has no tilemod
inline constexpr ModId no_mod_id = 65535;

// Placeholder mod used in dungeon generation for biome native ground mod.
inline constexpr ModId biome_mod_id = 65534;
inline constexpr ModId underground_biome_mod_id = 65533;

// The first mod id that is reserved for special hard-coded mod values.
inline constexpr ModId first_meta_mod = 65520;

template <typename T>
concept material_storage_value = std::same_as<T, MaterialId> || std::same_as<T, MaterialHue>;

// Validate the stored width, not registration: world data may contain sentinel IDs.
template <material_storage_value T, std::integral I>
[[nodiscard]] std::expected<T, StarException> checked_material_value(I value, std::string_view field) {
  if (!std::in_range<T>(+value))
    return std::unexpected(StarException(Star::format("{} value {} is out of range [0, {}]", field, +value, +std::numeric_limits<T>::max())));
  return static_cast<T>(value);
}

template <material_storage_value T>
[[nodiscard]] std::expected<T, StarException> checked_material_value(Json const& value, std::string_view field) {
  // Every representable stored value is exact in double. Check before casting
  // so fractional inputs cannot become valid IDs, even immediately below zero.
  auto number = value.to_double();
  if (!number)
    return std::unexpected(std::move(number.error()));
  if (!is_finite(*number) || *number < 0 || *number > std::numeric_limits<T>::max() || std::trunc(*number) != *number)
    return std::unexpected(StarException(Star::format("{} must be an integer in [0, {}], got {}", field, +std::numeric_limits<T>::max(), *number)));
  return static_cast<T>(*number);
}

[[nodiscard]] inline std::expected<MaterialId, StarException> biome_material_for_variant(std::int64_t variant) {
  if (variant < 0 || variant > biome5_material_id - biome_material_id)
    return std::unexpected(StarException(Star::format("surface variant {} is out of range [0, 5]", variant)));
  return static_cast<MaterialId>(biome_material_id + variant);
}

[[nodiscard]] constexpr float material_hue_to_degrees(MaterialHue hue) noexcept {
  return hue * 360.0f / 255.0f;
}

[[nodiscard]] inline std::expected<MaterialHue, StarException> material_hue_from_degrees(float degrees) {
  if (!is_finite(degrees))
    return std::unexpected(StarException("Material hue in degrees must be finite"));
  // Truncate while signed; only the integer conversion wraps negative hues.
  return static_cast<MaterialHue>(static_cast<int>(std::fmod(degrees, 360.0f) * 255.0f / 360.0f));
}

[[nodiscard]] constexpr bool is_real_material(MaterialId material) noexcept {
  return material < first_meta_material_id;
}

// Connectivity is independent of catalog registration and collision.
[[nodiscard]] constexpr bool is_connectable_material(MaterialId material) noexcept {
  return material != null_material_id && material != empty_material_id && material != boundary_material_id;
}

[[nodiscard]] constexpr bool is_biome_material(MaterialId material) noexcept {
  return material >= biome_material_id && material <= biome5_material_id;
}

[[nodiscard]] constexpr bool is_real_mod(ModId mod) noexcept {
  return mod < first_meta_mod;
}

[[nodiscard]] constexpr bool is_biome_mod(ModId mod) noexcept {
  return mod == biome_mod_id || mod == underground_biome_mod_id;
}
}

export module star.material_types;

export namespace Star {
  using ::Star::MaterialId;
  using ::Star::MaterialHue;
  using ::Star::FallingBehavior;
  using ::Star::empty_material_id;
  using ::Star::null_material_id;
  using ::Star::structure_material_id;
  using ::Star::biome5_material_id;
  using ::Star::biome4_material_id;
  using ::Star::biome3_material_id;
  using ::Star::biome2_material_id;
  using ::Star::biome1_material_id;
  using ::Star::biome_material_id;
  using ::Star::boundary_material_id;
  using ::Star::object_solid_material_id;
  using ::Star::object_platform_material_id;
  using ::Star::first_engine_meta_material_id;
  using ::Star::first_meta_material_id;
  using ::Star::MaterialColorVariant;
  using ::Star::default_material_color_variant;
  using ::Star::max_material_color_variant;
  using ::Star::ModId;
  using ::Star::no_mod_id;
  using ::Star::biome_mod_id;
  using ::Star::underground_biome_mod_id;
  using ::Star::first_meta_mod;
  using ::Star::material_storage_value;
  using ::Star::checked_material_value;
  using ::Star::biome_material_for_variant;
  using ::Star::material_hue_to_degrees;
  using ::Star::material_hue_from_degrees;
  using ::Star::is_real_material;
  using ::Star::is_connectable_material;
  using ::Star::is_biome_material;
  using ::Star::is_real_mod;
  using ::Star::is_biome_mod;
}
