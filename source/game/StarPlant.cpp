import star.json_extra;
import star.json;
import star.data_stream_extra;
import star.id_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.image;
import star.audio;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.inspectable_entity;
import star.plant;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;
import star.matrix3;

import star.light_source;
import star.entity_rendering;

import star.particle_database;
import star.object_database;
import star.image_metadata_database;
import star.plant_database;


import star.plant_drop;

import star.data_stream_devices;

import star.net_compatibility;
import star.random;
import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

std::expected<EnumMap<Plant::RotationType>, StarException> const Plant::rotation_type_names = EnumMap<Plant::RotationType>::create({
  {Plant::RotationType::DontRotate, "dontRotate"},
  {Plant::RotationType::RotateBranch, "rotateBranch"},
  {Plant::RotationType::RotateLeaves, "rotateLeaves"},
  {Plant::RotationType::RotateCrownBranch, "rotateCrownBranch"},
  {Plant::RotationType::RotateCrownLeaves, "rotateCrownLeaves"},
});

namespace {

// The object owner stays alive in the caller. Keep the iteration order used
// by JsonObject::keys(), without copying the keys or looking each one up again.
using TreeCandidate = std::pair<String const&, Json const&>;
using TreeCandidatePool = std::vector<TreeCandidate>;

TreeCandidatePool tree_candidate_pool(JsonObject const& object) {
  TreeCandidatePool pool;
  pool.reserve(object.size());
  for (auto [key, part] : object)
    pool.emplace_back(key, part);
  return pool;
}

// Base and crown only draw once, so they don't need an index allocation.
TreeCandidate pick_tree_candidate(JsonObject const& nonempty, RandomSource& random) {
  auto candidate = nonempty.begin();
  for (auto index = random.uniform_uint(nonempty.size() - 1); index != 0; --index)
    ++candidate;
  return *candidate;
}

std::expected<std::shared_ptr<JsonObject const>, StarException> optional_tree_object(Json const& settings, String const& key) {
  auto value = settings.find(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!*value || (*value)->get().is_null())
    return nullptr;
  return (*value)->get().object_ptr();
}

std::expected<double, StarException> tree_attachment_number(Json const& attachment, String const& key) {
  auto value = attachment.get_double(key);
  if (!value)
    return std::unexpected(std::move(value.error()));
  if (!is_finite(*value))
    return std::unexpected(PlantException::format("Tree attachment '{}' must be finite", key));
  return *value;
}

std::expected<Vec2F, StarException> tree_attachment_offset(
    Json const& attachment, Vec2F origin, String const& x_key, String const& y_key) {
  auto x = tree_attachment_number(attachment, x_key);
  if (!x)
    return std::unexpected(std::move(x.error()));
  auto y = tree_attachment_number(attachment, y_key);
  if (!y)
    return std::unexpected(std::move(y.error()));
  // Retain the original double addition followed by float rounding.
  double next_x = origin.x() + *x / tile_pixels;
  double next_y = origin.y() + *y / tile_pixels;
  constexpr double limit = std::numeric_limits<float>::max();
  if (next_x < -limit || next_x > limit || next_y < -limit || next_y > limit)
    return std::unexpected(PlantException::format("Tree attachment offset exceeds the float range"));
  return Vec2F(static_cast<float>(next_x), static_cast<float>(next_y));
}

}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create_uninitialized(Root& root) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(root);
  auto damage_status = EntityTileDamageStatus::create();
  if (!damage_status)
    return std::unexpected(std::move(damage_status.error()));
  entity->m_tile_damage_status = std::move(*damage_status);
  return entity;
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& root, TreeVariant const& config, std::uint64_t seed) {
  auto entity = create_uninitialized(root);
  if (!entity)
    return entity;
  if (auto result = (*entity)->initialize(config, seed); !result)
    return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(TreeVariant const& config, std::uint64_t seed) {
  if (!is_finite(config.stem_hue_shift) || !is_finite(config.foliage_hue_shift))
    return std::unexpected(PlantException::format("Tree hue shifts must be finite"));

  m_ceiling = config.ceiling;
  m_falls_when_dead = true;
  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tile_damage_parameters = config.tile_damage_parameters;

  auto stem_drops = (config.stem_drop_config.is_null() ? Json(JsonObject{}) : config.stem_drop_config)
    .set("hueshift", config.stem_hue_shift);
  if (!stem_drops)
    return std::unexpected(std::move(stem_drops.error()));
  m_stem_drop_config = std::move(*stem_drops);
  auto foliage_drops = (config.foliage_drop_config.is_null() ? Json(JsonObject{}) : config.foliage_drop_config)
    .set("hueshift", config.foliage_hue_shift);
  if (!foliage_drops)
    return std::unexpected(std::move(foliage_drops.error()));
  m_foliage_drop_config = std::move(*foliage_drops);
  m_sapling_drop_config = JsonObject{
    {"stemName", config.stem_name},
    {"stemHueShift", config.stem_hue_shift},
    {"foliageName", config.foliage_name},
    {"foliageHueShift", config.foliage_hue_shift},
  };

  RandomSource random(seed);
  auto& wind_random = root().random();
  float rotation_base = wind_random.uniform_float() * 0.5f;
  auto next_rotation = [&] { return wind_random.uniform_float() + rotation_base; };

  Vec2F offset;
  int segment = 0;
  auto add_piece = [&](String const& file, float hue, Vec2F position, bool structural,
                       PlantPieceKind kind, float z_level, RotationType rotation, float phase) {
    auto& piece = m_pieces.emplace_back();
    piece.image = Star::format("{}?hueshift={}", file, hue);
    piece.offset = position;
    piece.segment_idx = segment;
    piece.structural_segment = structural;
    piece.kind = kind;
    piece.z_level = z_level;
    piece.rotation_type = rotation;
    piece.rotation_offset = phase;
  };

  using LeafPool = std::optional<std::shared_ptr<JsonObject const>>;
  auto add_leaves = [&](LeafPool& pool, String const& pool_key, String const& candidate_key,
                        Vec2F position, RotationType rotation, std::optional<float> shared_phase = {})
      -> std::expected<void, StarException> {
    if (!pool) {
      auto object = optional_tree_object(config.foliage_settings, pool_key);
      if (!object)
        return std::unexpected(std::move(object.error()));
      pool.emplace(std::move(*object));
    }
    if (!*pool)
      return {};
    auto const& object = **pool;
    auto leaf = object.find(candidate_key);
    if (leaf == object.end())
      return {};

    auto attachment = leaf->second.get("attachment");
    if (!attachment)
      return std::unexpected(std::move(attachment.error()));
    auto leaf_position = tree_attachment_offset(*attachment, position, "bx", "by");
    if (!leaf_position)
      return std::unexpected(std::move(leaf_position.error()));

    // Front before back. Base/trunk leaves draw independently; branch/crown
    // leaves share their stem's phase and must not advance the Root RNG.
    for (auto const& [image_key, z_level] : {
      std::pair{std::string_view("image"), 3.0f}, std::pair{std::string_view("backimage"), -1.0f}}) {
      auto image = leaf->second.find(String(image_key));
      if (!image)
        return std::unexpected(std::move(image.error()));
      if (!*image)
        continue;
      auto name = (*image)->get().string_ptr();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if ((*name)->empty())
        continue;
      add_piece(AssetPath::relative_to(config.foliage_directory, **name), config.foliage_hue_shift,
        *leaf_position, false, Foliage, z_level, m_ceiling ? DontRotate : rotation,
        shared_phase ? *shared_phase : next_rotation());
    }
    return {};
  };

  auto stem_file = [&](Json const& part) -> std::expected<String, StarException> {
    auto name = part.get_string("image");
    if (!name)
      return std::unexpected(std::move(name.error()));
    return AssetPath::relative_to(config.stem_directory, *name);
  };
  auto apply_attachment = [&](Json const& attachment, String const& x, String const& y) -> std::expected<void, StarException> {
    auto next_offset = tree_attachment_offset(attachment, offset, x, y);
    if (!next_offset)
      return std::unexpected(std::move(next_offset.error()));
    offset = *next_offset;
    return {};
  };

  {
    auto base_settings = config.stem_settings.get("base");
    if (!base_settings)
      return std::unexpected(std::move(base_settings.error()));
    auto bases = base_settings->object_ptr();
    if (!bases)
      return std::unexpected(std::move(bases.error()));
    if ((*bases)->empty())
      return std::unexpected(PlantException("Tree base candidate pool is empty"));
    auto [key, part] = pick_tree_candidate(**bases, random);
    auto attachment = part.get("attachment");
    if (!attachment)
      return std::unexpected(std::move(attachment.error()));
    if (auto result = apply_attachment(*attachment, "bx", "by"); !result)
      return std::unexpected(std::move(result.error()));
    auto file = stem_file(part);
    if (!file)
      return std::unexpected(std::move(file.error()));
    if (m_ceiling) {
      auto assets = root().assets();
      if (!assets)
        return std::unexpected(std::move(assets.error()));
      auto image = (*assets)->image(*file);
      if (!image)
        return std::unexpected(std::move(image.error()));
      offset[1] = 1.0f - static_cast<float>((*image)->height()) / tile_pixels;
    }
    add_piece(*file, config.stem_hue_shift, offset, true, Stem, 0.0f, DontRotate, next_rotation());
    LeafPool leaves;
    if (auto result = add_leaves(leaves, "baseLeaves", key, offset, RotateLeaves); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = apply_attachment(*attachment, "x", "y"); !result)
      return std::unexpected(std::move(result.error()));
    ++segment;
  }

  {
    auto middle_settings = config.stem_settings.get("middle");
    if (!middle_settings)
      return std::unexpected(std::move(middle_settings.error()));
    auto middles = middle_settings->object_ptr();
    if (!middles)
      return std::unexpected(std::move(middles.error()));
    auto min_height = config.stem_settings.get_int("middleMinSize", 1);
    if (!min_height)
      return std::unexpected(std::move(min_height.error()));
    auto max_height = config.stem_settings.get_int("middleMaxSize", 6);
    if (!max_height)
      return std::unexpected(std::move(max_height.error()));
    if (*min_height < 0 || *min_height > *max_height || *max_height >= std::numeric_limits<int>::max())
      return std::unexpected(PlantException::format("Invalid tree middle size range [{}, {}]", *min_height, *max_height));
    int middle_height = static_cast<int>(*min_height + random.uniform_uint(*max_height - *min_height));
    auto middle_pool = middle_height != 0 ? tree_candidate_pool(**middles) : TreeCandidatePool{};
    if (middle_height != 0 && middle_pool.empty())
      return std::unexpected(PlantException::format("Tree middle candidate pool is empty"));

    auto branch_settings = config.stem_settings.find("branch");
    if (!branch_settings)
      return std::unexpected(std::move(branch_settings.error()));
    std::shared_ptr<JsonObject const> branches;
    if (*branch_settings) {
      auto object = (*branch_settings)->get().object_ptr();
      if (!object)
        return std::unexpected(std::move(object.error()));
      branches = std::move(*object);
    }

    // Build the branch index only when the first branch is attempted.
    TreeCandidatePool branch_pool;
    float branch_y = offset.y();
    std::optional<bool> always_branch;
    LeafPool trunk_leaves, branch_leaves;
    for (int i = 0; i < middle_height; ++i, ++segment) {
      auto const& [key, part] = middle_pool[random.uniform_uint(middle_pool.size() - 1)];
      auto attachment = part.get("attachment");
      if (!attachment)
        return std::unexpected(std::move(attachment.error()));
      if (auto result = apply_attachment(*attachment, "bx", "by"); !result)
        return std::unexpected(std::move(result.error()));
      auto file = stem_file(part);
      if (!file)
        return std::unexpected(std::move(file.error()));
      add_piece(*file, config.stem_hue_shift, offset, true, Stem, 1.0f, DontRotate, next_rotation());
      if (auto result = add_leaves(trunk_leaves, "trunkLeaves", key, offset, RotateLeaves); !result)
        return std::unexpected(std::move(result.error()));
      if (auto result = apply_attachment(*attachment, "x", "y"); !result)
        return std::unexpected(std::move(result.error()));

      while (branches && !branches->empty() && offset.y() >= branch_y) {
        if (branch_pool.empty())
          branch_pool = tree_candidate_pool(*branches);
        auto const& [branch_key, branch] = branch_pool[random.uniform_uint(branch_pool.size() - 1)];
        auto branch_attachment = branch.get("attachment");
        if (!branch_attachment)
          return std::unexpected(std::move(branch_attachment.error()));
        auto height = tree_attachment_number(*branch_attachment, "h");
        if (!height)
          return std::unexpected(std::move(height.error()));
        double step = *height / tile_pixels;
        if (step <= 0.0 || step > std::numeric_limits<float>::max())
          return std::unexpected(PlantException::format("Tree branch height must be positive and fit in a float"));
        float h = static_cast<float>(step);
        if (h == 0.0f)
          return std::unexpected(PlantException::format("Tree branch height rounds to zero"));
        if (offset.y() < branch_y + h / 2.0f)
          break;

        auto branch_position = tree_attachment_offset(*branch_attachment, Vec2F(offset.x(), branch_y), "bx", "by");
        if (!branch_position)
          return std::unexpected(std::move(branch_position.error()));
        if (!always_branch) {
          auto value = config.stem_settings.get_bool("alwaysBranch", false);
          if (!value)
            return std::unexpected(std::move(value.error()));
          always_branch = *value;
        }
        bool add_branch = *always_branch || random.uniform_uint(2 + i) != 0;
        // The skipped-branch step historically kept double precision.
        double next = add_branch ? static_cast<double>(branch_y) + h
          : branch_y + step / static_cast<float>(1 + random.uniform_uint(4));
        if (next > std::numeric_limits<float>::max())
          return std::unexpected(PlantException::format("Tree branch position exceeds the float range"));
        float next_y = static_cast<float>(next);
        if (next_y <= branch_y)
          return std::unexpected(PlantException::format("Tree branch height cannot advance the branch position"));
        branch_y = next_y;
        if (add_branch) {
          float phase = next_rotation();
          auto branch_file = stem_file(branch);
          if (!branch_file)
            return std::unexpected(std::move(branch_file.error()));
          add_piece(*branch_file, config.stem_hue_shift, *branch_position, false, Stem, 0.0f,
            m_ceiling ? DontRotate : RotateBranch, phase);
          if (auto result = add_leaves(branch_leaves, "branchLeaves", branch_key, *branch_position, RotateLeaves, phase); !result)
            return std::unexpected(std::move(result.error()));
        }
      }
    }
  }

  auto crowns = optional_tree_object(config.stem_settings, "crown");
  if (!crowns)
    return std::unexpected(std::move(crowns.error()));
  if (*crowns && !(*crowns)->empty()) {
    auto [key, part] = pick_tree_candidate(**crowns, random);
    auto attachment = part.get("attachment");
    if (!attachment)
      return std::unexpected(std::move(attachment.error()));
    if (auto result = apply_attachment(*attachment, "bx", "by"); !result)
      return std::unexpected(std::move(result.error()));
    float phase = next_rotation();
    auto file = stem_file(part);
    if (!file)
      return std::unexpected(std::move(file.error()));
    add_piece(*file, config.stem_hue_shift, offset, false, Stem, 0.0f, m_ceiling ? DontRotate : RotateCrownBranch, phase);
    LeafPool leaves;
    if (auto result = add_leaves(leaves, "crownLeaves", key, offset, RotateCrownLeaves, phase); !result)
      return std::unexpected(std::move(result.error()));
  }

  // Preserve the original sort (including its order for equal z levels).
  std::sort(m_pieces.begin(), m_pieces.end(), [](PlantPiece const& a, PlantPiece const& b) { return a.z_level < b.z_level; });
  if (auto result = initialize_piece_geometry(); !result)
    return std::unexpected(std::move(result.error()));
  return setup_net_states();
}

std::expected<Json, StarException> Plant::disk_store() const {
  auto damage = m_tile_damage_parameters.to_json();
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  auto pieces = write_pieces_to_json();
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  return JsonObject{
    {"tilePosition", json_from_vec2i(m_tile_position)},
    {"ceiling", m_ceiling},
    {"stemDropConfig", m_stem_drop_config},
    {"foliageDropConfig", m_foliage_drop_config},
    {"saplingDropConfig", m_sapling_drop_config},
    {"descriptions", m_descriptions},
    {"ephemeral", m_ephemeral},
    {"tileDamageParameters", std::move(*damage)},
    {"fallsWhenDead", m_falls_when_dead},
    {"pieces", std::move(*pieces)},
  };
}

std::expected<ByteArray, StarException> Plant::net_store(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  if (auto result = ds.viwrite(m_tile_position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viwrite(m_tile_position[1]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds << m_ceiling << m_stem_drop_config << m_foliage_drop_config << m_sapling_drop_config
      << m_descriptions << m_ephemeral << m_tile_damage_parameters << m_falls_when_dead; !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tile_damage_status->net_store(ds, rules); !result)
    return std::unexpected(std::move(result.error()));
  auto pieces = write_pieces();
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  if (auto result = ds.write(*pieces); !result)
    return std::unexpected(std::move(result.error()));
  return ds.take_data();
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& root, GrassVariant const& config, std::uint64_t seed) {
  auto entity = create_uninitialized(root);
  if (!entity)
    return entity;
  if (auto result = (*entity)->initialize(config, seed); !result)
    return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(GrassVariant const& config, std::uint64_t seed) {
  if (!is_finite(config.hue_shift))
    return std::unexpected(PlantException::format("Grass hue shift must be finite"));
  m_ceiling = config.ceiling;
  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tile_damage_parameters = config.tile_damage_parameters;

  RandomSource random(seed);
  auto image = random.sample_ref(config.images);
  if (!image)
    return std::unexpected(std::move(image.error()));
  auto image_name = AssetPath::relative_to(config.directory, image->get());

  Vec2F offset;
  if (m_ceiling) {
    auto metadata = root().image_metadata_database();
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    auto size = (*metadata)->image_size(image_name);
    if (!size)
      return std::unexpected(std::move(size.error()));
    offset[1] = 1.0f - static_cast<float>(size->y()) / tile_pixels;
  }

  auto& piece = m_pieces.emplace_back();
  piece.image = Star::format("{}?hueshift={}", image_name, config.hue_shift);
  piece.offset = offset;
  piece.structural_segment = true;
  if (auto result = initialize_piece_geometry(); !result)
    return std::unexpected(std::move(result.error()));
  return setup_net_states();
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& root, BushVariant const& config, std::uint64_t seed) {
  auto entity = create_uninitialized(root);
  if (!entity)
    return entity;
  if (auto result = (*entity)->initialize(config, seed); !result)
    return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(BushVariant const& config, std::uint64_t seed) {
  if (!is_finite(config.base_hue_shift) || !is_finite(config.mod_hue_shift))
    return std::unexpected(PlantException::format("Bush hue shifts must be finite"));
  m_ceiling = config.ceiling;
  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tile_damage_parameters = config.tile_damage_parameters;

  RandomSource random(seed);
  auto selected = random.sample_ref(config.shapes);
  if (!selected)
    return std::unexpected(std::move(selected.error()));
  auto const& shape = selected->get();
  auto image_name = AssetPath::relative_to(config.directory, shape.image);

  Vec2F offset;
  if (m_ceiling) {
    auto assets = root().assets();
    if (!assets)
      return std::unexpected(std::move(assets.error()));
    auto image = (*assets)->image(image_name);
    if (!image)
      return std::unexpected(std::move(image.error()));
    offset[1] = 1.0f - static_cast<float>((*image)->height()) / tile_pixels;
  }
  {
    auto& piece = m_pieces.emplace_back();
    piece.image = Star::format("{}?hueshift={}", image_name, config.base_hue_shift);
    piece.offset = offset;
    piece.structural_segment = true;
  }
  if (!shape.mods.empty()) {
    auto mod = random.sample_ref(shape.mods);
    if (!mod)
      return std::unexpected(std::move(mod.error()));
    if (!mod->get().empty()) {
      auto& piece = m_pieces.emplace_back();
      piece.image = Star::format("{}?hueshift={}", AssetPath::relative_to(config.directory, mod->get()), config.mod_hue_shift);
      piece.offset = offset;
    }
  }
  if (auto result = initialize_piece_geometry(); !result)
    return std::unexpected(std::move(result.error()));
  return setup_net_states();
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& root, Json const& disk_store) {
  auto entity = create_uninitialized(root);
  if (!entity)
    return entity;
  if (auto result = (*entity)->initialize(disk_store); !result)
    return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(Json const& disk_store) {
  auto position = disk_store.get("tilePosition");
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto tile_position = json_to_vec2i(*position);
  if (!tile_position)
    return std::unexpected(std::move(tile_position.error()));
  m_tile_position = *tile_position;
  auto ceiling = disk_store.get_bool("ceiling");
  if (!ceiling)
    return std::unexpected(std::move(ceiling.error()));
  m_ceiling = *ceiling;
  auto stem_drop = disk_store.get("stemDropConfig");
  if (!stem_drop)
    return std::unexpected(std::move(stem_drop.error()));
  m_stem_drop_config = std::move(*stem_drop);
  auto foliage_drop = disk_store.get("foliageDropConfig");
  if (!foliage_drop)
    return std::unexpected(std::move(foliage_drop.error()));
  m_foliage_drop_config = std::move(*foliage_drop);
  auto sapling_drop = disk_store.get("saplingDropConfig");
  if (!sapling_drop)
    return std::unexpected(std::move(sapling_drop.error()));
  m_sapling_drop_config = std::move(*sapling_drop);
  auto descriptions = disk_store.get("descriptions");
  if (!descriptions)
    return std::unexpected(std::move(descriptions.error()));
  m_descriptions = std::move(*descriptions);
  auto ephemeral = disk_store.get_bool("ephemeral");
  if (!ephemeral)
    return std::unexpected(std::move(ephemeral.error()));
  m_ephemeral = *ephemeral;
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto damage_json = disk_store.get("tileDamageParameters");
  if (!damage_json)
    return std::unexpected(std::move(damage_json.error()));
  auto damage = TileDamageParameters::create(*std::move(*assets), std::move(*damage_json));
  if (!damage)
    return std::unexpected(std::move(damage.error()));
  m_tile_damage_parameters = std::move(*damage);
  auto falls = disk_store.get_bool("fallsWhenDead");
  if (!falls)
    return std::unexpected(std::move(falls.error()));
  m_falls_when_dead = *falls;
  auto pieces = disk_store.get("pieces");
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  if (auto result = read_pieces_from_json(*pieces); !result)
    return std::unexpected(std::move(result.error()));
  return setup_net_states();
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& root, ByteArray const& net_store, NetCompatibilityRules rules) {
  auto entity = create_uninitialized(root);
  if (!entity)
    return entity;
  if (auto result = (*entity)->initialize(net_store, rules); !result)
    return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(ByteArray const& net_store, NetCompatibilityRules rules) {
  DataStreamReader ds(net_store.span());
  ds.set_stream_compatibility_version(rules);
  if (auto result = ds.viread(m_tile_position[0]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds.viread(m_tile_position[1]); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ds >> m_ceiling >> m_stem_drop_config >> m_foliage_drop_config >> m_sapling_drop_config
      >> m_descriptions >> m_ephemeral >> m_tile_damage_parameters >> m_falls_when_dead; !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_tile_damage_status->net_load(ds, rules); !result)
    return std::unexpected(std::move(result.error()));
  auto pieces = ds.read<ByteArray>();
  if (!pieces)
    return std::unexpected(std::move(pieces.error()));
  if (auto result = pieces->empty() ? initialize_piece_geometry() : read_pieces(*pieces); !result)
    return std::unexpected(std::move(result.error()));
  return setup_net_states();
}

Plant::Plant(Root& root) : Entity(root) {}

EntityType Plant::entity_type() const {
  return EntityType::Plant;
}

std::expected<void, StarException> Plant::init(World& world, EntityId entity_id, EntityMode mode) {
  if (auto initialized = Entity::init(world, entity_id, mode); !initialized)
    return std::unexpected(std::move(initialized.error()));
  m_tile_position = world.geometry().xwrap(m_tile_position);
  return {};
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Plant::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(from_version, rules);
}

std::expected<void, StarException> Plant::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  if (auto read = m_net_group.read_net_state(data, interpolation_time, rules); !read)
    return std::unexpected(std::move(read.error()));
  if (m_pieces_update_pending)
    return load_net_states();
  return {};
}

std::expected<void, StarException> Plant::enable_interpolation(float extrapolation_hint) {
  // Only enable plant interpolation when it actually matters, for things that
  // generate PlantDrops so that they match when the PlantDrops appear.
  if (m_falls_when_dead)
    return m_net_group.enable_net_interpolation(extrapolation_hint);
  return {};
}

std::expected<void, StarException> Plant::disable_interpolation() {
  return m_net_group.disable_net_interpolation();
}

std::expected<String, StarException> Plant::description() const {
  return m_descriptions.get_string("description");
}

Vec2F Plant::position() const {
  return Vec2F(m_tile_position);
}

RectF Plant::meta_bound_box() const {
  return m_piece_geometry.meta_bound_box;
}

bool Plant::ephemeral() const {
  return m_ephemeral;
}

Vec2I Plant::tile_position() const {
  return m_tile_position;
}

std::expected<void, StarException> Plant::set_tile_position(Vec2I const& tile_position) {
  m_tile_position = tile_position;
  return {};
}

List<Vec2I> Plant::spaces() const {
  return m_piece_geometry.spaces;
}

List<Vec2I> Plant::roots() const {
  return m_piece_geometry.roots;
}

Vec2I Plant::primary_root() const {
  return m_ceiling ? Vec2I(0, 1) : Vec2I(0, -1);
}

bool Plant::ceiling() const {
  return m_ceiling;
}

std::expected<bool, StarException> Plant::should_destroy() const {
  return m_broken || m_pieces.empty();
}

std::expected<bool, StarException> Plant::check_broken() {
  if (m_broken)
    return true;

  if (m_pending_fall) {
    auto current_world = world();
    if (!current_world)
      return std::unexpected(std::move(current_world.error()));
    if (auto resumed = resume_felling(current_world->get()); !resumed)
      return std::unexpected(std::move(resumed.error()));
    return false;
  }

  auto supported = allSpacesOccupied(m_piece_geometry.roots);
  if (!supported)
    return std::unexpected(std::move(supported.error()));
  if (!*supported && m_falls_when_dead) {
    auto current_world = world();
    if (!current_world)
      return std::unexpected(std::move(current_world.error()));
    if (auto fallen = break_at_position(current_world->get(), m_tile_position, Vec2F(m_tile_position)); !fallen)
      return std::unexpected(std::move(fallen.error()));
    return false;
  }

  if (*supported) {
    auto obstructed = anySpacesOccupied(m_piece_geometry.spaces);
    if (!obstructed)
      return std::unexpected(std::move(obstructed.error()));
    m_broken = *obstructed;
  } else {
    m_broken = true;
  }
  return m_broken;
}

List<Plant::PlantPiece> Plant::pieces() const {
  return m_pieces;
}

RectF Plant::interactive_bound_box() const {
  return m_piece_geometry.bound_box;
}

std::expected<Plant::PieceGeometry, StarException> Plant::prepare_piece_geometry(List<PlantPiece>& pieces) const {
  if (!pieces.empty()) {
    auto metadata = root().image_metadata_database();
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));

    for (auto& piece : pieces) {
      piece.image_path = piece.image;
      auto size = (*metadata)->image_size(piece.image_path);
      if (!size)
        return std::unexpected(std::move(size.error()));
      auto occupied = (*metadata)->image_spaces(piece.image_path, piece.offset * tile_pixels, plant_scan_threshold, piece.flip);
      if (!occupied)
        return std::unexpected(std::move(occupied.error()));
      piece.image_size = *size;
      piece.spaces = Set<Vec2I>::from(std::move(*occupied));
    }
  }
  return derive_piece_geometry(pieces);
}

Plant::PieceGeometry Plant::derive_piece_geometry(List<PlantPiece> const& pieces) const {
  Set<Vec2I> spaces;
  // The origin remains occupied even when the base image contributes no tiles.
  spaces.add({0, 0});
  for (auto const& piece : pieces)
    spaces.insert_range(piece.spaces);

  PieceGeometry geometry;
  geometry.spaces.reserve(spaces.size());
  RectI bounds = RectI::null();
  int const root_y = primary_root()[1];
  for (auto const& space : spaces) {
    geometry.spaces.push_back(space);
    bounds.combine(space);
    if (space[1] == 0)
      geometry.roots.emplace_back(space[0], root_y);
  }
  geometry.bound_box = RectF(bounds);
  // The visual box includes the last occupied tile and a one-tile margin.
  geometry.meta_bound_box = RectF(geometry.bound_box.min() - Vec2F(1, 1), geometry.bound_box.max() + Vec2F(2, 2));
  return geometry;
}

void Plant::commit_piece_geometry(PieceGeometry&& geometry) {
  m_piece_geometry = std::move(geometry);
}

std::expected<void, StarException> Plant::initialize_piece_geometry() {
  auto geometry = prepare_piece_geometry(m_pieces);
  if (!geometry)
    return std::unexpected(std::move(geometry.error()));
  commit_piece_geometry(std::move(*geometry));
  return {};
}

std::expected<void, StarException> Plant::replace_pieces(List<PlantPiece>&& pieces) {
  auto geometry = prepare_piece_geometry(pieces);
  if (!geometry)
    return std::unexpected(std::move(geometry.error()));

  m_pieces = std::move(pieces);
  commit_piece_geometry(std::move(*geometry));
  m_pieces_updated = true;
  return {};
}

namespace {
constexpr float plant_wind_period = 628.32f;
}

std::expected<void, StarException> Plant::update(float dt, std::uint64_t) {
  m_previous_wind_time = m_wind_time;
  m_previous_wind_level = m_wind_level;
  m_wind_time = std::fmod(m_wind_time + dt, plant_wind_period);
  auto active_world = world();
  if (!active_world)
    return std::unexpected(std::move(active_world.error()));
  // A failed world publication need not be followed by another hit or a break check.
  if (m_pending_fall) {
    if (auto resumed = resume_felling(active_world->get()); !resumed)
      return resumed;
  }
  auto wind = active_world->get().windLevel(Vec2F(m_tile_position));
  if (!wind)
    return std::unexpected(std::move(wind.error()));
  m_wind_level = *wind;

  if (isMaster()) {
    if (m_tile_damage_status->damaged())
      m_tile_damage_status->recover(m_tile_damage_parameters, dt);
  } else {
    if (m_tile_damage_status->damaged() && !m_tile_damage_status->damage_protected()) {
      float damage_effect = m_tile_damage_status->damage_effect_percentage();
      m_wind_time += damage_effect * 10 * dt;
      m_wind_level += damage_effect * 20;
    }

    if (auto ticked = m_net_group.tick_net_interpolation(dt); !ticked)
      return std::unexpected(std::move(ticked.error()));
    if (m_pieces_update_pending)
      return load_net_states();
  }
  return {};
}

std::expected<bool, StarException> Plant::submit_damage_effect(RenderCallback& callback, DamageEffect& effect) {
  if (!effect.particles_prepared) {
    auto config = m_stem_drop_config.get("particles", JsonObject()).and_then([](Json const& particles) {
      return particles.get("damageTree", JsonObject());
    });
    if (!config) return std::unexpected(std::move(config.error()));
    auto options = config->find("options");
    if (!options) return std::unexpected(std::move(options.error()));
    auto hue = m_stem_drop_config.get_float("hueshift", 0);
    if (!hue) return std::unexpected(std::move(hue.error()));
    if (!is_finite(*hue))
      return std::unexpected(PlantException("Plant damage particle hue shift must be finite"));
    auto density = config->get_float("density", 1);
    if (!density) return std::unexpected(std::move(density.error()));
    double rounded_count = std::ceil(static_cast<double>(*density));
    if (!is_finite(*density) || *density < 0 || rounded_count > std::numeric_limits<std::uint32_t>::max())
      return std::unexpected(PlantException("Plant damage particle count is out of range"));
    if (*options && !(*options)->get().is_null()) {
      auto array = (*options)->get().array_ptr();
      if (!array) return std::unexpected(std::move(array.error()));
      if (!(*array)->empty()) {
        effect.remaining = static_cast<std::uint32_t>(rounded_count);
        effect.particle_options = std::move(*array);
      }
    }
    effect.hue_shift = *hue / 360.0f;
    effect.particles_prepared = true;
  }

  auto& random = root().random();
  std::shared_ptr<ParticleDatabase const> database;
  constexpr std::size_t particle_sample_budget = 256;
  for (std::size_t budget = particle_sample_budget; effect.remaining != 0 && budget != 0; --budget, --effect.remaining) {
    if (!effect.pending_particle) {
      if (!effect.selected_particle) {
        auto selected = random.sample_ref(*effect.particle_options);
        if (!selected) return std::unexpected(std::move(selected.error()));
        effect.selected_particle = selected->get();
      }
      auto const& spec = *effect.selected_particle;
      if (!spec.is_null()) {
        auto size = spec.size();
        if (!size) return std::unexpected(std::move(size.error()));
        if (*size != 0) {
          if (!database) {
            auto loaded = root().particle_database();
            if (!loaded) return std::unexpected(std::move(loaded.error()));
            database = std::move(*loaded);
          }
          auto particle = database->particle(spec);
          if (!particle) return std::unexpected(std::move(particle.error()));
          particle->color.hue_shift(effect.hue_shift);
          if (!particle->string.empty()) {
            particle->string = Star::format("{}?hueshift={}", particle->string, effect.hue_shift);
            particle->image = particle->string;
          }
          particle->position = {effect.offset.x() + random.uniform_float(), effect.offset.y() + random.uniform_float()};
          particle->translate(effect.origin);
          effect.pending_particle = std::move(*particle);
        }
      }
    }
    if (effect.pending_particle) {
      if (auto added = callback.add_particle(std::move(*effect.pending_particle)); !added)
        return std::unexpected(std::move(added.error()));
      effect.pending_particle.reset();
    }
    effect.selected_particle.reset();
  }
  if (effect.remaining != 0)
    return false;

  if (!effect.pending_audio) {
    if (!effect.selected_sound) {
      auto sounds = m_stem_drop_config.get("sounds", JsonObject());
      if (!sounds) return std::unexpected(std::move(sounds.error()));
      auto options = sounds->find("damageTree");
      if (!options) return std::unexpected(std::move(options.error()));
      if (!*options || (*options)->get().is_null())
        return true;
      auto array = (*options)->get().array_ptr();
      if (!array) return std::unexpected(std::move(array.error()));
      if ((*array)->empty())
        return true;
      auto sound = random.sample_ref(**array);
      if (!sound) return std::unexpected(std::move(sound.error()));
      effect.selected_sound = sound->get();
      effect.sound_position = effect.origin + Vec2F(effect.offset.x() + random.uniform_float(), effect.offset.y() + random.uniform_float());
    }
    auto file = effect.selected_sound->get_string("file");
    if (!file) return std::unexpected(std::move(file.error()));
    auto volume = effect.selected_sound->get_float("volume", 1.0f);
    if (!volume) return std::unexpected(std::move(volume.error()));
    auto assets = root().assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto audio = (*assets)->audio(*file);
    if (!audio) return std::unexpected(std::move(audio.error()));
    auto instance = AudioInstance::create(**audio);
    if (!instance) return std::unexpected(std::move(instance.error()));
    (*instance)->set_position(effect.sound_position);
    (*instance)->set_volume(*volume);
    effect.pending_audio = std::move(*instance);
  }
  if (auto added = callback.add_audio(effect.pending_audio); !added)
    return std::unexpected(std::move(added.error()));
  return true;
}

bool Plant::has_pending_render_events() const {
  return m_damage_effect || (m_tile_damage_event && m_stem_drop_config.type() == Json::Type::Object);
}

std::expected<void, StarException> Plant::collect_render_events(RenderCallback& callback) {
  if (m_damage_effect) {
    auto completed = submit_damage_effect(callback, *m_damage_effect);
    if (!completed) return std::unexpected(std::move(completed.error()));
    if (*completed)
      m_damage_effect.reset();
    return {};
  }

  auto damage_offset = root().random().uniform_float(-0.1f, 0.1f);
  if (!damage_offset) return std::unexpected(std::move(damage_offset.error()));
  m_presentation_damage_x_offset = *damage_offset * m_tile_damage_status->damage_effect_percentage();
  if (!std::exchange(m_tile_damage_event, false) || m_stem_drop_config.type() != Json::Type::Object)
    return {};

  DamageEffect effect{position(), {m_tile_damage_x, m_tile_damage_y}};
  auto completed = submit_damage_effect(callback, effect);
  if (!completed || !*completed)
    m_damage_effect = std::make_unique<DamageEffect>(std::move(effect));
  if (!completed) return std::unexpected(std::move(completed.error()));
  return {};
}

std::expected<void, StarException> Plant::render(RenderCallback& render_callback) {
  constexpr float pixel_size = 1.0f / tile_pixels;
  auto world_position = position();
  auto base_transform = Mat3F::scaling(pixel_size);
  bool sway = inWorld() && (m_wind_level != 0.0f || m_previous_wind_level != 0.0f);
  // Grass and ceiling plants need no wind sample; rotating pieces share one.
  struct WindSample {
    float phase;
    float position_phase;
    float sign;
    float intensity;
    float bias;
  };
  std::optional<WindSample> wind;

  for (auto const& piece : m_pieces) {
    auto size = Vec2F(piece.image_size) / tile_pixels;
    Vec2F offset = piece.offset;
    if (m_ceiling ? offset.y() <= m_tile_damage_y : offset.y() + size.y() >= m_tile_damage_y)
      offset[0] += m_presentation_damage_x_offset;

    // The plant image is already validated and does not need centering.
    Drawable drawable;
    drawable.part = Drawable::ImagePart{piece.image_path, base_transform};
    drawable.position = offset;
    if (piece.flip)
      drawable.scale(Vec2F(-1, 1));

    if (piece.rotation_type != DontRotate) {
      float rotation = 0.0f;
      if (sway) {
        if (!wind) {
          float interpolation = std::clamp(render_callback.presentation().interpolation(), 0.0f, 1.0f);
          float level = std::lerp(m_previous_wind_level, m_wind_level, interpolation);
          float intensity = std::fabs(level);
          wind.emplace(
            m_wind_time + std::remainder(m_previous_wind_time - m_wind_time, plant_wind_period) * (1.0f - interpolation),
            world_position.x() / 10.0f, std::copysign(0.00117f, level), intensity, intensity / 300.0f);
        }
        rotation = wind->sign * (std::sin(wind->phase + piece.rotation_offset * 1.4f + wind->position_phase) * wind->intensity - wind->bias);
      }
      if (piece.rotation_type == RotateCrownBranch || piece.rotation_type == RotateCrownLeaves) {
        drawable.rotate(rotation * 0.7f, piece.offset + Vec2F(size.x() / 2.0f, 0));
        drawable.translate(Vec2F(0, -0.40f));
      } else {
        drawable.rotate(rotation, piece.offset + size / 2.0f);
      }
    }
    drawable.translate(world_position);
    if (auto result = render_callback.add_drawable(std::move(drawable), render_layer_plant); !result)
      return std::unexpected(std::move(result.error()));
  }

  return {};
}

std::expected<void, StarException> Plant::read_pieces(ByteArray const& pieces) {
  // An empty field means no update; a serialized empty list clears the plant.
  if (pieces.empty())
    return {};

  List<PlantPiece> decoded;
  DataStreamReader ds(pieces.span());
  auto read = ds.read_container(decoded, [](DataStream& stream, PlantPiece& piece) -> std::expected<void, StarException> {
    if (auto result = stream.read(piece.image); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.offset[0]); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.offset[1]); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.rotation_type); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.rotation_offset); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.structural_segment); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.kind); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.read(piece.segment_idx); !result)
      return std::unexpected(std::move(result.error()));
    return stream.read(piece.flip);
  });
  if (!read)
    return std::unexpected(std::move(read.error()));
  return replace_pieces(std::move(decoded));
}

