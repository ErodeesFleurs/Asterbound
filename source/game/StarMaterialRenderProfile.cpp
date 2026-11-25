import std;
import star.asset_path;
import star.bi_map;
import star.color;
import star.directives;
import star.format;
import star.game_types;
import star.image_metadata_database;
import star.json;
import star.json_extra;
import star.list;
import star.map;
import star.material_render_profile;
import star.material_types;
import star.math_common;
import star.rect;
import star.string;
import star.string_view;
import star.tile_damage;
import star.vector;

namespace Star {
namespace {

// Limit generated rectangles per profile to 16 MiB of RectF storage.
constexpr std::size_t max_profile_rectangles = 1'048'576;

constexpr auto material_join_type_names =
    sorted_enum_names(std::to_array<std::pair<MaterialJoinType, std::string_view>>(
        {{MaterialJoinType::All, "All"}, {MaterialJoinType::Any, "Any"}}));

enum class RuleKind : std::uint8_t { Connects, Shadows, EqualsSelf, EqualsId, PropertyEquals };
constexpr auto rule_kind_names = std::to_array<std::pair<RuleKind, std::string_view>>({
    {RuleKind::Connects, "Connects"},
    {RuleKind::Shadows, "Shadows"},
    {RuleKind::EqualsSelf, "EqualsSelf"},
    {RuleKind::EqualsId, "EqualsId"},
    {RuleKind::PropertyEquals, "PropertyEquals"},
});

std::expected<std::optional<RuleKind>, StarException> find_rule_kind(StringView name) {
  for (auto const& [kind, spelling] : rule_kind_names) {
    auto matches = name.equals_ignore_case(spelling);
    if (!matches)
      return std::unexpected(std::move(matches.error()));
    if (*matches)
      return kind;
  }
  return std::nullopt;
}

std::expected<Vec2F, StarException> parse_stride(Json const& config, String const& field) {
  auto value = config.find(field);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (*value)
    return json_to_vec2f((*value)->get());
  return Vec2F(0, 0);
}

using DamageFrame = std::pair<String, Vec2F>;
constexpr std::size_t damage_frame_count = 5;
std::array<DamageFrame, damage_frame_count> const cracking_frames{{
    {"/tiles/blockdamage.png:1", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:2", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:3", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:4", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:5", Vec2F(0, 0)},
}};
std::array<DamageFrame, damage_frame_count> const protected_frames{{
    {"/tiles/blockprotection.png:1", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:2", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:3", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:4", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:5", Vec2F(0, 0)},
}};

template <std::unsigned_integral Value>
std::expected<Value, StarException> checked_unsigned(
    std::expected<std::uint64_t, StarException> value, std::string_view field) {
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!std::in_range<Value>(*value))
    return std::unexpected(MaterialRenderProfileException::format(
        "{} value {} is out of range [0, {}]", field, *value, std::numeric_limits<Value>::max()));
  return static_cast<Value>(*value);
}

std::expected<Vec2I, StarException> parse_match_position(Json const& config) {
  auto components = config.iterate_array();
  if (!components || components->container->size() != 2)
    return std::unexpected(JsonException("Json not an array of size 2 in jsonToVec2I"));

  Vec2I position;
  for (std::size_t axis = 0; axis != 2; ++axis) {
    auto coordinate = (*components->container)[axis].to_int();
    if (!coordinate)
      return std::unexpected(std::move(coordinate.error()));
    constexpr auto limit = material_render_profile_max_neighbor_distance;
    if (*coordinate < -limit || *coordinate > limit)
      return std::unexpected(MaterialRenderProfileException::format(
          "Match position component {} outside of maximum rule distance {}", *coordinate, limit));
    position[axis] = static_cast<int>(*coordinate);
  }
  return position;
}

std::expected<MaterialRule, StarException> parse_rule(Json const& config) {
  auto join_name = config.get_string("join", "all");
  if (!join_name)
    return std::unexpected(std::move(join_name.error()));
  auto join = enum_value<material_join_type_names>(*join_name);
  if (!join)
    return std::unexpected(std::move(join.error()));
  MaterialRule result{*join, {}};

  auto entries = config.find("entries");
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  if (!*entries || (*entries)->get().is_null())
    return result;
  auto entries_array = (*entries)->get().iterate_array();
  if (!entries_array)
    return std::unexpected(std::move(entries_array.error()));
  result.entries.reserve(entries_array->container->size());
  for (auto const& entry_config : *entries_array) {
    auto inverse = entry_config.get_bool("inverse", false);
    if (!inverse)
      return std::unexpected(std::move(inverse.error()));
    auto name = entry_config.get_string("type");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto kind = find_rule_kind(*name);
    if (!kind)
      return std::unexpected(std::move(kind.error()));
    // Empty and other unrecognized rule spellings have never emitted an entry.
    if (!*kind)
      continue;

    switch (**kind) {
      case RuleKind::Connects:
        result.entries.push_back({MaterialRule::RuleConnects{}, *inverse});
        break;
      case RuleKind::Shadows:
        result.entries.push_back({MaterialRule::RuleShadows{}, *inverse});
        break;
      case RuleKind::EqualsSelf: {
        auto match_hue = entry_config.get_bool("matchHue", false);
        if (!match_hue)
          return std::unexpected(std::move(match_hue.error()));
        result.entries.push_back({MaterialRule::RuleEqualsSelf{*match_hue}, *inverse});
        break;
      }
      case RuleKind::EqualsId: {
        auto id = checked_unsigned<std::uint16_t>(entry_config.get_uint("id"), "EqualsId.id");
        if (!id)
          return std::unexpected(std::move(id.error()));
        result.entries.push_back({MaterialRule::RuleEqualsId{*id}, *inverse});
        break;
      }
      case RuleKind::PropertyEquals: {
        auto property_name = entry_config.get_string("propertyName");
        if (!property_name)
          return std::unexpected(std::move(property_name.error()));
        auto value = entry_config.get("propertyValue");
        if (!value)
          return std::unexpected(std::move(value.error()));
        result.entries.push_back({MaterialRule::RulePropertyEquals{std::move(*property_name), std::move(*value)}, *inverse});
        break;
      }
    }
  }
  return result;
}

}

