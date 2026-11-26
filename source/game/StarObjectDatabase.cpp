import star.output_proxy;
import star.memory;
import star.json_extra;
import star.json;
import star.id_map;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.ttl_cache;
import star.game_types;
import star.material_types;
import star.data_stream;
import star.bi_map;
import star.string;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.set;
import star.vector;
import star.strong_typedef;
import star.casting;
import star.rpc_promise;
import star.logging;
import star.math_common;
import star.periodic;
import star.lua;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;
import star.weighted_pool;

import star.lua_root;
import star.utility_lua_bindings;
import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.tile_damage;
import star.damage_types;
import star.status_types;
import star.entity_rendering_types;
import star.lua_components;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.entity_rendering;
import star.object;
import star.world_geometry;
import star.damage;
import star.entity;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.container_entity;
import star.item_recipe;
import star.container_object;
import star.collision_block;

import star.object_database;
import star.teleporter_object;
import star.physics_object;
import star.loungeable_object;
import star.farmable_object;
import star.material_database;
import star.image_metadata_database;
import star.rebuilder;

import star.light_source;

import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.algorithm;
namespace Star {

std::expected<ObjectOrientation::ParticleEmissionEntry, StarException> ObjectOrientation::parse_particle_emitter(Root& root, String const& path, Json const& config) {
  auto rate = config.get_float("emissionRate", 0.0f);
  if (!rate)
    return std::unexpected(std::move(rate.error()).with_context("Reading object emissionRate"));
  if (!is_finite(*rate))
    return std::unexpected(ObjectException::format("Object '{}' particle emissionRate must be finite", path));
  auto variance = config.get_float("emissionVariance", 0.0f);
  if (!variance)
    return std::unexpected(std::move(variance.error()).with_context("Reading object emissionVariance"));
  if (!is_finite(*variance))
    return std::unexpected(ObjectException::format("Object '{}' particle emissionVariance must be finite", path));
  // Nonpositive base rates disable emission. Stock emitters can legitimately
  // sample zero or a negative rate; the signed cooldown is handled at birth.
  if (*rate > 0.0f) {
    if (*variance < 0.0f)
      return std::unexpected(ObjectException::format("Object '{}' enabled emissionVariance must be nonnegative", path));
    constexpr double max_rate = std::numeric_limits<float>::max();
    constexpr double min_rate = 1.0 / max_rate;
    double minimum = static_cast<double>(*rate) - *variance;
    double maximum = static_cast<double>(*rate) + *variance;
    if (maximum > max_rate || *rate < min_rate || (minimum > 0.0 && minimum < min_rate))
      return std::unexpected(ObjectException::format("Object '{}' particle emissionRate +/- emissionVariance has an out-of-range rate or cooldown", path));
  }
  ParticleEmissionEntry result;
  result.particle_emission_rate = *rate;
  result.particle_emission_rate_variance = *variance;
  auto particle_config_result = config.get_object("particle", JsonObject());
  if (!particle_config_result)
    return std::unexpected(std::move(particle_config_result.error()));
  auto particle_config = std::move(*particle_config_result);
  auto particle_result = Particle::create(root, particle_config, path);
  if (!particle_result)
    return std::unexpected(std::move(particle_result.error()));
  auto particle = std::move(*particle_result);
  result.particle = std::move(particle);
  auto particle_variance_config_result = config.get_object("particleVariance", JsonObject());
  if (!particle_variance_config_result)
    return std::unexpected(std::move(particle_variance_config_result.error()));
  auto particle_variance_config = std::move(*particle_variance_config_result);
  auto particle_variance_result = Particle::create(root, particle_variance_config, path);
  if (!particle_variance_result)
    return std::unexpected(std::move(particle_variance_result.error()));
  auto particle_variance = std::move(*particle_variance_result);
  result.particle_variance = std::move(particle_variance);
  auto origin_config_result = config.get("pixelOrigin", JsonArray{tile_pixels / 2, tile_pixels / 2});
  if (!origin_config_result)
    return std::unexpected(std::move(origin_config_result.error()));
  auto origin_config = std::move(*origin_config_result);
  auto origin_result = json_to_vec2f(origin_config);
  if (!origin_result)
    return std::unexpected(std::move(origin_result.error()));
  auto origin = std::move(*origin_result);
  result.particle.position += origin / tile_pixels;
  auto place_in_spaces_result = config.get_bool("placeInSpaces", false);
  if (!place_in_spaces_result)
    return std::unexpected(std::move(place_in_spaces_result.error()));
  auto place_in_spaces = std::move(*place_in_spaces_result);
  result.place_in_spaces = place_in_spaces;
  return result;
}

std::expected<bool, StarException> ObjectOrientation::placement_valid(World const& world, Vec2I const& position) const {
  for (Vec2I space : spaces) {
    space += position;
    auto occupied_result = world.tileIsOccupied(space, TileLayer::Foreground, false, true);
    if (!occupied_result)
      return std::unexpected(std::move(occupied_result.error()));
    auto occupied = std::move(*occupied_result);
    if (occupied)
      return false;
    auto protected_tile = world.isTileProtected(space);
    if (protected_tile)
      return false;
  }
  return true;
}

std::expected<bool, StarException> ObjectOrientation::anchors_valid(World const& world, Vec2I const& position) const {
  if (anchors.empty())
    return true;
  auto material_database_result = world.root().material_database();
  if (!material_database_result)
    return std::unexpected(std::move(material_database_result.error()));
  auto material_database = std::move(*material_database_result);
  bool any_valid = false;
  for (auto const& anchor : anchors) {
    auto space = position + anchor.position;
    auto valid = [&]() -> std::expected<bool, StarException> {
      auto connectable_result = world.isTileConnectable(space, anchor.layer);
      if (!connectable_result)
        return std::unexpected(std::move(connectable_result.error()));
      auto connectable = std::move(*connectable_result);
      if (!connectable)
        return false;
      if (anchor.tilled) {
        auto tilled_result = material_database->is_tilled_mod(world.mod(space, anchor.layer));
        if (!tilled_result)
          return std::unexpected(std::move(tilled_result.error()));
        auto tilled = std::move(*tilled_result);
        if (!tilled)
          return false;
      }
      if (anchor.soil) {
        auto soil_result = material_database->is_soil(world.material(space, anchor.layer));
        if (!soil_result)
          return std::unexpected(std::move(soil_result.error()));
        auto soil = std::move(*soil_result);
        if (!soil)
          return false;
      }
      if (anchor.material && world.material(space, anchor.layer) != *anchor.material)
        return false;
      return true;
    }();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    if (*valid)
      any_valid = true;
    else if (!anchor_any)
      return false;
  }
  return any_valid;
}

std::expected<std::size_t, StarException> ObjectConfig::find_valid_orientation(
    std::span<std::shared_ptr<ObjectOrientation> const> orientations, World const& world, Vec2I const& position,
    std::optional<Direction> direction_affinity) {
  // Each candidate belongs to exactly one pass. Within a pass keep asset order.
  for (int pass = 0; pass < (direction_affinity ? 2 : 1); ++pass) {
    for (std::size_t i = 0; i < orientations.size(); ++i) {
      auto const& orientation = *orientations[i];
      if (direction_affinity && (orientation.direction_affinity == direction_affinity) != (pass == 0))
        continue;
      auto placement = orientation.placement_valid(world, position);
      if (!placement) return std::unexpected(std::move(placement.error()));
      if (!*placement) continue;
      auto anchored = orientation.anchors_valid(world, position);
      if (!anchored) return std::unexpected(std::move(anchored.error()));
      if (*anchored) return i;
    }
  }
  return npos;
}

std::expected<Json, StarException> ObjectDatabase::parse_touch_damage(Root& root, String const& path, Json const& config) {
  auto touch_damage_result = config.get("touchDamage", Json());
  if (!touch_damage_result)
    return std::unexpected(std::move(touch_damage_result.error()));
  auto touch_damage = std::move(*touch_damage_result);
  if (touch_damage.is_type(Json::Type::String)) {
    auto assets_result = root.assets();
    if (!assets_result)
      return std::unexpected(std::move(assets_result.error()));
    auto assets = std::move(*assets_result);
    auto damage_path_result = touch_damage.to_string();
    if (!damage_path_result)
      return std::unexpected(std::move(damage_path_result.error()));
    auto damage_path = AssetPath::relative_to(path, std::move(*damage_path_result));
    auto damage = assets->json(damage_path);
    if (!damage)
      return std::unexpected(assets->with_asset_context(std::move(damage.error()), damage_path, "Loading object touch damage"));
    return damage;
  }
  return touch_damage;
}

namespace {

std::expected<StringMap<String>, StarException> orientation_image_keys(Json const& orientation, Json const& base, std::same_as<Json> auto const&... overrides) {
  auto color_config = orientation.get("color", "default");
  if (!color_config)
    return std::unexpected(std::move(color_config.error()));
  auto color = color_config->to_string();
  if (!color)
    return std::unexpected(std::move(color.error()));
  StringMap<String> keys;
  keys.set("color", std::move(*color));
  auto merged = json_merge_query_def("imageKeys", JsonObject(), base, orientation, overrides...);
  if (!merged)
    return std::unexpected(std::move(merged.error()));
  auto entries = merged->iterate_object();
  if (!entries)
    return std::unexpected(std::move(entries.error()));
  for (auto const& [key, entry] : *entries) {
    auto value = entry.to_string();
    if (!value)
      return std::unexpected(std::move(value.error()));
    keys.set(key, std::move(*value));
  }
  return keys;
}

std::expected<void, StarException> read_particle_emitters(Root& root, String const& path, Json const& config,
    List<ObjectOrientation::ParticleEmissionEntry>& emitters) {
  auto single = config.find("particleEmitter");
  if (!single)
    return std::unexpected(std::move(single.error()));
  if (*single) {
    auto emitter = ObjectOrientation::parse_particle_emitter(root, path, (*single)->get());
    if (!emitter)
      return std::unexpected(std::move(emitter.error()));
    emitters.push_back(std::move(*emitter));
  }
  auto multiple = config.opt("particleEmitters");
  if (!multiple)
    return std::unexpected(std::move(multiple.error()));
  if (*multiple) {
    auto entries = (*multiple)->iterate_array();
    if (!entries)
      return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto emitter = ObjectOrientation::parse_particle_emitter(root, path, entry);
      if (!emitter)
        return std::unexpected(std::move(emitter.error()));
      emitters.push_back(std::move(*emitter));
    }
  }
  return {};
}

}