std::expected<ByteArray, StarException> Plant::write_pieces() const {
  return DataStreamBuffer::serialize_container(m_pieces, [](DataStream& stream, PlantPiece const& piece) -> std::expected<void, StarException> {
    if (auto result = stream.write(piece.image); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.offset[0]); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.offset[1]); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.rotation_type); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.rotation_offset); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.structural_segment); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.kind); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = stream.write(piece.segment_idx); !result)
      return std::unexpected(std::move(result.error()));
    return stream.write(piece.flip);
  });
}

std::expected<void, StarException> Plant::read_pieces_from_json(Json const& pieces) {
  auto decoded = json_to_list<PlantPiece>(pieces, [](Json const& value) -> std::expected<PlantPiece, StarException> {
    PlantPiece piece;
    auto image = value.get_string("image");
    if (!image)
      return std::unexpected(std::move(image.error()));
    piece.image = std::move(*image);
    auto offset = value.get("offset");
    if (!offset)
      return std::unexpected(std::move(offset.error()));
    auto position = json_to_vec2f(*offset);
    if (!position)
      return std::unexpected(std::move(position.error()));
    piece.offset = *position;
    auto rotation_name = value.get_string("rotationType");
    if (!rotation_name)
      return std::unexpected(std::move(rotation_name.error()));
    if (!rotation_type_names)
      return std::unexpected(rotation_type_names.error());
    auto rotation_type = rotation_type_names->get_left(*rotation_name);
    if (!rotation_type)
      return std::unexpected(std::move(rotation_type.error()));
    piece.rotation_type = rotation_type->get();
    auto rotation_offset = value.get_float("rotationOffset");
    if (!rotation_offset)
      return std::unexpected(std::move(rotation_offset.error()));
    piece.rotation_offset = *rotation_offset;
    auto structural_segment = value.get_bool("structuralSegment");
    if (!structural_segment)
      return std::unexpected(std::move(structural_segment.error()));
    piece.structural_segment = *structural_segment;
    auto kind = value.get_int("kind");
    if (!kind)
      return std::unexpected(std::move(kind.error()));
    piece.kind = static_cast<PlantPieceKind>(*kind);
    auto segment_idx = value.get_int("segmentIdx");
    if (!segment_idx)
      return std::unexpected(std::move(segment_idx.error()));
    piece.segment_idx = *segment_idx;
    auto flip = value.get_bool("flip");
    if (!flip)
      return std::unexpected(std::move(flip.error()));
    piece.flip = *flip;
    return piece;
  });
  if (!decoded)
    return std::unexpected(std::move(decoded.error()));
  return replace_pieces(std::move(*decoded));
}

