import std;
import star.asset_path;
import star.assets;
import star.collision_block;
import star.color;
import star.directives;
import star.drawable;
import star.format;
import star.game_types;
import star.json;
import star.json_extra;
import star.liquid_types;
import star.material_database;
import star.material_render_profile;
import star.material_types;
import star.math_common;
import star.rect;
import star.root;
import star.static_random;
import star.string;
import star.tile_damage;
import star.tile_drawer;
import star.vector;
import star.world_geometry;
import star.world_render_data;
import star.world_tiles;

namespace Star {

RenderTile const TileDrawer::default_render_tile{
    null_material_id,
    no_mod_id,
    null_material_id,
    no_mod_id,
    0,
    0,
    default_material_color_variant,
    TileDamageType::Protected,
    0,
    0,
    0,
    default_material_color_variant,
    TileDamageType::Protected,
    0,
    empty_liquid_id,
    0
};

std::expected<std::shared_ptr<TileDrawer>, StarException> TileDrawer::create(Root& root) {
  struct ConstructedDrawer final : TileDrawer {
    explicit ConstructedDrawer(Root& root) : TileDrawer(root) {}
  };
  auto drawer = std::make_shared<ConstructedDrawer>(root);
  auto initialized = drawer->initialize_tile_drawer();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return drawer;
}

std::expected<void, StarException> TileDrawer::initialize_tile_drawer() {
  auto assets = m_root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto background_config = (*assets)->json("/rendering.config:backgroundLayerColor");
  if (!background_config)
    return std::unexpected(std::move(background_config.error()));
  auto background = json_to_color(*background_config);
  if (!background)
    return std::unexpected(std::move(background.error()));
  m_background_layer_color = background->to_rgba();
  auto foreground_config = (*assets)->json("/rendering.config:foregroundLayerColor");
  if (!foreground_config)
    return std::unexpected(std::move(foreground_config.error()));
  auto foreground = json_to_color(*foreground_config);
  if (!foreground)
    return std::unexpected(std::move(foreground.error()));
  m_foreground_layer_color = foreground->to_rgba();
  auto liquid_config = (*assets)->json("/rendering.config:liquidDrawLevels");
  if (!liquid_config)
    return std::unexpected(std::move(liquid_config.error()));
  auto liquid = json_to_vec2f(*liquid_config);
  if (!liquid)
    return std::unexpected(std::move(liquid.error()));
  m_liquid_draw_levels = *liquid;
  return {};
}

std::expected<bool, StarException> TileDrawer::produce_terrain_drawables(Drawables& drawables,
    TerrainLayer terrain_layer, Vec2I const& pos, WorldRenderData const& render_data, float scale, Vec2I variant_offset,
    std::optional<TerrainLayer> variant_layer) {
  auto material_database_result = m_root.material_database();
  if (!material_database_result)
    return std::unexpected(std::move(material_database_result.error()));
  auto const& material_database = **material_database_result;
  auto image_metadata_result = m_root.image_metadata_database();
  if (!image_metadata_result)
    return std::unexpected(std::move(image_metadata_result.error()));
  auto const& image_metadata = *image_metadata_result;

  RenderTile const& tile = get_render_tile(render_data, pos);
  bool const is_background = terrain_layer == TerrainLayer::Background;
  auto material = is_background ? tile.background : tile.foreground;

  // render non-block colliding things in the midground
  auto collision = material_database.material_collision_kind(material);
  if (!collision)
    return std::unexpected(std::move(collision.error()));
  bool const is_block = BlockCollisionSet.contains(*collision);
  if ((is_block && terrain_layer == TerrainLayer::Midground) || (!is_block && terrain_layer == TerrainLayer::Foreground))
    return false;

  auto layer = is_background ? TileLayer::Background : TileLayer::Foreground;
  auto material_hue = is_background ? tile.backgroundHueShift : tile.foregroundHueShift;
  auto color_variant = is_background ? tile.backgroundColorVariant : tile.foregroundColorVariant;
  auto mod = is_background ? tile.backgroundMod : tile.foregroundMod;
  auto mod_hue = is_background ? tile.backgroundModHueShift : tile.foregroundModHueShift;
  auto damage_level = byte_to_float(is_background ? tile.backgroundDamageLevel : tile.foregroundDamageLevel);
  auto damage_type = is_background ? tile.backgroundDamageType : tile.foregroundDamageType;
  auto color = Color::rgba(is_background ? m_background_layer_color : m_foreground_layer_color);

  bool occlude = false;
  MaterialPieceResultList pieces;
  auto add_pieces = [&](MaterialRenderProfile const& profile, bool is_mod) -> std::expected<void, StarException> {
    pieces.clear();
    auto matched = determine_matching_pieces(pieces, std::ref(occlude), material_database, profile, render_data, pos, layer, is_mod);
    if (!matched)
      return std::unexpected(std::move(matched.error()));
    if (pieces.empty())
      return {};

    auto selected_color = profile.world_color_variant(color_variant);
    auto hue = is_mod ? mod_hue : material_hue;
    auto z_level = is_mod ? mod_z_level(profile.z_level, mod, hue, selected_color)
                          : material_z_level(profile.z_level, material, hue, selected_color);
    auto& draw_list = drawables[z_level];
    auto variant_index = static_cast<int>(variant_layer.value_or(terrain_layer));
    auto wrapped_x = render_data.geometry.xwrap(pos[0]);
    auto variance = is_mod
        ? static_random_u32(wrapped_x, pos[1], variant_index, "mod")
        : static_random_u32(wrapped_x + variant_offset[0], pos[1] + variant_offset[1], variant_index, "main");
    auto directives = profile.color_directive(selected_color);
    auto origin = Vec2F(pos);
    for (auto const& [piece, offset] : pieces) {
      auto rect = piece.get().world_rectangle(variance, selected_color);
      if (!rect)
        continue;
      auto const& box = rect->get();
      auto path = hue == 0
          ? Star::format("{}?crop={};{};{};{}", piece.get().texture, box.x_min(), box.y_min(), box.x_max(), box.y_max())
          : Star::format("{}?crop={};{};{};{}?hueshift={}", piece.get().texture, box.x_min(), box.y_min(), box.x_max(), box.y_max(),
                material_hue_to_degrees(hue));
      AssetPath image{std::string_view(path)};
      if (directives)
        image.directives += directives->get();
      auto drawable = Drawable::make_image(*image_metadata, std::move(image), scale, false, offset * scale + origin, color);
      if (!drawable)
        return std::unexpected(std::move(drawable.error()));
      draw_list.emplace_back(std::move(*drawable));
    }
    return {};
  };

  auto material_profile = material_database.material_render_profile(material);
  if (material_profile) {
    occlude = material_profile->get().occludes_behind;
    auto added = add_pieces(material_profile->get(), false);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }
  if (auto mod_profile = material_database.mod_render_profile(mod)) {
    auto added = add_pieces(mod_profile->get(), true);
    if (!added)
      return std::unexpected(std::move(added.error()));
  }

  if (material_profile && damage_level > 0 && is_block) {
    auto& draw_list = drawables[damage_z_level()];
    auto cracking_image = material_profile->get().damage_image(damage_level, damage_type);
    if (!cracking_image)
      return std::unexpected(std::move(cracking_image.error()));
    auto const& [image, offset] = cracking_image->get();
    auto drawable = Drawable::make_image(*image_metadata, image, scale, false, offset * scale + Vec2F(pos), color);
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    draw_list.emplace_back(std::move(*drawable));
  }

  return occlude;
}

WorldRenderData& TileDrawer::render_data() {
  return m_temp_render_data;
}

std::unique_lock<std::mutex> TileDrawer::lock_render_data() {
  return std::unique_lock(m_temp_render_data_mutex);
}

RenderTile const& TileDrawer::get_render_tile(WorldRenderData const& render_data, Vec2I const& world_pos) {
  auto const& tile_min = render_data.tile_min_position;
  int x = render_data.geometry.pdiff(world_pos[0], tile_min[0]);
  int y = world_pos[1] - tile_min[1];
  auto const& size = render_data.tiles.extents();
  if (x >= 0 && y >= 0 && std::cmp_less(x, size[0]) && std::cmp_less(y, size[1]))
    return render_data.tiles(Vec2S(x, y));

  return default_render_tile;
}

TileDrawer::QuadZLevel TileDrawer::material_z_level(std::uint32_t z_level, MaterialId material, MaterialHue hue, MaterialColorVariant color_variant) {
  return QuadZLevel(color_variant) | (QuadZLevel(hue) << 8) | (QuadZLevel(material) << 16) | (QuadZLevel(z_level) << 32);
}

TileDrawer::QuadZLevel TileDrawer::mod_z_level(std::uint32_t z_level, ModId mod, MaterialHue hue, MaterialColorVariant color_variant) {
  return material_z_level(z_level, mod, hue, color_variant) | (QuadZLevel(1) << 63);
}

TileDrawer::QuadZLevel TileDrawer::damage_z_level() {
  return std::numeric_limits<QuadZLevel>::max();
}

struct TileDrawer::MatchEvaluator {
  struct LayerTile {
    std::uint16_t id;
    MaterialHue hue;
  };