std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> ObjectDatabase::parse_orientations(
    Root& root, String const& path, Json const& config_list, Json const& base_config) {
  return parse_orientations_impl(root, path, config_list, base_config, true);
}

std::expected<List<std::shared_ptr<ObjectOrientation>>, StarException> ObjectDatabase::parse_orientations_impl(
    Root& root, String const& path, Json const& config_list, Json const& base_config, bool load_emitters) {
  auto material_database_result = root.material_database();
  if (!material_database_result)
    return std::unexpected(std::move(material_database_result.error()));
  auto material_database = std::move(*material_database_result);
  auto image_metadata_result = root.image_metadata_database();
  if (!image_metadata_result)
    return std::unexpected(std::move(image_metadata_result.error()));
  auto image_metadata = std::move(*image_metadata_result);
  List<std::shared_ptr<ObjectOrientation>> res;
  auto source_configs = config_list.array_ptr();
  if (!source_configs)
    return std::unexpected(std::move(source_configs.error()));
  auto const& configs = **source_configs;
  // Reject malformed entries before parsing any orientation-specific fields.
  std::size_t legacy_count = 0;
  for (auto const& entry : configs) {
    auto object = entry.object_ptr();
    if (!object)
      return std::unexpected(std::move(object.error()));
    legacy_count += (*object)->contains("dualImage") || (*object)->contains("leftImage");
  }
  JsonArray expanded;
  if (legacy_count) {
    expanded.reserve(configs.size() + legacy_count);
    for (auto const& entry : configs) {
      auto const& object = **entry.object_ptr();
      auto dual = object.find("dualImage");
      auto left = dual != object.end() ? dual : object.find("leftImage");
      if (left == object.end()) {
        expanded.emplace_back(entry);
        continue;
      }
      JsonObject left_config = object;
      left_config["image"] = left->second;
      if (dual != object.end())
        left_config["flipImages"] = true;
      left_config["direction"] = "left";
      expanded.emplace_back(std::move(left_config));
      JsonObject right_config = object;
      right_config["image"] = dual != object.end() ? dual->second : object.value("rightImage");
      right_config["direction"] = "right";
      expanded.emplace_back(std::move(right_config));
    }
  }
  auto const& ordered_configs = legacy_count ? expanded : configs;
  res.reserve(ordered_configs.size());
  for (auto const& orientation_settings : ordered_configs) {
    auto orientation = std::make_shared<ObjectOrientation>();
    orientation->config = orientation_settings;
    if (auto contains_result = orientation_settings.contains("imageLayers"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto layers_result = orientation_settings.get_array("imageLayers");
      if (!layers_result)
        return std::unexpected(std::move(layers_result.error()));
      auto layers = std::move(*layers_result);
      for (Json layer : layers) {
        auto image_result = layer.opt("image");
        if (!image_result)
          return std::unexpected(std::move(image_result.error()));
        auto image = std::move(*image_result);
        if (image) {
          auto image_name_result = image->to_string();
          if (!image_name_result)
            return std::unexpected(std::move(image_name_result.error()));
          auto image_name = std::move(*image_name_result);
          auto resolved_layer_result = layer.set("image", AssetPath::relative_to(path, image_name));
          if (!resolved_layer_result)
            return std::unexpected(std::move(resolved_layer_result.error()));
          auto resolved_layer = std::move(*resolved_layer_result);
          layer = std::move(resolved_layer);
        }
        auto centered_result = layer.get_bool("centered", false);
        if (!centered_result)
          return std::unexpected(std::move(centered_result.error()));
        auto centered = std::move(*centered_result);
        auto drawable_config_result = layer.set("centered", centered);
        if (!drawable_config_result)
          return std::unexpected(std::move(drawable_config_result.error()));
        auto drawable_config = std::move(*drawable_config_result);
        auto drawable_result = Drawable::create(*image_metadata, drawable_config);
        if (!drawable_result)
          return std::unexpected(std::move(drawable_result.error()));
        auto drawable = std::move(*drawable_result);
        drawable.scale(1.0f / tile_pixels);
        orientation->image_layers.push_back(std::move(drawable));
      }
    } else {
      auto image_result = orientation_settings.get_string("image");
      if (!image_result)
        return std::unexpected(std::move(image_result.error()));
      auto image = std::move(*image_result);
      auto drawable_result =
        Drawable::make_image(*image_metadata, AssetPath::relative_to(path, image), 1.0 / tile_pixels, false, {});
      if (!drawable_result)
        return std::unexpected(std::move(drawable_result.error()));
      auto drawable = std::move(*drawable_result);
      auto fullbright_result = orientation_settings.get_bool("fullbright", false);
      if (!fullbright_result)
        return std::unexpected(std::move(fullbright_result.error()));
      auto fullbright = std::move(*fullbright_result);
      drawable.fullbright = fullbright;
      orientation->image_layers.push_back(std::move(drawable));
    }
    auto render_layer_name_result = orientation_settings.get_string("renderLayer", "Object");
    if (!render_layer_name_result)
      return std::unexpected(std::move(render_layer_name_result.error()));
    auto render_layer_name = std::move(*render_layer_name_result);
    auto render_layer_result = parse_render_layer(render_layer_name);
    if (!render_layer_result)
      return std::unexpected(std::move(render_layer_result.error()));
    auto render_layer = std::move(*render_layer_result);
    orientation->render_layer = std::move(render_layer);
    auto flip_images_result = orientation_settings.get_bool("flipImages", false);
    if (!flip_images_result)
      return std::unexpected(std::move(flip_images_result.error()));
    auto flip_images = std::move(*flip_images_result);
    orientation->flip_images = std::move(flip_images);
    auto image_position_config_result = orientation_settings.get_array("imagePosition", {0, 0});
    if (!image_position_config_result)
      return std::unexpected(std::move(image_position_config_result.error()));
    auto image_position_config = std::move(*image_position_config_result);
    auto image_position_result = json_to_vec2f(image_position_config);
    if (!image_position_result)
      return std::unexpected(std::move(image_position_result.error()));
    auto image_position = std::move(*image_position_result);
    orientation->image_position = image_position / tile_pixels;
    auto frames = orientation_settings.get_double("frames", 1.0);
    if (!frames)
      return std::unexpected(std::move(frames.error()).with_context("Reading object orientation frames"));
    if (!is_finite(*frames) || *frames < 1 || *frames > std::numeric_limits<int>::max() || *frames != std::trunc(*frames))
      return std::unexpected(ObjectException::format("Object '{}' orientation {} frames must be an integer in [1, {}]",
          path, res.size(), std::numeric_limits<int>::max()));
    orientation->frames = static_cast<int>(*frames);
    auto cycle = orientation_settings.get_float("animationCycle", 1.0f);
    if (!cycle)
      return std::unexpected(std::move(cycle.error()).with_context("Reading object orientation animationCycle"));
    if (!is_finite(*cycle) || *cycle <= 0.0f)
      return std::unexpected(ObjectException::format("Object '{}' orientation {} animationCycle must be finite and positive as a float",
          path, res.size()));
    orientation->animation_cycle = *cycle;
    if (auto contains_result = orientation_settings.contains("spaces"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto spaces_result = orientation_settings.get_array("spaces");
      if (!spaces_result)
        return std::unexpected(std::move(spaces_result.error()));
      auto spaces = std::move(*spaces_result);
      for (auto const& space : spaces) {
        auto parsed_space_result = json_to_vec2i(space);
        if (!parsed_space_result)
          return std::unexpected(std::move(parsed_space_result.error()));
        auto parsed_space = std::move(*parsed_space_result);
        orientation->spaces.push_back(parsed_space);
      }
    } else {
      orientation->spaces = {{0, 0}};
    }
    if (auto contains_result = orientation_settings.contains("spaceScan"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto space_scan_spaces = Set<Vec2I>::from(orientation->spaces);
      auto image_keys = orientation_image_keys(orientation_settings, base_config);
      if (!image_keys)
        return std::unexpected(std::move(image_keys.error()));
      for (auto const& layer : orientation->image_layers) {
        if (layer.is_image()) {
          auto image = AssetPath::join(layer.image_part().image).replace_tags(*image_keys, true, "default");
          auto threshold_result = orientation_settings.get_double("spaceScan");
          if (!threshold_result)
            return std::unexpected(std::move(threshold_result.error()));
          auto threshold = std::move(*threshold_result);
          auto scanned_result = image_metadata->image_spaces(image, image_position + (layer.position + layer.image_part().transformation.transform_point(Vec2F())) * tile_pixels, threshold, orientation->flip_images);
          if (!scanned_result)
            return std::unexpected(std::move(scanned_result.error()));
          auto scanned = std::move(*scanned_result);
          space_scan_spaces.insert_range(scanned);
        }
      }
      orientation->spaces = space_scan_spaces.values();
    }
    orientation->bound_box = RectI::bound_box_of_points(orientation->spaces);
    auto meta_box_result = orientation_settings.opt("metaBoundBox");
    if (!meta_box_result)
      return std::unexpected(std::move(meta_box_result.error()));
    auto meta_box = std::move(*meta_box_result);
    if (meta_box) {
      auto meta_bound_box_result = json_to_rectf(*meta_box);
      if (!meta_bound_box_result)
        return std::unexpected(std::move(meta_bound_box_result.error()));
      auto meta_bound_box = std::move(*meta_bound_box_result);
      orientation->meta_bound_box = std::move(meta_bound_box);
    }
    auto tilled_result = orientation_settings.get_bool("requireTilledAnchors", false);
    if (!tilled_result)
      return std::unexpected(std::move(tilled_result.error()));
    auto tilled = std::move(*tilled_result);
    auto soil_result = orientation_settings.get_bool("requireSoilAnchors", false);
    if (!soil_result)
      return std::unexpected(std::move(soil_result.error()));
    auto soil = std::move(*soil_result);
    std::optional<MaterialId> anchor_material;
    auto anchor_material_name_result = orientation_settings.opt_string("anchorMaterial");
    if (!anchor_material_name_result)
      return std::unexpected(std::move(anchor_material_name_result.error()));
    auto anchor_material_name = std::move(*anchor_material_name_result);
    if (anchor_material_name) {
      auto material_result = material_database->material_id(*anchor_material_name);
      if (!material_result)
        return std::unexpected(std::move(material_result.error()));
      auto material = std::move(*material_result);
      anchor_material = material;
    }
    auto anchors_result = orientation_settings.get_array("anchors", JsonArray());
    if (!anchors_result)
      return std::unexpected(std::move(anchors_result.error()));
    auto anchors = std::move(*anchors_result);
    for (auto const& type : anchors) {
      auto anchor_type_result = type.to_string();
      if (!anchor_type_result)
        return std::unexpected(std::move(anchor_type_result.error()));
      auto anchor_type = std::move(*anchor_type_result);
      if (anchor_type == "left") {
        for (auto space : orientation->spaces) {
          if (space[0] == orientation->bound_box.x_min())
            orientation->anchors.push_back({TileLayer::Foreground, space + Vec2I(-1, 0), tilled, soil, anchor_material});
        }
      } else if (anchor_type == "bottom") {
        for (auto space : orientation->spaces) {
          if (space[1] == orientation->bound_box.y_min())
            orientation->anchors.push_back({TileLayer::Foreground, space + Vec2I(0, -1), tilled, soil, anchor_material});
        }
      } else if (anchor_type == "right") {
        for (auto space : orientation->spaces) {
          if (space[0] == orientation->bound_box.x_max())
            orientation->anchors.push_back({TileLayer::Foreground, space + Vec2I(1, 0), tilled, soil, anchor_material});
        }
      } else if (anchor_type == "top") {
        for (auto space : orientation->spaces) {
          if (space[1] == orientation->bound_box.y_max())
            orientation->anchors.push_back({TileLayer::Foreground, space + Vec2I(0, 1), tilled, soil, anchor_material});
        }
      } else if (anchor_type == "background") {
        for (auto space : orientation->spaces)
          orientation->anchors.push_back({TileLayer::Background, space, tilled, soil, anchor_material});
      } else {
        return std::unexpected(ObjectException::format("Unknown anchor type: {}", anchor_type));
      }
    }
    auto bg_anchors_result = orientation_settings.get_array("bgAnchors", JsonArray());
    if (!bg_anchors_result)
      return std::unexpected(std::move(bg_anchors_result.error()));
    auto bg_anchors = std::move(*bg_anchors_result);
    for (auto const& anchor : bg_anchors) {
      auto position_result = json_to_vec2i(anchor);
      if (!position_result)
        return std::unexpected(std::move(position_result.error()));
      auto position = std::move(*position_result);
      orientation->anchors.push_back({TileLayer::Background, position, tilled, soil, anchor_material});
    }
    auto fg_anchors_result = orientation_settings.get_array("fgAnchors", JsonArray());
    if (!fg_anchors_result)
      return std::unexpected(std::move(fg_anchors_result.error()));
    auto fg_anchors = std::move(*fg_anchors_result);
    for (auto const& anchor : fg_anchors) {
      auto position_result = json_to_vec2i(anchor);
      if (!position_result)
        return std::unexpected(std::move(position_result.error()));
      auto position = std::move(*position_result);
      orientation->anchors.push_back({TileLayer::Foreground, position, tilled, soil, anchor_material});
    }
    auto anchor_any_result = orientation_settings.get_bool("anchorAny", false);
    if (!anchor_any_result)
      return std::unexpected(std::move(anchor_any_result.error()));
    auto anchor_any = std::move(*anchor_any_result);
    orientation->anchor_any = std::move(anchor_any);
    if (auto contains_result = orientation_settings.contains("direction"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto direction_result = orientation_settings.get_string("direction", "left");
      if (!direction_result)
        return std::unexpected(std::move(direction_result.error()));
      auto direction = std::move(*direction_result);
      auto affinity_result = enum_value<direction_names>(direction);
      if (!affinity_result)
        return std::unexpected(std::move(affinity_result.error()));
      orientation->direction_affinity = *affinity_result;
    }
    auto collision_type_result = orientation_settings.get_string("collision", "none");
    if (!collision_type_result)
      return std::unexpected(std::move(collision_type_result.error()));
    auto collision_type = std::move(*collision_type_result);
    if (auto contains_result = orientation_settings.contains("materialSpaces"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto material_spaces_result = orientation_settings.get_array("materialSpaces");
      if (!material_spaces_result)
        return std::unexpected(std::move(material_spaces_result.error()));
      auto material_spaces = std::move(*material_spaces_result);
      for (auto const& space : material_spaces) {
        auto material_name_result = space.get_string(1);
        if (!material_name_result)
          return std::unexpected(std::move(material_name_result.error()));
        auto material_name = std::move(*material_name_result);
        auto space_config_result = space.get(0);
        if (!space_config_result)
          return std::unexpected(std::move(space_config_result.error()));
        auto space_config = std::move(*space_config_result);
        auto space_position_result = json_to_vec2i(space_config);
        if (!space_position_result)
          return std::unexpected(std::move(space_position_result.error()));
        auto space_position = std::move(*space_position_result);
        auto material_result = material_database->material_id(material_name);
        if (!material_result)
          return std::unexpected(std::move(material_result.error()));
        auto material = std::move(*material_result);
        orientation->material_spaces.push_back({space_position, material});
      }
    } else if (collision_type == "solid" || collision_type == "platform") {
      auto material = collision_type == "solid" ? object_solid_material_id : object_platform_material_id;
      if (auto contains_result = orientation_settings.contains("collisionSpaces"); !contains_result)
        return std::unexpected(std::move(contains_result.error()));
      else if (*contains_result) {
        auto collision_spaces_result = orientation_settings.get_array("collisionSpaces");
        if (!collision_spaces_result)
          return std::unexpected(std::move(collision_spaces_result.error()));
        auto collision_spaces = std::move(*collision_spaces_result);
        for (auto const& space : collision_spaces) {
          auto space_position_result = json_to_vec2i(space);
          if (!space_position_result)
            return std::unexpected(std::move(space_position_result.error()));
          auto space_position = std::move(*space_position_result);
          orientation->material_spaces.push_back({space_position, material});
        }
      } else {
        for (auto const& space : orientation->spaces) {
          if (collision_type == "solid" || space[1] == orientation->bound_box.y_max())
            orientation->material_spaces.push_back({space, material});
        }
      }
    }
    if (auto contains_result = orientation_settings.contains("interactiveSpaces"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
      auto interactive_spaces_result = orientation_settings.get_array("interactiveSpaces");
      if (!interactive_spaces_result)
        return std::unexpected(std::move(interactive_spaces_result.error()));
      auto interactive_spaces = std::move(*interactive_spaces_result);
      List<Vec2I> parsed_spaces;
      for (auto const& space : interactive_spaces) {
        auto position_result = json_to_vec2i(space);
        if (!position_result)
          return std::unexpected(std::move(position_result.error()));
        auto position = std::move(*position_result);
        parsed_spaces.push_back(position);
      }
      orientation->interactive_spaces = std::move(parsed_spaces);
    }
    auto light_position_config_result = orientation_settings.get_array("lightPosition", {0, 0});
    if (!light_position_config_result)
      return std::unexpected(std::move(light_position_config_result.error()));
    auto light_position_config = std::move(*light_position_config_result);
    auto light_position_result = json_to_vec2f(light_position_config);
    if (!light_position_result)
      return std::unexpected(std::move(light_position_result.error()));
    auto light_position = std::move(*light_position_result);
    orientation->light_position = std::move(light_position);
    auto beam_angle = orientation_settings.get_float("beamAngle", 0.0f);
    if (!beam_angle)
      return std::unexpected(std::move(beam_angle.error()));
    orientation->beam_angle = *beam_angle * Constants::degrees_to_radians;
    // Placement only needs the effective geometry. In particular, parsing an
    // animated emitter there would consume the gameplay random stream.
    if (load_emitters) {
      auto emitters = read_particle_emitters(root, path, orientation_settings, orientation->particle_emitters);
      if (!emitters)
        return std::unexpected(std::move(emitters.error()).with_context(
            Star::format("Reading object '{}' orientation {} emitters", path, res.size())));
    }
    auto area_result = orientation_settings.opt("statusEffectArea");
    if (!area_result)
      return std::unexpected(std::move(area_result.error()));
    auto area = std::move(*area_result);
    if (area) {
      auto status_effect_area_result = json_to_polyf(*area);
      if (!status_effect_area_result)
        return std::unexpected(std::move(status_effect_area_result.error()));
      auto status_effect_area = std::move(*status_effect_area_result);
      orientation->status_effect_area = std::move(status_effect_area);
    }
    auto touch_damage_config_result = parse_touch_damage(root, path, orientation_settings);
    if (!touch_damage_config_result)
      return std::unexpected(std::move(touch_damage_config_result.error()));
    auto touch_damage_config = std::move(*touch_damage_config_result);
    orientation->touch_damage_config = std::move(touch_damage_config);
    res.push_back(std::move(orientation));
  }
  return res;
}

ObjectDatabase::ObjectDatabase(ConstructionKey, Root& root) : m_root(root), m_config_cache(root.presentationRandom()) {}

std::expected<std::shared_ptr<ObjectDatabase>, StarException> ObjectDatabase::create(Root& root) {
  auto database = std::make_shared<ObjectDatabase>(ConstructionKey{}, root);
  auto rebuilder = Rebuilder::create(root, "object");
  if (!rebuilder)
    return std::unexpected(std::move(rebuilder.error()));
  database->m_rebuilder = std::move(*rebuilder);
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan_extension("object");
  if (!files)
    return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued)
    return std::unexpected(std::move(queued.error()));
  for (auto const& entry : files->get()) {
    auto const& file = entry.string();
    auto name = [&]() -> std::expected<String, StarException> {
      auto config = (*assets)->json(file);
      if (!config)
        return std::unexpected(std::move(config.error()));
      return config->get_string("objectName");
    }();
    if (!name) {
      auto logged = root.logger().error_exception(
        Star::format("Error loading object file {}", file),
        ExceptionDiagnostic((*assets)->with_asset_context(std::move(name.error()), file, "Loading object")));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else if (database->m_paths.contains(*name)) {
      auto error = (*assets)->with_asset_context(ObjectException::format("Object {} defined twice, second time from {}", *name, file), file, "Loading object");
      auto logged = root.logger().error_exception("Error loading object", ExceptionDiagnostic(error));
      if (!logged)
        return std::unexpected(std::move(logged.error()));
    } else {
      database->m_paths[*name] = file;
    }
  }
  return database;
}

void ObjectDatabase::cleanup() {
  std::lock_guard locker(m_cache_mutex);
  m_config_cache.cleanup([](String const&, std::shared_ptr<ObjectConfig> const& config) {
      return config.use_count() != 1;
    });
}

StringList ObjectDatabase::all_objects() const {
  return m_paths.keys();
}

bool ObjectDatabase::is_object(String const& object_name) const {
  return m_paths.contains(object_name);
}

std::expected<std::shared_ptr<ObjectConfig>, StarException> ObjectDatabase::get_config(String const& object_name) const {
  std::unique_lock lock(m_cache_mutex);
  for (;;) {
    auto cached = m_config_cache.find_ref(object_name);
    if (!cached)
      return std::unexpected(std::move(cached.error()));
    if (*cached)
      return (*cached)->get();

    if (auto pending = m_config_loads.find_ref(object_name)) {
      auto load = pending->get();
      load->completed.wait(lock, [&] { return load->finished; });
      continue;
    }

    auto path = m_paths.find_ref(object_name);
    if (!path)
      return std::unexpected(ObjectException::format("No such object named '{}'", object_name));
    auto load = std::make_shared<ConfigLoad>();
    m_config_loads.set(object_name, load);
    auto finish = make_scope_exit([&] {
      if (!lock.owns_lock())
        lock.lock();
      m_config_loads.remove(object_name);
      load->finished = true;
      lock.unlock();
      load->completed.notify_all();
    });
    // Serialize only producers for this name. Asset and Root dependencies can
    // block, and are not entered while holding the cache mutex.
    lock.unlock();
    auto parsed = read_config(path->get());
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    lock.lock();
    auto result = m_config_cache.get(object_name, [&](String const&) { return std::move(*parsed); });
    if (!result)
      return std::unexpected(std::move(result.error()));
    return result->get();
  }
}

std::expected<std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>, StarException> ObjectDatabase::get_orientations(String const& object_name) const {
  auto config = get_config(object_name);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto const& orientations = (*config)->orientations;
  return std::shared_ptr<List<std::shared_ptr<ObjectOrientation>> const>(std::move(*config), std::addressof(orientations));
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::create_object(
    String const& object_name, Json const& parameters) const {
  auto config = get_config(object_name);
  if (!config)
    return std::unexpected(std::move(config.error()));
  return construct_object(*config, parameters);
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::construct_object(
    std::shared_ptr<ObjectConfig const> const& config, Json const& parameters) const {
  auto created = [&]() -> std::expected<std::shared_ptr<Object>, StarException> {
    auto const& type = config->type;
    if (type == "object")
      return Object::create(m_root, config, parameters);
    if (type == "loungeable")
      return LoungeableObject::create(m_root, config, parameters);
    if (type == "container")
      return ContainerObject::create(m_root, config, parameters);
    if (type == "farmable")
      return FarmableObject::create(m_root, config, parameters);
    if (type == "teleporter")
      return TeleporterObject::create(m_root, config, parameters);
    if (type == "physics")
      return PhysicsObject::create(m_root, config, parameters);
    return std::unexpected(ObjectException::format("Unknown objectType '{}' constructing object '{}'", type, config->name));
  }();
  if (!created) {
    auto assets = m_root.assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    return std::unexpected((*assets)->with_asset_context(std::move(created.error()), config->path, "Constructing object"));
  }
  return created;
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::disk_load_object(Json const& disk_store) const {
  auto original_name = disk_store.get_string("name");
  if (!original_name)
    return std::unexpected(std::move(original_name.error()));
  auto original_params = disk_store.get("parameters");
  if (!original_params)
    return std::unexpected(std::move(original_params.error()));
  Json new_store = disk_store;
  std::shared_ptr<Object> object;
  auto attempt = [&, &database = *this](Json const& store) -> std::expected<void, StarException> {
    auto name = store.get_string("name");
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto parameters = store.get("parameters");
    if (!parameters)
      return std::unexpected(std::move(parameters.error()));
    auto candidate = database.create_object(*name, *parameters);
    if (!candidate)
      return std::unexpected(std::move(candidate.error()));
    auto loaded = (*candidate)->read_stored_data(store);
    if (!loaded)
      return std::unexpected(std::move(loaded.error()));
    auto state = (*candidate)->set_net_states();
    if (!state)
      return std::unexpected(std::move(state.error()));
    object = std::move(*candidate);
    return {};
  };
  auto loaded = [&]() -> std::expected<void, StarException> {
    if (*original_name == "perfectlygenericitem") {
      auto contains = original_params->contains("genericItemStorage");
      if (!contains)
        return std::unexpected(std::move(contains.error()));
      if (*contains) {
        auto stored = disk_store.get("genericItemStorage");
        if (!stored)
          return std::unexpected(std::move(stored.error()));
        new_store = std::move(*stored);
      }
    }
    return attempt(new_store);
  }();
  if (loaded)
    return object;
  auto original_error = std::move(loaded.error());
  auto rebuilt = m_rebuilder->rebuild(new_store, original_error, attempt);
  if (!rebuilt)
    return std::unexpected(std::move(rebuilt.error()));
  if (*rebuilt)
    return object;
  if (m_root.logger().loggable(LogLevel::Debug)) {
    auto stored_text = disk_store.repr();
    if (!stored_text)
      return std::unexpected(std::move(stored_text.error()));
    if (auto details = m_root.logger().debug("Object disk store: {}", *stored_text); !details)
      return std::unexpected(std::move(details.error()));
  }
  if (*original_name == "perfectlygenericitem") {
    auto logged = m_root.logger().error_exception(Star::format("Could not re-instantiate object '{}'", *original_name),
                                                  ExceptionDiagnostic(original_error));
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return create_object(*original_name, *original_params);
  }
  auto logged = m_root.logger().error_exception(Star::format("Could not instantiate object '{}'", *original_name),
                                                ExceptionDiagnostic(original_error));
  if (!logged)
    return std::unexpected(std::move(logged.error()));
  return create_object("perfectlygenericitem", JsonObject{
      {"genericItemStorage", disk_store},
      {"shortdescription", *original_name},
      {"description", "Reinstall the parent mod to return this item to normal"}
    });
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::net_load_object(ByteArray const& net_store, NetCompatibilityRules rules) const {
  DataStreamReader ds(net_store.span());
  ds.set_stream_compatibility_version(rules);
  auto name = ds.read<String>();
  if (!name)
    return std::unexpected(std::move(name.error()));
  auto parameters = ds.read<Json>();
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  return create_object(*name, *parameters);
}

namespace {

std::optional<std::reference_wrapper<Json const>> custom_orientation_overrides(Json const& parameters) {
  if (parameters.is_type(Json::Type::Object)) {
    auto custom = parameters.find("customOrientations");
    if (*custom && (*custom)->get().is_type(Json::Type::Array))
      return *custom;
  }
  return {};
}

}

std::expected<std::optional<List<std::shared_ptr<ObjectOrientation>>>, StarException>
ObjectDatabase::parse_custom_orientations(Root& root, ObjectConfig const& config, Json const& parameters, bool load_emitters) {
  auto overrides = custom_orientation_overrides(parameters);
  if (!overrides)
    return std::nullopt;
  auto base = config.config.get_array("orientations");
  if (!base)
    return std::unexpected(std::move(base.error()));
  auto overrides_array = overrides->get().array_ptr();
  if (!overrides_array)
    return std::unexpected(std::move(overrides_array.error()));
  auto const& entries = **overrides_array;
  if (entries.size() > base->size())
    base->resize(entries.size());
  for (std::size_t i = 0; i < entries.size(); ++i)
    (*base)[i] = json_merge_nulling((*base)[i], entries[i]);
  auto parsed = parse_orientations_impl(root, config.path, Json(std::move(*base)), config.config, load_emitters);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  return std::optional(std::move(*parsed));
}

std::expected<bool, StarException> ObjectDatabase::can_place_object(World const& world, Vec2I const& position,
    String const& object_name, Json const& parameters, std::optional<Direction> direction_affinity) const {
  auto config = get_config(object_name);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto custom = parse_custom_orientations(m_root, **config, parameters, false);
  if (!custom)
    return std::unexpected(std::move(custom.error()));
  auto const& orientations = *custom ? **custom : (*config)->orientations;
  auto index = ObjectConfig::find_valid_orientation(
      orientations, world, world.geometry().xwrap(position), direction_affinity);
  if (!index)
    return std::unexpected(std::move(index.error()));
  return *index != npos;
}

std::expected<std::shared_ptr<Object>, StarException> ObjectDatabase::create_for_placement(World const& world,
    String const& object_name, Vec2I const& position, Direction direction, Json const& parameters) const {
  auto config = get_config(object_name);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto wrapped = world.geometry().xwrap(position);
  auto find_orientation = [&](std::span<std::shared_ptr<ObjectOrientation> const> orientations) {
    return ObjectConfig::find_valid_orientation(orientations, world, wrapped, direction);
  };
  auto custom = custom_orientation_overrides(parameters);
  if (!custom) {
    // Avoid constructing an object for a rejected ordinary placement.
    auto index = find_orientation((*config)->orientations);
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (*index == npos)
      return std::shared_ptr<Object>{};
  }
  auto object = construct_object(*config, parameters);
  if (!object)
    return std::unexpected(std::move(object.error()));
  if (custom) {
    // Reuse the parsed instance orientations; reparsing may resample animated particle definitions.
    auto index = find_orientation((*object)->get_orientations());
    if (!index)
      return std::unexpected(std::move(index.error()));
    if (*index == npos)
      return std::shared_ptr<Object>{};
  }
  auto positioned = (*object)->set_tile_position(wrapped);
  if (!positioned)
    return std::unexpected(std::move(positioned.error()));
  (*object)->set_direction(direction);
  return std::move(*object);
}

namespace {

std::expected<void, StarException> read_object_damage_team(Json const& config, EntityDamageTeam& team) {
  auto present = config.contains("damageTeam");
  if (!present)
    return std::unexpected(std::move(present.error()));
  if (!*present)
    return {};
  auto damage_team = config.get("damageTeam");
  if (!damage_team)
    return std::unexpected(std::move(damage_team.error()));
  auto type = damage_team->get_string("type", "environment")
                .and_then([](String const& name) -> std::expected<TeamType, StarException> {
                  return enum_value<TeamTypeNames>(name);
                });
  if (!type)
    return std::unexpected(std::move(type.error()));
  team.type = *type;
  auto number = damage_team->get_uint("team", 0);
  if (!number)
    return std::unexpected(std::move(number.error()));
  team.team = *number;
  return {};
}

std::expected<void, StarException> read_object_lighting(Root& root, Json const& config, ObjectConfig& object) {
  auto single_color = config.contains("lightColor");
  if (!single_color)
    return std::unexpected(std::move(single_color.error()));
  if (*single_color) {
    auto color = config.get("lightColor").and_then(json_to_color);
    if (!color)
      return std::unexpected(std::move(color.error()));
    object.light_colors["default"] = *color;
  } else {
    auto multiple_colors = config.contains("lightColors");
    if (!multiple_colors)
      return std::unexpected(std::move(multiple_colors.error()));
    if (*multiple_colors) {
      auto colors = config.get_object("lightColors");
      if (!colors)
        return std::unexpected(std::move(colors.error()));
      for (auto const& [name, value] : *colors) {
        auto color = json_to_color(value);
        if (!color)
          return std::unexpected(std::move(color.error()));
        object.light_colors[name] = *color;
      }
    }
  }
  auto type = parse_light_type(config);
  if (!type)
    return std::unexpected(std::move(type.error()));
  object.light_type = *type;
  auto point_beam = config.get_float("pointBeam", 0.0f);
  if (!point_beam)
    return std::unexpected(std::move(point_beam.error()));
  object.point_beam = *point_beam;
  auto ambience = config.get_float("beamAmbience", 0.0f);
  if (!ambience)
    return std::unexpected(std::move(ambience.error()));
  object.beam_ambience = *ambience;
  auto flickering = config.contains("flickerPeriod");
  if (!flickering)
    return std::unexpected(std::move(flickering.error()));
  if (*flickering) {
    auto period = config.get_float("flickerPeriod");
    if (!period)
      return std::unexpected(std::move(period.error()));
    auto minimum = config.get_float("flickerMinIntensity", 0.0);
    if (!minimum)
      return std::unexpected(std::move(minimum.error()));
    auto maximum = config.get_float("flickerMaxIntensity", 0.0);
    if (!maximum)
      return std::unexpected(std::move(maximum.error()));
    auto period_variance = config.get_float("flickerPeriodVariance", 0.0);
    if (!period_variance)
      return std::unexpected(std::move(period_variance.error()));
    auto intensity_variance = config.get_float("flickerIntensityVariance", 0.0);
    if (!intensity_variance)
      return std::unexpected(std::move(intensity_variance.error()));
    auto flicker = PeriodicFunction<float>::create(root.presentationRandom(), *period, *minimum, *maximum,
        *period_variance, *intensity_variance);
    if (!flicker)
      return std::unexpected(ObjectException("Invalid object light flicker settings", std::move(flicker.error())));
    object.light_flickering = std::move(*flicker);
  }
  return {};
}

}// namespace

std::expected<std::shared_ptr<ObjectConfig>, StarException> ObjectDatabase::read_config(String const& path) const {
  auto assets_result = m_root.assets();
  if (!assets_result)
    return std::unexpected(ObjectException(Star::format("Error loading object '{}'", path), std::move(assets_result.error())));
  auto assets = std::move(*assets_result);
  auto result = [&, &root = m_root]() -> std::expected<std::shared_ptr<ObjectConfig>, StarException> {
    auto config_result = assets->json(path);
    if (!config_result)
      return std::unexpected(std::move(config_result.error()));
    auto config = std::move(*config_result);
    auto object_config = std::make_shared<ObjectConfig>();
    object_config->path = path;
    object_config->config = config;
    auto name_result = config.get_string("objectName");
    if (!name_result)
      return std::unexpected(std::move(name_result.error()));
    auto name = std::move(*name_result);
    object_config->name = std::move(name);
    auto type_result = config.get_string("objectType", "object");
    if (!type_result)
      return std::unexpected(std::move(type_result.error()));
    auto type = std::move(*type_result);
    object_config->type = std::move(type);
    auto race_result = config.get_string("race", "generic");
    if (!race_result)
      return std::unexpected(std::move(race_result.error()));
    auto race = std::move(*race_result);
    object_config->race = std::move(race);
    auto category_result = config.get_string("category", "other");
    if (!category_result)
      return std::unexpected(std::move(category_result.error()));
    auto category = std::move(*category_result);
    object_config->category = std::move(category);
    auto colony_tags = config.get("colonyTags", JsonArray()).and_then(json_to_string_list);
    if (!colony_tags)
      return std::unexpected(std::move(colony_tags.error()));
    object_config->colony_tags = std::move(*colony_tags);
    auto scripts = config.get("scripts", JsonArray()).and_then(json_to_string_list);
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    std::ranges::for_each(*scripts, [&](String& script) { script = AssetPath::relative_to(path, script); });
    object_config->scripts = std::move(*scripts);
    auto animation_scripts = config.get("animationScripts", JsonArray()).and_then(json_to_string_list);
    if (!animation_scripts)
      return std::unexpected(std::move(animation_scripts.error()));
    std::ranges::for_each(*animation_scripts, [&](String& script) { script = AssetPath::relative_to(path, script); });
    object_config->animation_scripts = std::move(*animation_scripts);
    auto price_result = config.get_int("price", 0);
    if (!price_result)
      return std::unexpected(std::move(price_result.error()));
    auto price = std::move(*price_result);
    object_config->price = std::move(price);
    if (object_config->price == 0)
      object_config->price = 1;
    auto has_object_item_result = config.get_bool("hasObjectItem", true);
    if (!has_object_item_result)
      return std::unexpected(std::move(has_object_item_result.error()));
    auto has_object_item = std::move(*has_object_item_result);
    object_config->has_object_item = std::move(has_object_item);
    auto scannable_result = config.get_bool("scannable", true);
    if (!scannable_result)
      return std::unexpected(std::move(scannable_result.error()));
    auto scannable = std::move(*scannable_result);
    object_config->scannable = std::move(scannable);
    if (object_config->has_object_item) {
    auto printable_result = config.get_bool("printable", object_config->scannable);
    if (!printable_result)
      return std::unexpected(std::move(printable_result.error()));
    auto printable = std::move(*printable_result);
    object_config->printable = std::move(printable);
    } else {
      object_config->printable = false;
    }
    auto retain_object_parameters_in_item_result = config.get_bool("retainObjectParametersInItem", false);
    if (!retain_object_parameters_in_item_result)
      return std::unexpected(std::move(retain_object_parameters_in_item_result.error()));
    auto retain_object_parameters_in_item = std::move(*retain_object_parameters_in_item_result);
    object_config->retain_object_parameters_in_item = std::move(retain_object_parameters_in_item);
    if (auto contains_result = config.contains("breakDropPool"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
    auto break_drop_pool_result = config.get_string("breakDropPool");
    if (!break_drop_pool_result)
      return std::unexpected(std::move(break_drop_pool_result.error()));
    auto break_drop_pool = std::move(*break_drop_pool_result);
    object_config->break_drop_pool = std::move(break_drop_pool);
    }
    auto break_options = config.find("breakDropOptions");
    if (!break_options)
      return std::unexpected(std::move(break_options.error()));
    if (*break_options) {
      auto option_groups = (*break_options)->get().iterate_array();
      if (!option_groups)
        return std::unexpected(std::move(option_groups.error()));
      for (auto const& choices : *option_groups) {
        auto choices_array = choices.iterate_array();
        if (!choices_array)
          return std::unexpected(std::move(choices_array.error()));
        List<ItemDescriptor> group;
        for (auto const& choice : *choices_array) {
          auto item_name = choice.get_string(0);
          if (!item_name)
            return std::unexpected(std::move(item_name.error()));
          auto item_count = choice.get_uint(1);
          if (!item_count)
            return std::unexpected(std::move(item_count.error()));
          auto item_parameters = choice.get_object(2);
          if (!item_parameters)
            return std::unexpected(std::move(item_parameters.error()));
          auto descriptor = ItemDescriptor::create(*item_name, static_cast<std::size_t>(*item_count), *item_parameters);
          if (!descriptor)
            return std::unexpected(std::move(descriptor.error()));
          group.push_back(std::move(*descriptor));
        }
        object_config->break_drop_options.push_back(std::move(group));
      }
      // Explicit empty break options mean always dropping nothing.
      if (object_config->break_drop_options.empty())
        object_config->break_drop_options.push_back({});
    }
    if (auto contains_result = config.contains("smashDropPool"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
    auto smash_drop_pool_result = config.get_string("smashDropPool");
    if (!smash_drop_pool_result)
      return std::unexpected(std::move(smash_drop_pool_result.error()));
    auto smash_drop_pool = std::move(*smash_drop_pool_result);
    object_config->smash_drop_pool = std::move(smash_drop_pool);
    }
    auto smash_options = config.find("smashDropOptions");
    if (!smash_options)
      return std::unexpected(std::move(smash_options.error()));
    if (*smash_options) {
      auto option_groups = (*smash_options)->get().iterate_array();
      if (!option_groups)
        return std::unexpected(std::move(option_groups.error()));
      for (auto const& choices : *option_groups) {
        auto choices_array = choices.iterate_array();
        if (!choices_array)
          return std::unexpected(std::move(choices_array.error()));
        List<ItemDescriptor> group;
        for (auto const& choice : *choices_array) {
          auto descriptor = ItemDescriptor::create(choice);
          if (!descriptor)
            return std::unexpected(std::move(descriptor.error()));
          group.push_back(std::move(*descriptor));
        }
        object_config->smash_drop_options.push_back(std::move(group));
      }
    }
    auto field12Result = config.get_array("smashSounds", JsonArray());
    if (!field12Result)
      return std::unexpected(std::move(field12Result.error()));
    auto field12 = std::move(*field12Result);
    for (auto const& sound : field12) {
    auto field13Result = sound.to_string();
    if (!field13Result)
      return std::unexpected(std::move(field13Result.error()));
    auto field13 = std::move(*field13Result);
    object_config->smash_sound_options.push_back(AssetPath::relative_to(path, field13));
    }
    if (auto contains_result = config.contains("smashParticles"); !contains_result)
      return std::unexpected(std::move(contains_result.error()));
    else if (*contains_result) {
    auto smash_particles_result = config.get_array("smashParticles");
    if (!smash_particles_result)
      return std::unexpected(std::move(smash_particles_result.error()));
    auto smash_particles = std::move(*smash_particles_result);
    object_config->smash_particles = std::move(smash_particles);
    }
    auto smashable_result = config.get_bool("smashable", false);
    if (!smashable_result)
      return std::unexpected(std::move(smashable_result.error()));
    auto smashable = std::move(*smashable_result);
    object_config->smashable = std::move(smashable);
    auto smash_on_break_result = config.get_bool("smashOnBreak", object_config->smashable);
    if (!smash_on_break_result)
      return std::unexpected(std::move(smash_on_break_result.error()));
    auto smash_on_break = std::move(*smash_on_break_result);
    object_config->smash_on_break = std::move(smash_on_break);
    auto unbreakable_result = config.get_bool("unbreakable", false);
    if (!unbreakable_result)
      return std::unexpected(std::move(unbreakable_result.error()));
    auto unbreakable = std::move(*unbreakable_result);
    object_config->unbreakable = std::move(unbreakable);
    if (object_config->unbreakable)
      object_config->smashable = false;
    auto damage_table_source = config.get("damageTable", "/objects/defaultParameters.config:damageTable");
    if (!damage_table_source)
      return std::unexpected(std::move(damage_table_source.error()));
    std::optional<String> damage_table_path;
    if (damage_table_source->is_type(Json::Type::String)) {
      auto source_path = damage_table_source->to_string();
      if (!source_path)
        return std::unexpected(std::move(source_path.error()));
      damage_table_path = std::move(*source_path);
    }
    auto damage_table = assets->fetch_json(*damage_table_source);
    if (!damage_table)
      return std::unexpected(assets->with_asset_context(std::move(damage_table.error()), damage_table_path ? *damage_table_path : path, "Loading object damage table"));
    auto damage_health = config.opt_float("health");
    if (!damage_health)
      return std::unexpected(std::move(damage_health.error()));
    auto harvest_level = config.opt_uint("harvestLevel");
    if (!harvest_level)
      return std::unexpected(std::move(harvest_level.error()));
    auto tile_damage = TileDamageParameters::create(*assets, *damage_table, *damage_health, *harvest_level);
    if (!tile_damage)
      return std::unexpected(assets->with_asset_context(std::move(tile_damage.error()), damage_table_path ? *damage_table_path : path, "Loading object damage table"));
    object_config->tile_damage_parameters = std::move(*tile_damage);
    auto damage_shake_magnitude_result = config.get_float("damageShakeMagnitude", 0.2f);
    if (!damage_shake_magnitude_result)
      return std::unexpected(std::move(damage_shake_magnitude_result.error()));
    auto damage_shake_magnitude = std::move(*damage_shake_magnitude_result);
    object_config->damage_shake_magnitude = std::move(damage_shake_magnitude);
    auto damage_material_kind_result = config.get_string("damageMaterialKind", "solid");
    if (!damage_material_kind_result)
      return std::unexpected(std::move(damage_material_kind_result.error()));
    auto damage_material_kind = std::move(*damage_material_kind_result);
    object_config->damage_material_kind = std::move(damage_material_kind);
    if (auto damage_team = read_object_damage_team(config, object_config->damage_team); !damage_team)
      return std::unexpected(std::move(damage_team.error()));
    if (auto lighting = read_object_lighting(root, config, *object_config); !lighting)
      return std::unexpected(std::move(lighting.error()));
    auto sound_effect_result = config.get_string("soundEffect", "");
    if (!sound_effect_result)
      return std::unexpected(std::move(sound_effect_result.error()));
    auto sound_effect = std::move(*sound_effect_result);
    object_config->sound_effect = std::move(sound_effect);
    auto sound_effect_range_multiplier_result = config.get_float("soundEffectRangeMultiplier", 1.0f);
    if (!sound_effect_range_multiplier_result)
      return std::unexpected(std::move(sound_effect_range_multiplier_result.error()));
    auto sound_effect_range_multiplier = std::move(*sound_effect_range_multiplier_result);
    object_config->sound_effect_range_multiplier = std::move(sound_effect_range_multiplier);
    auto field34Result = config.get_array("statusEffects", JsonArray());
    if (!field34Result)
      return std::unexpected(std::move(field34Result.error()));
    auto field34 = std::move(*field34Result);
    for (auto const& effect : field34) {
    auto field35Result = jsonToPersistentStatusEffect(effect);
    if (!field35Result)
      return std::unexpected(std::move(field35Result.error()));
    auto field35 = std::move(*field35Result);
    object_config->status_effects.push_back(std::move(field35));
    }
    auto touch_damage_config_result = parse_touch_damage(root, path, config);
    if (!touch_damage_config_result)
      return std::unexpected(std::move(touch_damage_config_result.error()));
    auto touch_damage_config = std::move(*touch_damage_config_result);
    object_config->touch_damage_config = std::move(touch_damage_config);
    auto minimum_liquid_level_result = config.opt_float("minimumLiquidLevel");
    if (!minimum_liquid_level_result)
      return std::unexpected(std::move(minimum_liquid_level_result.error()));
    auto minimum_liquid_level = std::move(*minimum_liquid_level_result);
    object_config->minimum_liquid_level = std::move(minimum_liquid_level);
    auto maximum_liquid_level_result = config.opt_float("maximumLiquidLevel");
    if (!maximum_liquid_level_result)
      return std::unexpected(std::move(maximum_liquid_level_result.error()));
    auto maximum_liquid_level = std::move(*maximum_liquid_level_result);
    object_config->maximum_liquid_level = std::move(maximum_liquid_level);
    auto liquid_check_interval_result = config.get_float("liquidCheckInterval", 0.5);
    if (!liquid_check_interval_result)
      return std::unexpected(std::move(liquid_check_interval_result.error()));
    auto liquid_check_interval = std::move(*liquid_check_interval_result);
    object_config->liquid_check_interval = std::move(liquid_check_interval);
    auto health_result = config.get_float("health", 1);
    if (!health_result)
      return std::unexpected(std::move(health_result.error()));
    auto health = std::move(*health_result);
    object_config->health = std::move(health);
    auto read_animation = [&](Json const& value) -> std::expected<Json, StarException> {
      if (!value.is_type(Json::Type::String))
        return value;
      auto reference = value.to_string();
      if (!reference)
        return std::unexpected(std::move(reference.error()));
      auto animation_path = AssetPath::relative_to(path, *reference);
      auto animation = assets->json(animation_path);
      if (!animation)
        return std::unexpected(assets->with_asset_context(std::move(animation.error()), animation_path, "Loading object animation"));
      return animation;
    };
    auto field36Result = config.get("animation", Json());
    if (!field36Result)
      return std::unexpected(std::move(field36Result.error()));
    auto field36 = std::move(*field36Result);
    if (field36) {
    auto animation_config_result = read_animation(field36);
    if (!animation_config_result)
      return std::unexpected(std::move(animation_config_result.error()));
    auto animation_config = std::move(*animation_config_result);
    object_config->animation_config = std::move(animation_config);
    auto field37Result = config.get("animationCustom", Json());
    if (!field37Result)
      return std::unexpected(std::move(field37Result.error()));
    auto field37 = std::move(*field37Result);
      if (field37) {
    auto field38Result = read_animation(field37);
    if (!field38Result)
      return std::unexpected(std::move(field38Result.error()));
    auto field38 = std::move(*field38Result);
    auto field39 = json_merge(object_config->animation_config, field38);
        object_config->animation_config = std::move(field39);
      }
    }
    auto field40Result = config.get("orientations");
    if (!field40Result)
      return std::unexpected(std::move(field40Result.error()));
    auto field40 = std::move(*field40Result);
    auto orientations_result = parse_orientations(root, path, field40, config);
    if (!orientations_result)
      return std::unexpected(std::move(orientations_result.error()));
    auto orientations = std::move(*orientations_result);
    object_config->orientations = std::move(orientations);
    // Keep compatibility with base-level particle emitter specifications.
    List<ObjectOrientation::ParticleEmissionEntry> particle_emitters;
    if (auto emitters = read_particle_emitters(root, path, config, particle_emitters); !emitters)
      return std::unexpected(std::move(emitters.error()));
    for (auto const& orientation : object_config->orientations)
      orientation->particle_emitters.append_range(particle_emitters);
    auto rooting_result = config.get_bool("rooting", false);
    if (!rooting_result)
      return std::unexpected(std::move(rooting_result.error()));
    auto rooting = std::move(*rooting_result);
    object_config->rooting = std::move(rooting);
    auto biome_placed_result = config.get_bool("biomePlaced", false);
    if (!biome_placed_result)
      return std::unexpected(std::move(biome_placed_result.error()));
    auto biome_placed = std::move(*biome_placed_result);
    object_config->biome_placed = std::move(biome_placed);
    return object_config;
  }();
  if (!result)
    return std::unexpected(ObjectException(Star::format("Error loading object '{}'", path), assets->with_asset_context(std::move(result.error()), path, "Loading object configuration")));
  return result;
}

std::expected<ObjectPlacementPreview, StarException> ObjectDatabase::cursor_hint_drawables(World const& world,
    String const& object_name, Vec2I const& position, Direction direction, Json const& parameters,
    bool test_placement) const {
  auto config_result = get_config(object_name);
  if (!config_result)
    return std::unexpected(std::move(config_result.error()));
  auto const& config = **config_result;
  auto metadata = m_root.image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));
  auto placement_image = json_merge_query("placementImage", config.config, parameters);
  if (!placement_image)
    return std::unexpected(std::move(placement_image.error()));

  ObjectPlacementPreview preview;
  std::optional<List<std::shared_ptr<ObjectOrientation>>> custom;
  std::size_t index = npos;
  if (test_placement || placement_image->is_null()) {
    auto parsed = parse_custom_orientations(m_root, config, parameters, false);
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    custom = std::move(*parsed);
    auto selected = ObjectConfig::find_valid_orientation(
        custom ? *custom : config.orientations, world, world.geometry().xwrap(position), direction);
    if (!selected)
      return std::unexpected(std::move(selected.error()));
    index = *selected;
    preview.placeable = index != npos;
  }
  if (!placement_image->is_null()) {
    auto image = placement_image->to_string();
    if (!image)
      return std::unexpected(std::move(image.error()));
    if (direction == Direction::Left)
      *image += "?flipx";
    auto offset = json_merge_query_def("placementImagePosition", JsonArray{0, 0}, config.config, parameters)
        .and_then(json_to_vec2f);
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    auto drawable = Drawable::make_image(**metadata, AssetPath::relative_to(config.path, *image),
        1.0f / tile_pixels, false, Vec2F(position) + *offset / tile_pixels);
    if (!drawable)
      return std::unexpected(std::move(drawable.error()));
    preview.drawables.push_back(std::move(*drawable));
    return preview;
  }

  auto const& orientations = custom ? *custom : config.orientations;
  if (index == npos) {
    // Database previews prefer the last affinity match; object previews
    // historically choose the first. Do not unify those fallback orders.
    index = 0;
    for (std::size_t i = 0; i < orientations.size(); ++i)
      if (orientations[i]->direction_affinity == direction)
        index = i;
  }
  auto entry = orientations.at(index);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  auto const& orientation = *entry->get();
  auto image_keys = orientation_image_keys(orientation.config, config.config, parameters);
  if (!image_keys)
    return std::unexpected(std::move(image_keys.error()));
  preview.drawables.reserve(orientation.image_layers.size());
  for (auto const& layer : orientation.image_layers) {
    Drawable drawable = layer;
    if (drawable.is_image()) {
      auto& image = drawable.image_part().image;
      image = AssetPath::join(image).replace_tags(*image_keys, true, "default");
    }
    if (orientation.flip_images) {
      auto bounds = drawable.bound_box(**metadata, false);
      if (!bounds)
        return std::unexpected(std::move(bounds.error()));
      drawable.scale(Vec2F(-1, 1), bounds->center() - drawable.position);
    }
    preview.drawables.push_back(std::move(drawable));
  }
  Drawable::translate_all(preview.drawables, Vec2F(position) + orientation.image_position);
  return preview;
}

}