std::expected<Json, StarException> Plant::write_pieces_to_json() const {
  return json_from_list<PlantPiece>(m_pieces, [](PlantPiece const& piece) -> std::expected<Json, StarException> {
    if (!rotation_type_names)
      return std::unexpected(rotation_type_names.error());
    auto rotation_name = rotation_type_names->get_right(piece.rotation_type);
    if (!rotation_name)
      return std::unexpected(std::move(rotation_name.error()));
    return JsonObject{
      {"image", piece.image},
      {"offset", json_from_vec2f(piece.offset)},
      {"rotationType", rotation_name->get()},
      {"rotationOffset", piece.rotation_offset},
      {"structuralSegment", piece.structural_segment},
      {"kind", piece.kind},
      {"segmentIdx", piece.segment_idx},
      {"flip", piece.flip},
    };
  });
}

std::expected<void, StarException> Plant::setup_net_states() {
  if (auto result = m_net_group.add_net_element(*m_tile_damage_status); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_net_group.add_net_element(m_pieces_net_state); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_net_group.add_net_element(m_tile_damage_x_net_state); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_net_group.add_net_element(m_tile_damage_y_net_state); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_net_group.add_net_element(m_tile_damage_event_net_state); !result)
    return std::unexpected(std::move(result.error()));
  m_net_group.set_needs_store_callback([this]() { return store_net_states(); });
  m_net_group.set_needs_load_callback([this](bool) { return load_net_states(); });
  return {};
}