class MaterialProfileCompiler {
public:
  std::expected<MaterialRenderProfile, StarException> compile(
      ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path);

private:
  using Range = MaterialRenderMatch::Range;

  std::expected<void, StarException> compile_settings(Json const& spec);
  std::expected<void, StarException> compile_rules(Json const& spec);
  std::expected<void, StarException> compile_pieces(
      ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path);
  std::expected<void, StarException> compile_named_matches(Json const& spec);
  std::expected<Range, StarException> compile_match_list(Json const& spec);
  std::expected<MaterialRenderMatch, StarException> compile_match(Json const& config);
  std::expected<Range, StarException> compile_points(Json const& spec);
  std::expected<Range, StarException> compile_placements(Json const& spec);

  MaterialRenderProfile m_profile;
  StringMap<std::size_t> m_rule_indices;
  StringMap<Range> m_named_matches;
  std::size_t m_rectangle_count = 0;
};

std::expected<void, StarException> MaterialProfileCompiler::compile_settings(Json const& spec) {
  auto transparent = spec.get_bool("lightTransparent", false);
  if (!transparent)
    return std::unexpected(std::move(transparent.error()));
  auto foreground = spec.get_bool("foregroundLightTransparent", *transparent);
  if (!foreground)
    return std::unexpected(std::move(foreground.error()));
  m_profile.foreground_light_transparent = *foreground;
  auto background = spec.get_bool("backgroundLightTransparent", *transparent);
  if (!background)
    return std::unexpected(std::move(background.error()));
  m_profile.background_light_transparent = *background;

  auto multi_colored = spec.get_bool("multiColored", false);
  if (!multi_colored)
    return std::unexpected(std::move(multi_colored.error()));
  if (*multi_colored) {
    auto count = checked_unsigned<std::uint8_t>(
        spec.get_uint("colorVariants", max_material_color_variant + 1), "colorVariants");
    if (!count)
      return std::unexpected(std::move(count.error()));
    m_profile.color_variants = *count;
  }

  auto directives = spec.find("colorDirectives");
  if (!directives)
    return std::unexpected(std::move(directives.error()));
  if (*directives && !(*directives)->get().is_null()) {
    auto entries = (*directives)->get().iterate_array();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    m_profile.color_directives.reserve(entries->container->size());
    for (auto const& entry : *entries) {
      auto text = entry.to_string();
      if (!text)
        return std::unexpected(std::move(text.error()));
      m_profile.color_directives.emplace_back(*text);
    }
  }

  auto occludes_behind = spec.get_bool("occludesBelow", true);
  if (!occludes_behind)
    return std::unexpected(std::move(occludes_behind.error()));
  m_profile.occludes_behind = *occludes_behind;

  // The storage width is independent of the renderer's mod-layer sort bit.
  auto z_level = checked_unsigned<std::uint32_t>(spec.get_uint("zLevel", 0), "zLevel");
  if (!z_level)
    return std::unexpected(std::move(z_level.error()));
  m_profile.z_level = *z_level;

  auto light_config = spec.get("radiantLight", JsonArray{0, 0, 0});
  if (!light_config)
    return std::unexpected(std::move(light_config.error()));
  auto light = json_to_vec3b(*light_config);
  if (!light)
    return std::unexpected(std::move(light.error()));
  m_profile.radiant_light = Color::rgb(*light).to_rgb_f();

  auto representative = spec.get_string("representativePiece");
  if (!representative)
    return std::unexpected(std::move(representative.error()));
  m_profile.representative_piece = std::move(*representative);
  return {};
}

