import std;
import star.asset_path;
import star.assets;
import star.color;
import star.data_stream;
import star.data_stream_extra;
import star.directives;
import star.format;
import star.image_metadata_database;
import star.json;
import star.json_extra;
import star.lexical_cast;
import star.list;
import star.math_common;
import star.parallax;
import star.plant_database;
import star.random;
import star.root;
import star.string;
import star.vector;

namespace Star {

namespace {

std::unexpected<StarException> invalid_field(std::string_view field, std::string_view reason) {
  return std::unexpected(StarException::format("Parallax field '{}': {}", field, reason));
}

std::expected<void, StarException> read_animation(Json const& settings, ParallaxLayer& layer, bool allow_static_zero) {
  auto count = settings.get_int("frameNumber", 1);
  if (!count) return std::unexpected(std::move(count.error()));
  if (*count < (allow_static_zero ? 0 : 1) || !std::in_range<int>(*count))
    return invalid_field("frameNumber", allow_static_zero ? "must be between 0 and INT_MAX" : "must be between 1 and INT_MAX");
  auto offset = settings.get_int("frameOffset", 0);
  if (!offset) return std::unexpected(std::move(offset.error()));
  if (!std::in_range<int>(*offset))
    return invalid_field("frameOffset", "must fit in a signed int");
  auto cycle = settings.get_float("animationCycle", 1.0f);
  if (!cycle) return std::unexpected(std::move(cycle.error()));
  if (!is_finite(*cycle))
    return invalid_field("animationCycle", "must be finite");
  if (*count > 1 && !(*cycle / static_cast<float>(*count) > 0.0f))
    return invalid_field("animationCycle", "must give each frame a positive duration");

  layer.frame_number = static_cast<unsigned>(*count);
  layer.frame_offset = static_cast<int>(*offset);
  layer.animation_cycle = *cycle;
  return {};
}

std::expected<void, StarException> validate_parallax_value(Vec2F const& parallax_value, std::string_view field) {
  auto finite = classify_finite(parallax_value[0], parallax_value[1]);
  if (!finite[0] || !finite[1] || parallax_value[0] == 0.0f || parallax_value[1] == 0.0f)
    return invalid_field(field, "both components must be finite and nonzero");
  return {};
}

std::expected<void, StarException> validate_layer_geometry(
    ParallaxLayer const& layer, std::string_view offset_field = "parallaxOffset") {
  auto finite = classify_finite(layer.vertical_origin, layer.z_level, layer.parallax_offset[0], layer.parallax_offset[1],
      layer.speed[0], layer.speed[1], layer.fade_percent);
  if (!finite[0]) return invalid_field("verticalOrigin", "must be finite");
  if (!finite[1]) return invalid_field("zLevel", "must be finite");
  if (!finite[2] || !finite[3]) return invalid_field(offset_field, "both components must be finite");
  if (!finite[4] || !finite[5]) return invalid_field("speed", "both components must be finite");
  if (!finite[6]) return invalid_field("fadePercent", "must be finite");
  if (layer.tile_limit_top && !is_finite(*layer.tile_limit_top))
    return invalid_field("tileLimitTop", "must be finite");
  if (layer.tile_limit_bottom && !is_finite(*layer.tile_limit_bottom))
    return invalid_field("tileLimitBottom", "must be finite");
  if (layer.wind_speed_multiplier && !is_finite(*layer.wind_speed_multiplier))
    return invalid_field("windSpeedMultiplier", "must be finite");
  return {};
}

std::expected<Vec2F, StarException> read_speed(Json const& setting) {
  if (!setting.is_type(Json::Type::Array)) {
    auto speed = setting.to_float();
    if (!speed) return std::unexpected(std::move(speed.error()));
    return Vec2F(*speed, 0.0f);
  }
  auto x = setting.get_float(0);
  if (!x) return std::unexpected(std::move(x.error()));
  auto y = setting.get_float(1);
  if (!y) return std::unexpected(std::move(y.error()));
  return Vec2F(*x, *y);
}

void sort_layers(ParallaxLayers& layers) {
  std::stable_sort(layers.begin(), layers.end(), [](ParallaxLayer const& a, ParallaxLayer const& b) {
    return a.z_level > b.z_level;
  });
}

template <typename Layer>
  requires std::same_as<std::remove_const_t<Layer>, ParallaxLayer>
auto legacy_wire_fields(Layer& layer) {
  return std::tie(layer.textures, layer.directives, layer.alpha, layer.parallax_value,
      layer.repeat, layer.tile_limit_top, layer.tile_limit_bottom, layer.vertical_origin, layer.z_level,
      layer.parallax_offset, layer.time_of_day_correlation, layer.speed, layer.follows_wind,
      layer.wind_speed_multiplier, layer.unlit, layer.light_mapped, layer.fade_percent);
}

}

std::expected<ParallaxLayer, StarException> ParallaxLayer::create(Json const& store) {
  ParallaxLayer result;
  auto textures = store.get("textures");
  if (!textures) return std::unexpected(std::move(textures.error()));
  auto texture_list = json_to_string_list(*textures);
  if (!texture_list) return std::unexpected(std::move(texture_list.error()));
  result.textures = std::move(*texture_list);
  auto directives = store.get_string("directives");
  if (!directives) return std::unexpected(std::move(directives.error()));
  result.directives = std::move(*directives);

  // Existing saves with zero frames were rendered as static layers.
  if (auto animation = read_animation(store, result, true); !animation)
    return std::unexpected(std::move(animation.error()));
  auto tile_limit_top = store.opt_float("tileLimitTop");
  if (!tile_limit_top) return std::unexpected(std::move(tile_limit_top.error()));
  result.tile_limit_top = *tile_limit_top;
  auto tile_limit_bottom = store.opt_float("tileLimitBottom");
  if (!tile_limit_bottom) return std::unexpected(std::move(tile_limit_bottom.error()));
  result.tile_limit_bottom = *tile_limit_bottom;
  auto vertical_origin = store.get_float("verticalOrigin");
  if (!vertical_origin) return std::unexpected(std::move(vertical_origin.error()));
  result.vertical_origin = *vertical_origin;
  auto z_level = store.get_float("zLevel");
  if (!z_level) return std::unexpected(std::move(z_level.error()));
  result.z_level = *z_level;
  auto correlation = store.get_string("timeOfDayCorrelation");
  if (!correlation) return std::unexpected(std::move(correlation.error()));
  result.time_of_day_correlation = std::move(*correlation);
  auto follows_wind = store.get_bool("followsWind", false);
  if (!follows_wind) return std::unexpected(std::move(follows_wind.error()));
  result.follows_wind = *follows_wind;
  auto wind_speed_multiplier = store.opt_float("windSpeedMultiplier");
  if (!wind_speed_multiplier) return std::unexpected(std::move(wind_speed_multiplier.error()));
  result.wind_speed_multiplier = *wind_speed_multiplier;
  auto unlit = store.get_bool("unlit");
  if (!unlit) return std::unexpected(std::move(unlit.error()));
  result.unlit = *unlit;
  auto light_mapped = store.get_bool("lightMapped");
  if (!light_mapped) return std::unexpected(std::move(light_mapped.error()));
  result.light_mapped = *light_mapped;
  auto fade_percent = store.get_float("fadePercent");
  if (!fade_percent) return std::unexpected(std::move(fade_percent.error()));
  result.fade_percent = *fade_percent;
  auto parallax_json = store.get("parallaxValue");
  if (!parallax_json) return std::unexpected(std::move(parallax_json.error()));
  auto parallax_value = json_to_vec2f(*parallax_json);
  if (!parallax_value) return std::unexpected(std::move(parallax_value.error()));
  if (auto valid = validate_parallax_value(*parallax_value, "parallaxValue"); !valid)
    return std::unexpected(std::move(valid.error()));
  result.parallax_value = *parallax_value;
  auto repeat_json = store.get("repeat");
  if (!repeat_json) return std::unexpected(std::move(repeat_json.error()));
  auto repeat = json_to_vec2b(*repeat_json);
  if (!repeat) return std::unexpected(std::move(repeat.error()));
  result.repeat = *repeat;
  auto offset_json = store.get("parallaxOffset");
  if (!offset_json) return std::unexpected(std::move(offset_json.error()));
  auto offset = json_to_vec2f(*offset_json);
  if (!offset) return std::unexpected(std::move(offset.error()));
  result.parallax_offset = *offset;
  auto speed_x = store.get_float("speed");
  if (!speed_x) return std::unexpected(std::move(speed_x.error()));
  auto speed_y = store.get_float("speedY", 0.0f);
  if (!speed_y) return std::unexpected(std::move(speed_y.error()));
  result.speed = {*speed_x, *speed_y};
  if (auto valid = validate_layer_geometry(result); !valid)
    return std::unexpected(std::move(valid.error()));
  return result;
}

Json ParallaxLayer::store() const {
  return JsonObject{
      {"textures", json_from_string_list(textures)},
      {"directives", directives.string()},
      {"frameNumber", frame_number},
      {"frameOffset", frame_offset},
      {"animationCycle", animation_cycle},
      {"parallaxValue", json_from_vec2f(parallax_value)},
      {"repeat", json_from_vec2b(repeat)},
      {"tileLimitTop", json_from_optional(tile_limit_top)},
      {"tileLimitBottom", json_from_optional(tile_limit_bottom)},
      {"verticalOrigin", vertical_origin},
      {"zLevel", z_level},
      {"parallaxOffset", json_from_vec2f(parallax_offset)},
      {"timeOfDayCorrelation", time_of_day_correlation},
      {"speed", speed[0]},
      {"speedY", speed[1]},
      {"followsWind", follows_wind},
      {"windSpeedMultiplier", json_from_optional(wind_speed_multiplier)},
      {"unlit", unlit},
      {"lightMapped", light_mapped},
      {"fadePercent", fade_percent}};
}

namespace {
void append_directive(Directives& directives, String operation) {
  auto prefix = directives.maybe_string();
  if (!prefix) {
    directives = std::move(operation);
    return;
  }
  auto const& text = prefix->get();
  String combined;
  combined.reserve(text.utf8_size() + operation.utf8_size() + 1);
  combined += text;
  combined += "?";
  combined += operation;
  directives = std::move(combined);
}
}

std::expected<void, StarException> ParallaxLayer::fade_to_sky_color(Color const& sky_color) {
  if (fade_percent > 0) {
    auto hex = sky_color.to_hex().slice(0, 6);
    if (!hex) return std::unexpected(std::move(hex.error()));
    append_directive(directives, "fade=" + *hex + "=" + Star::to_string(fade_percent));
  }
  return {};
}

unsigned ParallaxLayer::animation_frame(double epoch_time) const {
  if (frame_number <= 1)
    return 0;
  auto finite = classify_finite(epoch_time, animation_cycle);
  if (!finite[0] || !finite[1])
    return 0;
  auto duration = animation_cycle / static_cast<float>(frame_number);
  if (!(duration > 0.0f))
    return 0;

  // Keep the original float frame boundaries and clamp negative elapsed time.
  auto elapsed = static_cast<float>(std::fmod(epoch_time, static_cast<double>(animation_cycle)));
  auto frame = static_cast<double>(elapsed / duration);
  auto count = static_cast<std::int64_t>(frame_number);
  auto index = static_cast<std::int64_t>(std::clamp(frame, 0.0, static_cast<double>(count - 1)));
  auto shifted = (index + frame_offset) % count;
  return static_cast<unsigned>(shifted < 0 ? shifted + count : shifted);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, ParallaxLayer& layer) {
  // The old wire has alpha but no animation. Replace the whole layer only on success.
  ParallaxLayer decoded;
  auto fields = legacy_wire_fields(decoded);
  auto read = stream >> fields;
  if (!read) return read;
  layer = std::move(decoded);
  return std::ref(stream);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, ParallaxLayer const& layer) {
  return stream << legacy_wire_fields(layer);
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::create(Root& root, String const& asset_file,
    std::uint64_t seed, float vertical_origin, float hue_shift, std::optional<TreeVariant> parallax_tree_variant) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(asset_file);
  if (!config) return std::unexpected(std::move(config.error()));
  auto asset_error = [&](StarException error) {
    return std::unexpected(std::move(error).with_context(Star::format("Parallax asset '{}'", asset_file)));
  };

  auto origin = config->get_float("verticalOrigin", 0.0f);
  if (!origin) return asset_error(std::move(origin.error()));
  vertical_origin += *origin;
  auto finite = classify_finite(vertical_origin, hue_shift);
  if (!finite[0]) return asset_error(invalid_field("verticalOrigin", "combined origin must be finite").error());
  if (!finite[1]) return asset_error(invalid_field("hueShift", "must be finite").error());
  auto layers_json = config->get("layers");
  if (!layers_json) return asset_error(std::move(layers_json.error()));
  auto layers = layers_json->array_ptr();
  if (!layers) return asset_error(std::move(layers.error()));

  auto result = std::make_shared<Parallax>(ConstructionKey{}, root);
  result->m_seed = seed;
  result->m_vertical_origin = vertical_origin;
  result->m_parallax_tree_variant = std::move(parallax_tree_variant);
  result->m_hue_shift = hue_shift;
  result->m_image_directory = "/parallax/images/";

  RandomSource random(seed);
  std::shared_ptr<ImageMetadataDatabase const> image_metadata;
  for (auto const& settings : **layers) {
    auto kind = settings.get_string("kind");
    if (!kind) return asset_error(std::move(kind.error()));
    auto layer_error = [&](StarException error) {
      return asset_error(std::move(error).with_context(Star::format("Parallax layer '{}'", *kind)));
    };
    auto frequency = settings.get_float("frequency", 1.0f);
    if (!frequency) return layer_error(std::move(frequency.error()));
    if (!is_finite(*frequency)) return layer_error(invalid_field("frequency", "must be finite").error());
    if (random.uniform_float() > *frequency)
      continue;
    if (auto built = result->build_layer(settings, *kind, image_metadata); !built)
      return layer_error(std::move(built.error()));
  }
  sort_layers(result->m_layers);
  return result;
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::create(Root& root, Json const& store) {
  auto seed = store.get_uint("seed");
  if (!seed) return std::unexpected(std::move(seed.error()));
  auto vertical_origin = store.get_float("verticalOrigin");
  if (!vertical_origin) return std::unexpected(std::move(vertical_origin.error()));
  auto hue_shift = store.get_float("hueShift");
  if (!hue_shift) return std::unexpected(std::move(hue_shift.error()));
  auto finite = classify_finite(*vertical_origin, *hue_shift);
  if (!finite[0]) return invalid_field("verticalOrigin", "must be finite");
  if (!finite[1]) return invalid_field("hueShift", "must be finite");
  auto image_directory = store.get_string("imageDirectory");
  if (!image_directory) return std::unexpected(std::move(image_directory.error()));

  auto result = std::make_shared<Parallax>(ConstructionKey{}, root);
  result->m_seed = *seed;
  result->m_vertical_origin = *vertical_origin;
  result->m_hue_shift = *hue_shift;
  result->m_image_directory = std::move(*image_directory);
  auto tree = store.opt("parallaxTreeVariant");
  if (!tree) return std::unexpected(std::move(tree.error()));
  if (*tree) {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto variant = TreeVariant::create(**assets, **tree);
    if (!variant) return std::unexpected(std::move(variant.error()));
    result->m_parallax_tree_variant = std::move(*variant);
  }
  auto layers_json = store.get("layers");
  if (!layers_json) return std::unexpected(std::move(layers_json.error()));
  auto layers = layers_json->array_ptr();
  if (!layers) return std::unexpected(std::move(layers.error()));
  result->m_layers.reserve((*layers)->size());
  for (auto const& layer : **layers) {
    auto loaded = ParallaxLayer::create(layer);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    result->m_layers.push_back(std::move(*loaded));
  }
  sort_layers(result->m_layers);
  return result;
}

std::expected<Json, StarException> Parallax::store() const {
  Json tree;
  if (m_parallax_tree_variant) {
    auto stored = m_parallax_tree_variant->toJson();
    if (!stored) return std::unexpected(std::move(stored.error()));
    tree = std::move(*stored);
  }
  return JsonObject{{"seed", m_seed},
      {"verticalOrigin", m_vertical_origin},
      {"parallaxTreeVariant", std::move(tree)},
      {"hueShift", m_hue_shift},
      {"imageDirectory", m_image_directory},
      {"layers", m_layers.transformed(std::mem_fn(&ParallaxLayer::store))}};
}

std::expected<void, StarException> Parallax::fade_to_sky_color(Color const& sky_color) {
  for (auto& layer : m_layers) {
    if (auto faded = layer.fade_to_sky_color(sky_color); !faded) return std::unexpected(std::move(faded.error()));
  }
  return {};
}

std::expected<std::shared_ptr<Parallax>, StarException> Parallax::create_overlay(String const& asset_file) const {
  return create(m_root, asset_file, m_seed, m_vertical_origin, m_hue_shift, m_parallax_tree_variant);
}

ParallaxLayers const& Parallax::layers() const {
  return m_layers;
}

std::expected<void, StarException> Parallax::build_layer(
    Json const& settings, String const& kind, std::shared_ptr<ImageMetadataDatabase const>& image_metadata) {
  auto is_foliage = kind.starts_with("foliage/");
  if (!is_foliage) return std::unexpected(std::move(is_foliage.error()));
  auto is_stem = kind.starts_with("stem/");
  if (!is_stem) return std::unexpected(std::move(is_stem.error()));

  String texture_path;
  if (*is_foliage) {
    if (!m_parallax_tree_variant)
      return {};
    auto enabled = m_parallax_tree_variant->foliageSettings.get_bool("parallaxFoliage", false);
    if (!enabled) return std::unexpected(std::move(enabled.error()));
    if (!*enabled)
      return {};
    auto subdirectory = kind.replace("foliage/", "");
    if (!subdirectory) return std::unexpected(std::move(subdirectory.error()));
    texture_path = m_parallax_tree_variant->foliageDirectory + "parallax/" + *subdirectory + "/";
  } else if (*is_stem) {
    if (!m_parallax_tree_variant)
      return {};
    auto subdirectory = kind.replace("stem/", "");
    if (!subdirectory) return std::unexpected(std::move(subdirectory.error()));
    texture_path = m_parallax_tree_variant->stemDirectory + "parallax/" + *subdirectory + "/";
  } else {
    texture_path = m_image_directory + kind + "/";
  }

  ParallaxLayer layer;
  if (auto animation = read_animation(settings, layer, false); !animation)
    return std::unexpected(std::move(animation.error()));

  auto base_count = settings.get_int("baseCount", 1);
  if (!base_count) return std::unexpected(std::move(base_count.error()));
  if (*base_count < 1 || !std::in_range<int>(*base_count))
    return invalid_field("baseCount", "must be between 1 and INT_MAX");
  auto mod_count = settings.get_int("modCount", 0);
  if (!mod_count) return std::unexpected(std::move(mod_count.error()));
  if (*mod_count < 0 || !std::in_range<int>(*mod_count))
    return invalid_field("modCount", "must be between 0 and INT_MAX");

  // Skipped configurations do not advance the layer seed. Draw even for
  // singleton ranges, so subsequent offsets and speeds retain their sequence.
  RandomSource random(m_seed + m_layers.size());
  auto base = random.uniform_uint(static_cast<std::uint64_t>(*base_count - 1)) + 1;
  auto mod = random.uniform_uint(static_cast<std::uint64_t>(*mod_count));
  layer.textures.reserve(mod != 0 ? 2 : 1);
  layer.textures.push_back(AssetPath::relative_to(texture_path, "base/" + Star::to_string(base) + ".png"));
  if (mod != 0)
    layer.textures.push_back(AssetPath::relative_to(texture_path, "mod/" + Star::to_string(mod) + ".png"));

  auto parallax = settings.get("parallax");
  if (!parallax) return std::unexpected(std::move(parallax.error()));
  if (parallax->is_type(Json::Type::Array)) {
    auto value = json_to_vec2f(*parallax);
    if (!value) return std::unexpected(std::move(value.error()));
    layer.parallax_value = *value;
  } else {
    auto value = parallax->to_float();
    if (!value) return std::unexpected(std::move(value.error()));
    layer.parallax_value = Vec2F::filled(*value);
  }
  if (auto valid = validate_parallax_value(layer.parallax_value, "parallax"); !valid)
    return std::unexpected(std::move(valid.error()));

  auto repeat_y = settings.get_bool("repeatY", false);
  if (!repeat_y) return std::unexpected(std::move(repeat_y.error()));
  layer.repeat = {true, *repeat_y};
  if (*repeat_y) {
    auto top = settings.opt_float("tileLimitTop");
    if (!top) return std::unexpected(std::move(top.error()));
    layer.tile_limit_top = *top;
    auto bottom = settings.opt_float("tileLimitBottom");
    if (!bottom) return std::unexpected(std::move(bottom.error()));
    layer.tile_limit_bottom = *bottom;
  }

  layer.vertical_origin = m_vertical_origin;
  layer.z_level = layer.parallax_value.sum();
  auto offset = settings.find("offset");
  if (!offset) return std::unexpected(std::move(offset.error()));
  if (*offset && !(*offset)->get().is_null()) {
    auto const& components = (*offset)->get();
    auto x = components.get_float(0);
    if (!x) return std::unexpected(std::move(x.error()));
    auto y = components.get_float(1);
    if (!y) return std::unexpected(std::move(y.error()));
    layer.parallax_offset = {*x, *y};
  }
  auto no_random_offset = settings.get_bool("noRandomOffset", false);
  if (!no_random_offset) return std::unexpected(std::move(no_random_offset.error()));
  if (!*no_random_offset) {
    if (!image_metadata) {
      auto metadata = m_root.image_metadata_database();
      if (!metadata) return std::unexpected(std::move(metadata.error()));
      image_metadata = std::move(*metadata);
    }
    auto size = image_metadata->image_size(layer.textures[0]);
    if (!size) return std::unexpected(std::move(size.error()));
    layer.parallax_offset[0] += random.uniform_uint((*size)[0]);
  }
  auto correlation = settings.get_string("timeOfDayCorrelation", "");
  if (!correlation) return std::unexpected(std::move(correlation.error()));
  layer.time_of_day_correlation = std::move(*correlation);

  auto min_speed_json = settings.get("minSpeed", 0.0f);
  if (!min_speed_json) return std::unexpected(std::move(min_speed_json.error()));
  auto min_speed = read_speed(*min_speed_json);
  if (!min_speed) return std::unexpected(std::move(min_speed.error()));
  auto max_speed_json = settings.get("maxSpeed", 0.0f);
  if (!max_speed_json) return std::unexpected(std::move(max_speed_json.error()));
  auto max_speed = read_speed(*max_speed_json);
  if (!max_speed) return std::unexpected(std::move(max_speed.error()));
  auto speed_x = random.uniform_float((*min_speed)[0], (*max_speed)[0]);
  if (!speed_x) return std::unexpected(std::move(speed_x.error()));
  auto speed_y = random.uniform_float((*min_speed)[1], (*max_speed)[1]);
  if (!speed_y) return std::unexpected(std::move(speed_y.error()));
  layer.speed = {*speed_x, *speed_y};

  auto follows_wind = settings.get_bool("followsWind", false);
  if (!follows_wind) return std::unexpected(std::move(follows_wind.error()));
  layer.follows_wind = *follows_wind;
  auto wind_speed_multiplier = settings.opt_float("windSpeedMultiplier");
  if (!wind_speed_multiplier) return std::unexpected(std::move(wind_speed_multiplier.error()));
  layer.wind_speed_multiplier = *wind_speed_multiplier;
  auto unlit = settings.get_bool("unlit", false);
  if (!unlit) return std::unexpected(std::move(unlit.error()));
  layer.unlit = *unlit;
  auto light_mapped = settings.get_bool("lightMapped", false);
  if (!light_mapped) return std::unexpected(std::move(light_mapped.error()));
  layer.light_mapped = *light_mapped;
  auto fade_percent = settings.get_float("fadePercent", 0.0f);
  if (!fade_percent) return std::unexpected(std::move(fade_percent.error()));
  layer.fade_percent = *fade_percent;
  if (auto valid = validate_layer_geometry(layer, "offset"); !valid)
    return std::unexpected(std::move(valid.error()));

  auto directives = settings.get_string("directives", "");
  if (!directives) return std::unexpected(std::move(directives.error()));
  layer.directives = std::move(*directives);
  if (*is_foliage) {
    append_directive(layer.directives, String(Star::format("hueshift={}", m_parallax_tree_variant->foliageHueShift)));
  } else if (*is_stem) {
    append_directive(layer.directives, String(Star::format("hueshift={}", m_parallax_tree_variant->stemHueShift)));
  } else {
    auto no_hue_shift = settings.get_bool("nohueshift", false);
    if (!no_hue_shift) return std::unexpected(std::move(no_hue_shift.error()));
    if (!*no_hue_shift)
      append_directive(layer.directives, String(Star::format("hueshift={}", m_hue_shift)));
  }

  m_layers.push_back(std::move(layer));
  return {};
}

}