std::expected<void, StarException> Plant::load_net_states() {
  // Retain the application request when parsing or asset lookup fails.
  m_pieces_update_pending |= m_pieces_net_state.pull_updated();
  if (m_pieces_update_pending) {
    if (auto read = read_pieces(m_pieces_net_state.get()); !read)
      return std::unexpected(std::move(read.error()));
    m_pieces_update_pending = false;
  }

  m_tile_damage_x = m_tile_damage_x_net_state.get();
  m_tile_damage_y = m_tile_damage_y_net_state.get();
  if (m_tile_damage_event_net_state.pull_occurred()) {
    m_tile_damage_event = true;
    m_tile_damage_event_trigger = true;
  }
  return {};
}

std::expected<void, StarException> Plant::store_net_states() {
  if (m_pieces_updated) {
    auto pieces = write_pieces();
    if (!pieces)
      return std::unexpected(std::move(pieces.error()));
    if (m_pieces_net_state.get() != *pieces)
      m_pieces_net_state.push(std::move(*pieces));
    m_pieces_updated = false;
  }
  m_tile_damage_x_net_state.set(m_tile_damage_x);
  m_tile_damage_y_net_state.set(m_tile_damage_y);
  if (m_tile_damage_event_trigger) {
    m_tile_damage_event_trigger = false;
    m_tile_damage_event_net_state.trigger();
  }
  return {};
}