std::expected<void, StarException> MaterialProfileCompiler::compile_rules(Json const& spec) {
  auto config = spec.get("rules");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto rules = config->iterate_object();
  if (!rules)
    return std::unexpected(std::move(rules.error()));
  m_rule_indices.reserve(rules->container->size());
  m_profile.compiled_rules.reserve(rules->container->size());
  for (auto const& [name, rule_config] : *rules) {
    auto rule = parse_rule(rule_config);
    if (!rule)
      return std::unexpected(std::move(rule.error()));
    m_rule_indices[name] = m_profile.compiled_rules.size();
    m_profile.compiled_rules.push_back(std::move(*rule));
  }
  return {};
}

std::expected<void, StarException> MaterialProfileCompiler::compile_pieces(
    ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path) {
  auto config = spec.get("pieces");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto pieces = config->iterate_object();
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  if (pieces->container->empty())
    return {};

  auto default_texture = spec.get_string("texture");
  if (!default_texture)
    return std::unexpected(std::move(default_texture.error()));
  auto default_variants = spec.get_uint("variants", 1);
  if (!default_variants)
    return std::unexpected(std::move(default_variants.error()));
  auto color_rows = std::size_t(m_profile.color_variants) + 1;
  m_profile.compiled_pieces.reserve(pieces->container->size());
  m_profile.piece_indices.reserve(pieces->container->size());

  for (auto const& [name, piece_config] : *pieces) {
    MaterialRenderPiece piece;
    piece.piece_id = m_profile.compiled_pieces.size();
    piece.last_color = m_profile.color_variants;
    auto texture = piece_config.opt_string("texture");
    if (!texture)
      return std::unexpected(std::move(texture.error()));
    piece.texture = AssetPath::relative_to(relative_path, texture->has_value() ? **texture : *default_texture);

    auto count = piece_config.opt_uint("variants");
    if (!count)
      return std::unexpected(std::move(count.error()));
    auto variants = count->value_or(*default_variants);
    if (variants > (max_profile_rectangles - m_rectangle_count) / color_rows)
      return std::unexpected(MaterialRenderProfileException::format(
          "Piece '{}' has {} variants across {} color rows; material profile exceeds the {} rectangle limit",
          name, variants, color_rows, max_profile_rectangles));
    piece.variant_count = static_cast<unsigned>(variants);
    auto rectangle_count = static_cast<std::size_t>(variants) * color_rows;

    auto size = piece_config.get("textureSize").and_then(json_to_vec2f);
    if (!size)
      return std::unexpected(std::move(size.error()));
    auto position = piece_config.get("texturePosition").and_then(json_to_vec2f);
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto variant_stride = parse_stride(piece_config, "variantStride");
    if (!variant_stride)
      return std::unexpected(std::move(variant_stride.error()));
    auto color_stride = parse_stride(piece_config, "colorStride");
    if (!color_stride)
      return std::unexpected(std::move(color_stride.error()));
    auto image_size = image_metadata.image_size(piece.texture);
    if (!image_size)
      return std::unexpected(std::move(image_size.error()));
    auto image_height = (*image_size)[1];

    piece.rectangles.reserve(rectangle_count);
    if (rectangle_count != 0) {
      for (std::size_t color = 0; color < color_rows; ++color) {
        auto color_offset = color != 0 ? *color_stride * color : Vec2F(0, 0);
        for (unsigned variant = 0; variant < piece.variant_count; ++variant) {
          auto origin = *position + *variant_stride * variant;
          if (color != 0)
            origin += color_offset;
          auto rect = RectF::with_size(origin, *size);
          piece.rectangles.push_back(
              RectF::with_size(Vec2F(rect.x_min(), image_height - rect.y_max()), rect.size()));
        }
      }
    }
    m_rectangle_count += rectangle_count;
    m_profile.piece_indices[name] = piece.piece_id;
    m_profile.compiled_pieces.push_back(std::move(piece));
  }
  return {};
}