  MaterialPieceResultList& result_list;
  std::optional<std::reference_wrapper<bool>> occlude;
  MaterialDatabase const& material_database;
  MaterialRenderProfile const& profile;
  WorldRenderData const& render_data;
  Vec2I const& base_pos;
  TileLayer layer;
  bool is_mod;
  LayerTile base_layer;

  LayerTile select_layer_tile(RenderTile const& tile, MaterialId material) const {
    if (layer == TileLayer::Foreground)
      return is_mod ? LayerTile{tile.foregroundMod, tile.foregroundModHueShift} : LayerTile{material, tile.foregroundHueShift};
    return is_mod ? LayerTile{tile.backgroundMod, tile.backgroundModHueShift} : LayerTile{material, tile.backgroundHueShift};
  }

  std::expected<bool, StarException> property_matches(MaterialRule::RulePropertyEquals const& rule, std::uint16_t neighbor_id) const {
    auto neighbor_profile = is_mod ? material_database.mod_render_profile(neighbor_id)
                                   : material_database.material_render_profile(neighbor_id);
    if (!neighbor_profile)
      return false;

    auto const& properties = neighbor_profile->get().rule_properties;
    auto property = properties.find(rule.property_name);
    if (!property)
      return std::unexpected(std::move(property.error()));
    if (*property)
      return (*property)->get() == rule.compare;
    if (is_mod)
      return rule.compare.is_null();
    // Use the ordinary strict keyed lookup diagnostic for a missing material property.
    return std::unexpected(properties.get(rule.property_name).error());
  }

