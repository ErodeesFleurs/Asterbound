module;
import std;
import star.string;
import star.map;
import star.format;
import star.json;

import star.rect;
import star.bi_map;
import star.game_types;
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

namespace Star {
class ImageMetadataDatabase;

struct MaterialRenderProfileExceptionTag {
  static constexpr std::string_view name() { return "MaterialRenderProfileException"; }
};
using MaterialRenderProfileException = StarError<MaterialRenderProfileExceptionTag, StarException>;

enum class MaterialJoinType : std::uint8_t { All, Any };
extern std::expected<EnumMap<MaterialJoinType>, StarException> const MaterialJoinTypeNames;

struct MaterialRule {
  struct RuleEmpty {};
  struct RuleConnects {};
  struct RuleShadows {};
  struct RuleEqualsSelf {
    bool matchHue;
  };

  struct RuleEqualsId {
    std::uint16_t id;
  };

  struct RulePropertyEquals {
    String propertyName;
    Json compare;
  };

  struct RuleEntry {
    // Keep both EqualsSelf slots; type-selected rules use the first (index 4).
    std::variant<std::monostate, RuleEmpty, RuleConnects, RuleShadows, RuleEqualsSelf, RuleEqualsSelf, RuleEqualsId, RulePropertyEquals> rule;
    bool inverse;
  };

  MaterialJoinType join;
  List<RuleEntry> entries;
};
using RuleMap = StringMap<std::shared_ptr<MaterialRule const>>;

struct MaterialMatchPoint {
  Vec2I position;
  std::shared_ptr<MaterialRule const> rule;
};

struct MaterialRenderPiece {
  std::size_t pieceId;
  String texture;
  // Maps each MaterialColorVariant to a list of texture coordinates for each
  // random variant
  HashMap<MaterialColorVariant, List<RectF>> variants;
};

struct MaterialRenderMatch;

using MaterialRenderMatchList = List<std::shared_ptr<MaterialRenderMatch const>>;

struct MaterialRenderMatch {
  List<MaterialMatchPoint> matchPoints;
  MaterialJoinType matchJoin;

  // Positions here are in TilePixels
  List<std::pair<std::shared_ptr<MaterialRenderPiece const>, Vec2F>> resultingPieces;
  MaterialRenderMatchList subMatches;
  std::optional<TileLayer> requiredLayer;
  std::optional<bool> occlude;
  bool haltOnMatch;
  bool haltOnSubMatch;
};

using PieceMap = StringMap<std::shared_ptr<MaterialRenderPiece const>>;
using MatchMap = StringMap<MaterialRenderMatchList>;

// This is the maximum distance in either X or Y that material neighbor rules
// are limited to.  This can be used as a maximum limit on the "sphere of
// influence" that a tile can have on other tile's rendering.  A value of 1
// here means "1 away", so would be interpreted as a 3x3 block with the
// rendered tile in the center.
inline constexpr int MaterialRenderProfileMaxNeighborDistance = 2;

struct MaterialRenderProfile {
  RuleMap rules;
  PieceMap pieces;
  MatchMap matches;

  String representativePiece;

  MaterialRenderMatchList mainMatchList;
  List<std::pair<String, Vec2F>> crackingFrames;
  List<std::pair<String, Vec2F>> protectedFrames;
  List<Directives> colorDirectives;
  Json ruleProperties;

  bool foregroundLightTransparent;
  bool backgroundLightTransparent;
  std::uint8_t colorVariants;
  bool occludesBehind;
  std::uint32_t zLevel;
  Vec3F radiantLight;

  // Get a single asset path for just a single piece of a material, with the
  // image cropped to the piece itself.
  [[nodiscard]]
  std::expected<String, StarException> pieceImage(String const& pieceName,
      unsigned variant,
      MaterialColorVariant colorVariant = DefaultMaterialColorVariant,
      MaterialHue hueShift = MaterialHue()) const;

  // Get an overlay image for rendering damaged tiles, as well as the offset
  // for it in world coordinates.
  [[nodiscard]]
  std::expected<std::reference_wrapper<std::pair<String, Vec2F> const>, StarException> damageImage(float damageLevel, TileDamageType damageType) const;
};

[[nodiscard]]
std::expected<MaterialRenderProfile, StarException> parseMaterialRenderProfile(ImageMetadataDatabase const& imageMetadata, Json const& spec, String const& relativePath = "");

}

export module star.material_render_profile;

export namespace Star {
  using ::Star::MaterialRenderProfileExceptionTag;
  using ::Star::MaterialRenderProfileException;
  using ::Star::MaterialJoinType;
  using ::Star::MaterialJoinTypeNames;
  using ::Star::MaterialRule;
  using ::Star::RuleMap;
  using ::Star::MaterialMatchPoint;
  using ::Star::MaterialRenderPiece;
  using ::Star::MaterialRenderMatch;
  using ::Star::MaterialRenderMatchList;
  using ::Star::PieceMap;
  using ::Star::MatchMap;
  using ::Star::MaterialRenderProfileMaxNeighborDistance;
  using ::Star::MaterialRenderProfile;
  using ::Star::parseMaterialRenderProfile;
}