std::expected<MaterialProfileCompiler::Range, StarException>
MaterialProfileCompiler::compile_points(Json const& spec) {
  auto points = spec.iterate_array();
  if (!points)
    return std::unexpected(std::move(points.error()));
  Range range{m_profile.compiled_points.size(), points->container->size()};
  for (auto const& point : *points) {
    auto position_config = point.get(0);
    if (!position_config)
      return std::unexpected(std::move(position_config.error()));
    auto position = parse_match_position(*position_config);
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto name = point.get_string(1);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto rule = m_rule_indices.get(*name);
    if (!rule)
      return std::unexpected(std::move(rule.error()));
    m_profile.compiled_points.push_back({*position, rule->get()});
  }
  return range;
}

std::expected<MaterialProfileCompiler::Range, StarException>
MaterialProfileCompiler::compile_placements(Json const& spec) {
  auto placements = spec.iterate_array();
  if (!placements)
    return std::unexpected(std::move(placements.error()));
  Range range{m_profile.compiled_placements.size(), placements->container->size()};
  for (auto const& placement : *placements) {
    auto name = placement.get_string(0);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto piece_index = m_profile.piece_indices.get(*name);
    if (!piece_index)
      return std::unexpected(std::move(piece_index.error()));
    auto position_config = placement.get(1);
    if (!position_config)
      return std::unexpected(std::move(position_config.error()));
    auto position = json_to_vec2f(*position_config);
    if (!position)
      return std::unexpected(std::move(position.error()));
    m_profile.compiled_placements.push_back({piece_index->get(), *position});
  }
  return range;
}

std::expected<MaterialRenderMatch, StarException> MaterialProfileCompiler::compile_match(Json const& config) {
  MaterialRenderMatch match;
  auto points = config.find("matchAllPoints");
  if (!points)
    return std::unexpected(std::move(points.error()));
  if (!*points || (*points)->get().is_null()) {
    points = config.find("matchAnyPoints");
    if (!points)
      return std::unexpected(std::move(points.error()));
    if (*points && !(*points)->get().is_null())
      match.match_join = MaterialJoinType::Any;
  }
  if (*points && !(*points)->get().is_null()) {
    auto range = compile_points((*points)->get());
    if (!range)
      return std::unexpected(std::move(range.error()));
    match.point_range = *range;
  }

  auto pieces = config.find("pieces");
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  if (*pieces && !(*pieces)->get().is_null()) {
    auto range = compile_placements((*pieces)->get());
    if (!range)
      return std::unexpected(std::move(range.error()));
    match.placement_range = *range;
  }

  auto sub_matches = config.find("subMatches");
  if (!sub_matches)
    return std::unexpected(std::move(sub_matches.error()));
  if (*sub_matches && !(*sub_matches)->get().is_null()) {
    auto const& list_config = (*sub_matches)->get();
    if (list_config.is_type(Json::Type::String)) {
      auto name = list_config.to_string();
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto range = m_named_matches.get(*name);
      if (!range)
        return std::unexpected(std::move(range.error()));
      // Resolve to the current range, not to a name that can be rebound later.
      match.sub_match_range = range->get();
    } else {
      auto range = compile_match_list(list_config);
      if (!range)
        return std::unexpected(std::move(range.error()));
      match.sub_match_range = *range;
    }
  }

  auto layer = config.opt_string("requiredLayer");
  if (!layer)
    return std::unexpected(std::move(layer.error()));
  if (*layer) {
    auto value = enum_value<tile_layer_names>(**layer);
    if (!value)
      return std::unexpected(std::move(value.error()));
    match.required_layer = *value;
  }
  auto halt_on_match = config.get_bool("haltOnMatch", false);
  if (!halt_on_match)
    return std::unexpected(std::move(halt_on_match.error()));
  match.halt_on_match = *halt_on_match;
  auto halt_on_sub_match = config.get_bool("haltOnSubMatch", false);
  if (!halt_on_sub_match)
    return std::unexpected(std::move(halt_on_sub_match.error()));
  match.halt_on_sub_match = *halt_on_sub_match;
  auto occlude = config.opt_bool("occlude");
  if (!occlude)
    return std::unexpected(std::move(occlude.error()));
  match.occlude = *occlude;
  return match;
}