  std::expected<bool, StarException> matches_set(MaterialRenderMatch const& match) const {
    if (match.required_layer && *match.required_layer != layer)
      return false;
    auto points = profile.match_points(match);
    if (points.empty())
      return true;

    bool const all = match.match_join == MaterialJoinType::All;
    bool valid = all;
    for (auto const& point : points) {
      auto const& rule = profile.rule(point.rule_index);
      bool const all_rules = rule.join == MaterialJoinType::All;
      bool rules_valid = all_rules;
      if (!rule.entries.empty()) {
        auto const& neighbor = get_render_tile(render_data, base_pos + point.position);
        auto neighbor_material = layer == TileLayer::Foreground ? neighbor.foreground : neighbor.background;
        auto neighbor_layer = select_layer_tile(neighbor, neighbor_material);
        std::optional<bool> shadows;
        for (auto const& entry : rule.entries) {
          bool matches;
          if (std::holds_alternative<MaterialRule::RuleEqualsSelf>(entry.rule)) {
            auto const& equals = std::get<MaterialRule::RuleEqualsSelf>(entry.rule);
            matches = neighbor_layer.id == base_layer.id && (!equals.match_hue || neighbor_layer.hue == base_layer.hue);
          } else if (std::holds_alternative<MaterialRule::RuleConnects>(entry.rule)) {
            // A mod connects based on its supporting material, not the neighbor mod.
            matches = is_connectable_material(neighbor_material);
          } else if (std::holds_alternative<MaterialRule::RuleEqualsId>(entry.rule)) {
            matches = neighbor_layer.id == std::get<MaterialRule::RuleEqualsId>(entry.rule).id;
          } else if (std::holds_alternative<MaterialRule::RuleShadows>(entry.rule)) {
            if (layer == TileLayer::Background && !shadows) {
              auto foreground_profile = material_database.material_render_profile(neighbor.foreground);
              shadows = foreground_profile && !foreground_profile->get().foreground_light_transparent;
            }
            matches = shadows.value_or(false);
          } else {
            auto const& property = std::get<MaterialRule::RulePropertyEquals>(entry.rule);
            auto result = property_matches(property, neighbor_layer.id);
            if (!result)
              return std::unexpected(std::move(result.error()));
            matches = *result;
          }

          bool const accepted = matches != entry.inverse;
          if (all_rules && !accepted) {
            rules_valid = false;
            break;
          }
          // An Any join must keep evaluating rules even after an entry succeeds.
          rules_valid = rules_valid || accepted;
        }
      }
      if (all && !rules_valid)
        return false;
      // Likewise, a later point can fail even after an Any join is already true.
      valid = valid || rules_valid;
    }
    return valid;
  }

  std::expected<bool, StarException> append_matches(std::span<MaterialRenderMatch const> matches) const {
    bool matched = false;
    for (auto const& match : matches) {
      auto accepted = matches_set(match);
      if (!accepted)
        return std::unexpected(std::move(accepted.error()));
      if (!*accepted)
        continue;

      matched = true;
      if (occlude && match.occlude)
        occlude->get() = *match.occlude;
      for (auto const& placement : profile.placements(match))
        result_list.emplace_back(std::cref(profile.piece(placement.piece_index)), placement.position);

      auto children = profile.sub_matches(match);
      if (!children.empty()) {
        auto child_matched = append_matches(children);
        if (!child_matched)
          return std::unexpected(std::move(child_matched.error()));
        if (*child_matched && match.halt_on_sub_match)
          break;
      }
      if (match.halt_on_match)
        break;
    }
    return matched;
  }
};

std::expected<bool, StarException> TileDrawer::determine_matching_pieces(
    MaterialPieceResultList& result_list, std::optional<std::reference_wrapper<bool>> occlude, MaterialDatabase const& material_database,
    MaterialRenderProfile const& profile, WorldRenderData const& render_data, Vec2I const& base_pos, TileLayer layer, bool is_mod) {
  auto matches = profile.main_matches();
  if (matches.empty())
    return false;

  auto const& tile = get_render_tile(render_data, base_pos);
  MatchEvaluator evaluator{result_list, occlude, material_database, profile, render_data, base_pos, layer, is_mod, {}};
  evaluator.base_layer = evaluator.select_layer_tile(tile, layer == TileLayer::Foreground ? tile.foreground : tile.background);
  return evaluator.append_matches(matches);
}

}