std::expected<bool, StarException> Plant::damage_tiles(List<Vec2I> const& positions, Vec2F const& source_position, TileDamage const& tile_damage) {
  if (positions.empty())
    return std::unexpected(PlantException("Cannot damage a plant without a hit position"));

  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  if (m_pending_fall) {
    if (auto resumed = resume_felling(current_world->get()); !resumed)
      return std::unexpected(std::move(resumed.error()));
    return true;
  }

  auto geometry = current_world->get().geometry();
  auto position = base_damage_position(positions, geometry);
  auto displacement = geometry.diff(position, m_tile_position);
  m_tile_damage_status->damage(m_tile_damage_parameters, tile_damage);
  m_tile_damage_x = displacement[0];
  m_tile_damage_y = displacement[1];
  m_tile_damage_event = true;
  m_tile_damage_event_trigger = true;

  if (!m_tile_damage_status->dead())
    return false;
  if (m_falls_when_dead) {
    m_tile_damage_status->reset();
    if (auto fallen = break_at_position(current_world->get(), position, source_position); !fallen)
      return std::unexpected(std::move(fallen.error()));
  } else {
    m_broken = true;
  }
  return true;
}

std::expected<void, StarException> Plant::break_at_position(World& world, Vec2I const& position, Vec2F const& source_position) {
  auto internal_position = world.geometry().diff(position, m_tile_position);
  auto cut = m_pieces.size();
  auto last_structural = cut;
  for (std::size_t i = 0; i < m_pieces.size(); ++i) {
    auto const& piece = m_pieces[i];
    if (!piece.structural_segment)
      continue;
    last_structural = i;
    if ((cut == m_pieces.size() || piece.segment_idx < m_pieces[cut].segment_idx) && piece.spaces.contains(internal_position))
      cut = i;
  }
  if (cut == m_pieces.size())
    cut = last_structural;
  if (cut == m_pieces.size())
    return {};

  auto const& cut_piece = m_pieces[cut];
  Vec2F break_point = Vec2F(position) - Vec2F(m_tile_position);
  if (!cut_piece.spaces.empty()) {
    RectF bounds = RectF::null();
    for (auto const& space : cut_piece.spaces) {
      bounds.combine(Vec2F(space));
      bounds.combine(Vec2F(space) + Vec2F(1, 1));
    }
    break_point[0] = (bounds.max()[0] + bounds.min()[0]) / 2.0f;
    break_point[1] = m_ceiling ? bounds.max()[1] : bounds.min()[1];
  }

  auto plant_description = description();
  if (!plant_description)
    return std::unexpected(std::move(plant_description.error()));
  auto random = root().random().uniform_float(-0.3f, 0.3f);
  if (!random)
    return std::unexpected(std::move(random.error()));

  PendingFall pending;
  pending.break_point = break_point;
  pending.world_break_point = break_point + Vec2F(m_tile_position);
  pending.fall_vector = (pending.world_break_point - source_position).normalized();
  pending.description = std::move(*plant_description);
  pending.random = *random;

  // Count groups in first-encounter order, then fill their contiguous slots
  // in original piece order. Publication consumes the groups from the back.
  auto cut_segment = cut_piece.segment_idx;
  HashMap<int, std::size_t> groups;
  for (auto const& piece : m_pieces) {
    if (piece.segment_idx < cut_segment)
      continue;
    auto [group, inserted] = groups.try_emplace(piece.segment_idx, groups.size());
    if (inserted)
      pending.segment_ends.push_back(0);
    ++pending.segment_ends[group->second];
  }
  std::partial_sum(pending.segment_ends.begin(), pending.segment_ends.end(), pending.segment_ends.begin());
  pending.indices.resize(pending.segment_ends.back());
  for (auto& cursor : groups.value_view())
    cursor = cursor == 0 ? 0 : pending.segment_ends[cursor - 1];

  pending.slots.resize(m_pieces.size(), PendingFall::no_slot);
  for (std::size_t i = 0; i < m_pieces.size(); ++i) {
    auto segment = m_pieces[i].segment_idx;
    if (segment >= cut_segment) {
      auto slot = groups.find(segment)->second++;
      pending.slots[i] = slot;
      pending.indices[slot] = i;
    }
  }
  auto published = publish_fall(world, pending);
  if (!published)
    m_pending_fall = std::make_unique<PendingFall>(std::move(pending));
  return published;
}