std::expected<MaterialProfileCompiler::Range, StarException>
MaterialProfileCompiler::compile_match_list(Json const& spec) {
  auto configs = spec.iterate_array();
  if (!configs)
    return std::unexpected(std::move(configs.error()));
  // Reserve the entire sibling range before appending any inline children.
  Range range{m_profile.compiled_matches.size(), configs->container->size()};
  m_profile.compiled_matches.resize(range.begin + range.size);
  auto index = range.begin;
  for (auto const& config : *configs) {
    auto match = compile_match(config);
    if (!match)
      return std::unexpected(std::move(match.error()));
    m_profile.compiled_matches[index++] = std::move(*match);
  }
  return range;
}

std::expected<void, StarException> MaterialProfileCompiler::compile_named_matches(Json const& spec) {
  auto config = spec.get("matches");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto matches = config->iterate_array();
  if (!matches)
    return std::unexpected(std::move(matches.error()));
  m_named_matches.reserve(matches->container->size());
  for (auto const& entry : *matches) {
    auto name = entry.get_string(0);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto list_config = entry.get(1);
    if (!list_config)
      return std::unexpected(std::move(list_config.error()));
    auto range = compile_match_list(*list_config);
    if (!range)
      return std::unexpected(std::move(range.error()));
    m_named_matches[*name] = *range;
  }
  auto main = m_named_matches.get("main");
  if (!main)
    return std::unexpected(std::move(main.error()));
  m_profile.main_match_range = main->get();
  return {};
}

std::expected<MaterialRenderProfile, StarException> MaterialProfileCompiler::compile(
    ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path) {
  if (auto settings = compile_settings(spec); !settings)
    return std::unexpected(std::move(settings.error()));
  if (auto rules = compile_rules(spec); !rules)
    return std::unexpected(std::move(rules.error()));
  if (auto pieces = compile_pieces(image_metadata, spec, relative_path); !pieces)
    return std::unexpected(std::move(pieces.error()));
  if (auto matches = compile_named_matches(spec); !matches)
    return std::unexpected(std::move(matches.error()));

  auto properties = spec.get("ruleProperties", JsonObject());
  if (!properties)
    return std::unexpected(std::move(properties.error()));
  m_profile.rule_properties = std::move(*properties);
  return std::move(m_profile);
}

std::expected<std::reference_wrapper<MaterialRenderPiece const>, StarException>
MaterialRenderProfile::find_piece(String const& name) const {
  auto index = piece_indices.get(name);
  if (!index)
    return std::unexpected(std::move(index.error()));
  return std::cref(piece(index->get()));
}

std::expected<String, StarException> MaterialRenderProfile::piece_image(
    String const& piece_name, unsigned variant, MaterialColorVariant color_variant, MaterialHue hue_shift) const {
  auto result = find_piece(piece_name);
  if (!result)
    return std::unexpected(std::move(result.error()));
  auto const& piece = result->get();
  auto selected = piece.rectangle(variant, color_variant, false);
  if (!selected)
    return std::unexpected(MaterialRenderProfileException::format(
        "Piece '{}' has no texture variants for color {}", piece_name, color_variant));
  auto const& rect = selected->get();
  if (hue_shift != MaterialHue())
    return Star::format("{}?hueshift={}?crop={};{};{};{}", piece.texture, material_hue_to_degrees(hue_shift),
        rect.x_min(), rect.y_min(), rect.x_max(), rect.y_max());
  return Star::format("{}?crop={};{};{};{}", piece.texture, rect.x_min(), rect.y_min(), rect.x_max(), rect.y_max());
}

std::expected<std::reference_wrapper<DamageFrame const>, StarException>
MaterialRenderProfile::damage_image(float damage_level, TileDamageType damage_type) const {
  if (!is_finite(damage_level))
    return std::unexpected(MaterialRenderProfileException("Damage level must be finite"));
  auto const& frames = damage_type == TileDamageType::Protected ? protected_frames : cracking_frames;
  auto scaled = std::clamp(damage_level, 0.0f, 1.0f) * frames.size();
  auto index = std::min(static_cast<std::size_t>(scaled), frames.size() - 1);
  return std::cref(frames[index]);
}

std::expected<MaterialRenderProfile, StarException> parse_material_render_profile(
    ImageMetadataDatabase const& image_metadata, Json const& spec, String const& relative_path) {
  return MaterialProfileCompiler().compile(image_metadata, spec, relative_path);
}

}
