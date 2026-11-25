module;
import std;
import star.string;
import star.map;
import star.format;
import star.json;

import star.rect;
import star.game_types;
import star.material_types;
import star.vector;
import star.tile_damage;
import star.directives;

namespace Star {
class ImageMetadataDatabase;
class MaterialProfileCompiler;

struct MaterialRenderProfileExceptionTag {
  static constexpr std::string_view name() { return "MaterialRenderProfileException"; }
};
using MaterialRenderProfileException = StarError<MaterialRenderProfileExceptionTag, StarException>;

enum class MaterialJoinType : std::uint8_t { All, Any };

struct MaterialRule {
  struct RuleConnects {};
  struct RuleShadows {};
  struct RuleEqualsSelf {
    bool match_hue;
  };

  struct RuleEqualsId {
    std::uint16_t id;
  };

  struct RulePropertyEquals {
    String property_name;
    Json compare;
  };

  struct RuleEntry {
    std::variant<RuleConnects, RuleShadows, RuleEqualsSelf, RuleEqualsId, RulePropertyEquals> rule;
    bool inverse;
  };

  MaterialJoinType join;
  std::vector<RuleEntry> entries;
};

struct MaterialMatchPoint {
  Vec2I position;
  std::size_t rule_index;
};

struct MaterialPiecePlacement {
  std::size_t piece_index;
  // In tile pixels, relative to the matched tile.
  Vec2F position;
};

struct MaterialRenderPiece {
  std::size_t piece_id;
  String texture;

  // World rendering falls back to color zero, but an empty piece is skipped.
  [[nodiscard]] std::optional<std::reference_wrapper<RectF const>>
  world_rectangle(unsigned variant, MaterialColorVariant color_variant) const {
    return rectangle(variant, color_variant, true);
  }

private:
  friend class MaterialProfileCompiler;
  friend struct MaterialRenderProfile;

  [[nodiscard]] std::optional<std::reference_wrapper<RectF const>>
  rectangle(unsigned variant, MaterialColorVariant color_variant, bool fallback) const {
    if (variant_count == 0)
      return {};
    if (color_variant > last_color) {
      if (!fallback)
        return {};
      color_variant = default_material_color_variant;
    }
    return std::cref(rectangles[std::size_t(color_variant) * variant_count + variant % variant_count]);
  }

  // Dense color rows; every row has the same number of random variants.
  std::vector<RectF> rectangles;
  unsigned variant_count = 0;
  MaterialColorVariant last_color = default_material_color_variant;
};

struct MaterialRenderMatch {
  MaterialJoinType match_join = MaterialJoinType::All;
  std::optional<TileLayer> required_layer;
  std::optional<bool> occlude;
  bool halt_on_match = false;
  bool halt_on_sub_match = false;

private:
  friend class MaterialProfileCompiler;
  friend struct MaterialRenderProfile;

  struct Range {
    std::size_t begin = 0;
    std::size_t size = 0;
  };

  Range point_range;
  Range placement_range;
  Range sub_match_range;
};

// Neighbor rules can influence rendered tiles within this many tiles on each axis.
inline constexpr int material_render_profile_max_neighbor_distance = 2;

struct MaterialRenderProfile {
  String representative_piece;
  std::vector<Directives> color_directives;
  Json rule_properties;

  bool foreground_light_transparent = false;
  bool background_light_transparent = false;
  std::uint8_t color_variants = 0;
  bool occludes_behind = true;
  std::uint32_t z_level = 0;
  Vec3F radiant_light;

  // These views borrow the profile; indices are local to this compiled graph.
  [[nodiscard]] std::span<MaterialRenderMatch const> main_matches() const {
    return std::span(compiled_matches).subspan(main_match_range.begin, main_match_range.size);
  }
  [[nodiscard]] std::span<MaterialRenderMatch const> sub_matches(MaterialRenderMatch const& match) const {
    return std::span(compiled_matches).subspan(match.sub_match_range.begin, match.sub_match_range.size);
  }
  [[nodiscard]] std::span<MaterialMatchPoint const> match_points(MaterialRenderMatch const& match) const {
    return std::span(compiled_points).subspan(match.point_range.begin, match.point_range.size);
  }
  [[nodiscard]] std::span<MaterialPiecePlacement const> placements(MaterialRenderMatch const& match) const {
    return std::span(compiled_placements).subspan(match.placement_range.begin, match.placement_range.size);
  }
  [[nodiscard]] MaterialRule const& rule(std::size_t index) const { return compiled_rules[index]; }
  [[nodiscard]] MaterialRenderPiece const& piece(std::size_t index) const { return compiled_pieces[index]; }
  [[nodiscard]] std::expected<std::reference_wrapper<MaterialRenderPiece const>, StarException> find_piece(String const& name) const;

  [[nodiscard]] MaterialColorVariant world_color_variant(MaterialColorVariant color_variant) const {
    return color_variants > 0 ? color_variant % color_variants : default_material_color_variant;
  }
  [[nodiscard]] std::size_t color_directive_slot(MaterialColorVariant selected_color) const {
    return color_directives.empty() ? 0 : selected_color % color_directives.size();
  }
  [[nodiscard]] std::optional<std::reference_wrapper<Directives const>>
  color_directive(MaterialColorVariant selected_color) const {
    if (color_directives.empty())
      return {};
    return std::cref(color_directives[color_directive_slot(selected_color)]);
  }

  // Crop a single piece; explicit color selection never falls back.
  [[nodiscard]]
  std::expected<String, StarException> piece_image(String const& piece_name,
      unsigned variant,
      MaterialColorVariant color_variant = default_material_color_variant,
      MaterialHue hue_shift = MaterialHue()) const;

  // Get a damage overlay and its offset in world coordinates.
  [[nodiscard]]
  std::expected<std::reference_wrapper<std::pair<String, Vec2F> const>, StarException> damage_image(float damage_level, TileDamageType damage_type) const;

private:
  friend class MaterialProfileCompiler;

  StringMap<std::size_t> piece_indices;
  std::vector<MaterialRule> compiled_rules;
  std::vector<MaterialRenderPiece> compiled_pieces;
  std::vector<MaterialRenderMatch> compiled_matches;
  std::vector<MaterialMatchPoint> compiled_points;
  std::vector<MaterialPiecePlacement> compiled_placements;
  MaterialRenderMatch::Range main_match_range;
};

[[nodiscard]]
std::expected<MaterialRenderProfile, StarException> parse_material_render_profile(ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path = "");

}

export module star.material_render_profile;

export namespace Star {
  using ::Star::MaterialRenderProfileExceptionTag;
  using ::Star::MaterialRenderProfileException;
  using ::Star::MaterialJoinType;
  using ::Star::MaterialRule;
  using ::Star::MaterialMatchPoint;
  using ::Star::MaterialPiecePlacement;
  using ::Star::MaterialRenderPiece;
  using ::Star::MaterialRenderMatch;
  using ::Star::material_render_profile_max_neighbor_distance;
  using ::Star::MaterialRenderProfile;
  using ::Star::parse_material_render_profile;
}