std::expected<void, StarException> Plant::resume_felling(World& world) {
  auto published = publish_fall(world, *m_pending_fall);
  if (published)
    m_pending_fall.reset();
  return published;
}

std::expected<void, StarException> Plant::publish_fall(World& world, PendingFall& pending) {
  auto publish = [&]() -> std::expected<void, StarException> {
    while (!pending.segment_ends.empty()) {
      if (!pending.unpublished_drop) {
        auto group_count = pending.segment_ends.size();
        auto begin = group_count == 1 ? 0 : pending.segment_ends[group_count - 2];
        auto end = pending.segment_ends.back();
        auto segment = std::span(pending.indices).subspan(begin, end - begin);
        auto drop = PlantDrop::create_segment(root(), m_pieces, segment, pending.break_point, pending.world_break_point,
            pending.fall_vector, pending.description, m_ceiling, m_stem_drop_config, m_foliage_drop_config,
            m_sapling_drop_config, pending.first, pending.random);
        if (!drop)
          return std::unexpected(std::move(drop.error()));
        pending.unpublished_drop = std::move(*drop);
      }
      if (auto added = world.addEntity(pending.unpublished_drop); !added)
        return added;
      pending.unpublished_drop.reset();
      pending.segment_ends.pop_back();
      pending.first = false;
    }
    return {};
  };
  auto result = publish();

  // Remove exactly the published suffix of the grouped indices, even on an
  // error. Compact standing wood once, preserving both orders for a retry.
  auto remaining = pending.segment_ends.empty() ? 0 : pending.segment_ends.back();
  if (remaining != pending.indices.size()) {
    std::size_t kept = 0;
    for (std::size_t i = 0; i < m_pieces.size(); ++i) {
      auto slot = pending.slots[i];
      if (slot != PendingFall::no_slot) {
        if (slot >= remaining)
          continue;
        pending.indices[slot] = kept;
      }
      if (kept != i) {
        m_pieces[kept] = std::move(m_pieces[i]);
        pending.slots[kept] = slot;
      }
      ++kept;
    }
    m_pieces.resize(kept);
    pending.slots.resize(kept);
    pending.indices.resize(remaining);
    commit_piece_geometry(derive_piece_geometry(m_pieces));
    m_pieces_updated = true;
  }

  return result;
}

Vec2I Plant::base_damage_position(List<Vec2I> const& positions, WorldGeometry const& geometry) const {
  auto result = positions.front();
  if (positions.size() == 1)
    return result;

  // Retain the original piece/space iteration order. At equal Y the ceiling
  // chooses the first match, while a floor plant chooses the last match.
  HashMap<Vec2U, std::size_t> hits;
  hits.reserve(positions.size());
  for (std::size_t i = 0; i < positions.size(); ++i) {
    auto index = geometry.index(positions[i]);
    if (m_ceiling)
      hits.try_emplace(index, i);
    else
      hits.insert_or_assign(index, i);
  }
  for (auto const& piece : m_pieces) {
    if (!piece.structural_segment)
      continue;
    for (auto const& space : piece.spaces) {
      auto hit = hits.find(geometry.index(m_tile_position + space));
      if (hit == hits.end())
        continue;
      auto const& position = positions[hit->second];
      if ((result[1] < position[1]) == m_ceiling)
        result = position;
    }
  }
  return result;
}

}
