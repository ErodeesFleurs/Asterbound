import std;
import star.asset_path;
import star.assets;
import star.audio;
import star.byte_array;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.directives;
import star.drawable;
import star.entity;
import star.entity_rendering;
import star.entity_rendering_types;
import star.format;
import star.game_types;
import star.image_metadata_database;
import star.item_descriptor;
import star.item_drop;
import star.json;
import star.list;
import star.math_common;
import star.matrix3;
import star.mixer;
import star.movement_controller;
import star.net_compatibility;
import star.particle;
import star.particle_database;
import star.plant;
import star.plant_drop;
import star.poly;
import star.random;
import star.rect;
import star.root;
import star.set;
import star.string;
import star.vector;
import star.world;

namespace Star {

namespace {
// Account for misses as well as emissions, and share the budget between the
// initial break and impact. An unfinished sample sequence resumes next tick.
constexpr std::size_t particle_sample_budget = 256;

Vec2F particle_world_position(Vec2F const& local, Vec2F const& origin, float rotation) {
  // Keep the legacy Release grouping when a burst stores its position
  // across ticks. Adding the origin after the completed rotation rounds
  // differently in world coordinates.
#pragma clang fp reassociate(off)
  float const cosine = std::cos(rotation);
  float const sine = std::sin(rotation);
  return {(origin.x() + local.x() * cosine) - local.y() * sine,
      (origin.y() + local.x() * sine) + local.y() * cosine};
}
}

PlantDrop::PlantDrop(Root& root) : Entity(root) {}

std::expected<void, StarException> PlantDrop::initialize_net_elements(Root& root) {
  auto controller = MovementController::create(root);
  if (!controller)
    return std::unexpected(std::move(controller.error()));
  m_movement_controller = std::move(*controller);
  if (auto added = m_net_group.add_net_element(*m_movement_controller); !added)
    return added;
  return m_net_group.add_net_element(m_spawned_drops);
}

std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create(Root& root, List<Plant::PlantPiece> pieces,
    Vec2F const& position, Vec2F const& strike_vector, String const& description, bool upside_down,
    Json stem_config, Json foliage_config, Json sapling_config, bool master, float random) {
  return create_fall<false>(root, std::as_const(pieces), Vec2F(), position, strike_vector, description, upside_down,
      std::move(stem_config), std::move(foliage_config), std::move(sapling_config), master, random);
}

std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create_segment(Root& root,
    List<Plant::PlantPiece> const& pieces, std::span<std::size_t const> indices, Vec2F const& break_point,
    Vec2F const& position, Vec2F const& strike_vector, String const& description, bool upside_down,
    Json stem_config, Json foliage_config, Json sapling_config, bool master, float random) {
  auto segment = indices | std::views::transform([&pieces](std::size_t index) -> Plant::PlantPiece const& {
    return pieces[index];
  });
  return create_fall<true>(root, segment, break_point, position, strike_vector, description, upside_down,
      std::move(stem_config), std::move(foliage_config), std::move(sapling_config), master, random);
}

template <bool prepared_paths, std::ranges::sized_range Pieces>
  requires std::ranges::input_range<Pieces>
    && std::same_as<std::ranges::range_reference_t<Pieces>, Plant::PlantPiece const&>
std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create_fall(Root& root, Pieces&& pieces,
    Vec2F const& break_point, Vec2F const& position, Vec2F const& strike_vector, String const& description,
    bool upside_down, Json stem_config, Json foliage_config, Json sapling_config, bool master, float random) {
  struct MakeShared final : PlantDrop {
    explicit MakeShared(Root& root) : Entity(root), PlantDrop(root) {}
  };
  auto entity = std::make_shared<MakeShared>(root);
  if (auto initialized = entity->initialize_net_elements(root); !initialized)
    return std::unexpected(std::move(initialized.error()));

  entity->m_stem_config = stem_config.is_null() ? Json(JsonObject()) : std::move(stem_config);
  entity->m_foliage_config = foliage_config.is_null() ? Json(JsonObject()) : std::move(foliage_config);
  entity->m_sapling_config = std::move(sapling_config);
  entity->m_master = master;
  entity->m_observed_live = true;
  entity->m_break_pending = true;
  entity->m_spawned_drops.set(false);
  entity->m_description = description;
  entity->m_movement_controller->set_position(position);
  if (!upside_down) {
    entity->m_rotation_rate = std::copysign(0.00001f, -strike_vector[0] + random);
    entity->m_rotation_fall_threshold = std::numbers::pi / (3 + random);
    entity->m_rotation_cap = std::numbers::pi - entity->m_rotation_fall_threshold;
  }

  auto tile_offset = Vec2I::round(break_point);
  RectF stem_bounds = RectF::null();
  RectF full_bounds = RectF::null();
  entity->m_pieces.reserve(std::ranges::size(pieces));
  for (auto const& piece : pieces) {
    RectF bounds = RectF::null();
    for (auto const& space : piece.spaces) {
      auto min = Vec2F(space - tile_offset);
      bounds.combine(min);
      bounds.combine(min + Vec2F(1, 1));
    }
    if (!bounds.is_null()) {
      full_bounds.combine(bounds);
      if (piece.structural_segment)
        stem_bounds.combine(bounds);
    }
    if constexpr (prepared_paths)
      entity->m_pieces.push_back({piece.image_path, piece.offset - break_point, piece.kind, piece.flip});
    else
      entity->m_pieces.push_back({AssetPath(piece.image), piece.offset, piece.kind, piece.flip});
  }

  entity->m_bounding_box = full_bounds.is_null() ? RectF(position, position) : full_bounds;
  entity->m_collision_rect = stem_bounds.is_null() ? entity->m_bounding_box : stem_bounds;
  return entity;
}

std::expected<std::shared_ptr<PlantDrop>, StarException> PlantDrop::create(Root& root, ByteArray const& net_store, NetCompatibilityRules rules) {
  struct MakeShared final : PlantDrop {
    explicit MakeShared(Root& root) : Entity(root), PlantDrop(root) {}
  };
  auto entity = std::make_shared<MakeShared>(root);
  if (auto initialized = entity->initialize_net_elements(root); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto initialized = entity->initialize(net_store, rules); !initialized)
    return std::unexpected(std::move(initialized.error()));
  return entity;
}

std::expected<void, StarException> PlantDrop::initialize(ByteArray const& net_store, NetCompatibilityRules rules) {
  DataStreamReader stream(net_store.span());
  stream.set_stream_compatibility_version(rules);
  if (auto read = stream.read(m_time); !read) return read;
  if (auto read = stream.read(m_master); !read) return read;
  if (auto read = stream.read(m_description); !read) return read;
  if (auto read = stream.read(m_bounding_box); !read) return read;
  if (auto read = stream.read(m_collision_rect); !read) return read;
  if (auto read = stream.read(m_rotation_rate); !read) return read;
  auto pieces = stream.read_container(m_pieces, [](DataStream& stream, PlantDropPiece& piece) -> std::expected<void, StarException> {
    if (auto read = stream.read(piece.image); !read) return read;
    if (auto read = stream.read(piece.offset[0]); !read) return read;
    if (auto read = stream.read(piece.offset[1]); !read) return read;
    if (auto read = stream.read(piece.flip); !read) return read;
    return stream.read(piece.kind);
  });
  if (!pieces) return pieces;
  if (auto read = stream.read(m_stem_config); !read) return read;
  if (auto read = stream.read(m_foliage_config); !read) return read;
  return stream.read(m_sapling_config);
}

std::expected<ByteArray, StarException> PlantDrop::net_store(NetCompatibilityRules) {
  DataStreamBuffer stream;
  if (auto written = stream.write(m_time); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_master); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_description); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_bounding_box); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_collision_rect); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_rotation_rate); !written) return std::unexpected(std::move(written.error()));
  auto pieces = stream.write_container(m_pieces, [](DataStream& stream, PlantDropPiece const& piece) -> std::expected<void, StarException> {
    if (auto written = stream.write(piece.image); !written) return written;
    if (auto written = stream.write(piece.offset[0]); !written) return written;
    if (auto written = stream.write(piece.offset[1]); !written) return written;
    if (auto written = stream.write(piece.flip); !written) return written;
    return stream.write(piece.kind);
  });
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  if (auto written = stream.write(m_stem_config); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_foliage_config); !written) return std::unexpected(std::move(written.error()));
  if (auto written = stream.write(m_sapling_config); !written) return std::unexpected(std::move(written.error()));
  return stream.take_data();
}

EntityType PlantDrop::entity_type() const {
  return EntityType::PlantDrop;
}

std::expected<void, StarException> PlantDrop::init(World& world, EntityId entity_id, EntityMode mode) {
  if (auto initialized = Entity::init(world, entity_id, mode); !initialized)
    return initialized;
  m_movement_controller->init(world);

  MovementParameters parameters;
  parameters.collision_poly = PolyF(RectF::with_center(m_collision_rect.center(), m_collision_rect.size() / 2.0f));
  parameters.ignore_platform_collision = true;
  parameters.gravity_multiplier = 0.2f;
  parameters.physics_effect_categories = StringSet({"plantdrop"});
  return m_movement_controller->apply_parameters(parameters);
}

std::expected<void, StarException> PlantDrop::uninit() {
  if (auto uninitialized = Entity::uninit(); !uninitialized)
    return uninitialized;
  m_movement_controller->uninit();
  return {};
}

std::expected<String, StarException> PlantDrop::description() const {
  return m_description;
}

Vec2F PlantDrop::position() const {
  return m_movement_controller->position();
}

RectF PlantDrop::meta_bound_box() const {
  return m_bounding_box;
}

RectF PlantDrop::collision_rect(float rotation) const {
  PolyF shape(m_collision_rect);
  shape.rotate(rotation);
  return shape.bounding_box();
}

std::optional<std::reference_wrapper<Json const>> PlantDrop::piece_config(Plant::PlantPieceKind kind) const {
  switch (kind) {
    case Plant::PlantPieceKind::Stem: return std::cref(m_stem_config);
    case Plant::PlantPieceKind::Foliage: return std::cref(m_foliage_config);
    default: return std::nullopt;
  }
}

std::expected<void, StarException> PlantDrop::deliver_items(World& world, ItemDelivery& delivery) {
  std::shared_ptr<ImageMetadataDatabase const> metadata;
  for (; delivery.piece_index < m_pieces.size(); ++delivery.piece_index) {
    auto const& piece = m_pieces[delivery.piece_index];
    if (!delivery.items) {
      if (!delivery.selection) {
        auto config = piece_config(piece.kind);
        if (!config)
          continue;
        auto drops = config->get().find("drops");
        if (!drops) return std::unexpected(std::move(drops.error()));
        if (!*drops || (*drops)->get().is_null())
          continue;
        auto options = (*drops)->get().array_ptr();
        if (!options) return std::unexpected(std::move(options.error()));
        if ((*options)->empty())
          continue;
        auto selected = root().random().sample_ref(**options);
        if (!selected) return std::unexpected(std::move(selected.error()));
        delivery.selection = selected->get();
      }
      auto items = delivery.selection->array_ptr();
      if (!items) return std::unexpected(std::move(items.error()));
      delivery.items = std::move(*items);
      delivery.selection.reset();
    }

    for (; delivery.item_index < delivery.items->size(); ++delivery.item_index) {
      if (!delivery.pending_item) {
        auto const& item = (*delivery.items)[delivery.item_index];
        auto name = item.get_string("item");
        if (!name) return std::unexpected(std::move(name.error()));
        auto count = item.get_int("count", 1);
        if (!count) return std::unexpected(std::move(count.error()));
        if (*count < 0)
          return std::unexpected(PlantException("Plant drop count must not be negative"));
        bool sapling = *name == "sapling";
        auto descriptor = ItemDescriptor::create(std::move(*name), static_cast<std::uint64_t>(*count), sapling ? m_sapling_config : Json());
        if (!descriptor) return std::unexpected(std::move(descriptor.error()));

        if (!delivery.center) {
          if (!metadata) {
            auto loaded = root().image_metadata_database();
            if (!loaded) return std::unexpected(std::move(loaded.error()));
            metadata = std::move(*loaded);
          }
          auto size = metadata->image_size(piece.image);
          if (!size) return std::unexpected(std::move(size.error()));
          delivery.center = (piece.offset + Vec2F(*size) * .5f / tile_pixels).rotate(delivery.rotation);
        }
        if (!delivery.position) {
          auto x = root().random().uniform_float(-0.2f, 0.2f);
          if (!x) return std::unexpected(std::move(x.error()));
          auto y = root().random().uniform_float(-0.2f, 0.2f);
          if (!y) return std::unexpected(std::move(y.error()));
          delivery.position = delivery.origin + (*delivery.center + Vec2F(*x, *y));
        }
        auto drop = ItemDrop::create_randomized_drop(root(), *descriptor, *delivery.position);
        if (!drop) return std::unexpected(std::move(drop.error()));
        delivery.pending_item = std::move(*drop);
      }
      if (delivery.pending_item) {
        if (auto published = world.addEntity(delivery.pending_item); !published)
          return published;
        delivery.pending_item.reset();
      }
      delivery.position.reset();
    }
    delivery.center.reset();
    delivery.items.reset();
    delivery.item_index = 0;
  }
  return {};
}

void PlantDrop::observe_replica_effects() {
  if (m_observed_live || m_hit_finished)
    return;
  if (m_spawned_drops.get()) {
    // The initial snapshot already acknowledges impact. Late joiners see
    // neither the original break nor the completed impact.
    m_hit_finished = true;
  } else {
    m_observed_live = true;
    m_break_pending = true;
  }
}

std::expected<void, StarException> PlantDrop::update(float dt, std::uint64_t) {
  auto& controller = *m_movement_controller;
  capturePresentationRotation(controller.rotation());
  m_time -= dt;
  controller.set_timestep(dt);
  if (!isMaster()) {
    if (auto advanced = m_net_group.tick_net_interpolation(dt); !advanced)
      return advanced;
    observe_replica_effects();
    return controller.tick_slave(dt);
  }

  auto owner = world();
  if (!owner) return std::unexpected(std::move(owner.error()));
  auto& world = owner->get();
  if (!m_item_delivery) {
    auto gravity = world.gravity(position());
    if (!gravity) return std::unexpected(std::move(gravity.error()));
    auto acceleration = 0.01f * *gravity * std::copysign(1.0f, m_rotation_rate) * dt;
    auto rotation = std::abs(controller.rotation());
    if (rotation > m_rotation_cap)
      m_rotation_rate -= acceleration;
    else if (rotation < m_rotation_fall_threshold)
      m_rotation_rate += acceleration;

    m_presentation_rotation_travel = m_rotation_rate * dt;
    controller.rotate(m_rotation_rate);
    if (m_time > 0) {
      bool gravity_enabled = std::fabs(controller.rotation()) >= m_rotation_fall_threshold;
      if (controller.parameters().gravity_enabled != gravity_enabled) {
        MovementParameters parameters;
        parameters.gravity_enabled = gravity_enabled;
        if (auto applied = controller.apply_parameters(parameters); !applied)
          return applied;
      }
      if (auto ticked = controller.tick_master(dt); !ticked)
        return ticked;
      if (controller.on_ground())
        m_time = 0;
    }
    if (m_spawned_drops.get())
      return {};
    if (m_time > 0) {
      // This is the post-movement position; its gravity may differ from the
      // pre-movement gravity used to accelerate the falling spin.
      auto gravity_at_impact = world.gravity(position());
      if (!gravity_at_impact) return std::unexpected(std::move(gravity_at_impact.error()));
      if (*gravity_at_impact != 0)
        return {};
    }
    ItemDelivery delivery{position(), controller.rotation()};
    if (auto delivered = deliver_items(world, delivery); !delivered) {
      m_item_delivery = std::make_unique<ItemDelivery>(std::move(delivery));
      return delivered;
    }
  } else {
    m_presentation_rotation_travel = 0.0f;
    if (auto delivered = deliver_items(world, *m_item_delivery); !delivered)
      return delivered;
    m_item_delivery.reset();
  }
  m_spawned_drops.set(true);
  return {};
}

std::expected<PlantDrop::ParticleBurst, StarException> PlantDrop::prepare_particles(PlantDropPiece const& piece, Json const& config, String const& mode) {
  ParticleBurst particles;
  auto particle_config = config.get("particles", JsonObject()).and_then([&](Json const& effects) { return effects.get(mode, JsonObject()); });
  if (!particle_config) return std::unexpected(std::move(particle_config.error()));
  auto options = particle_config->find("options");
  if (!options) return std::unexpected(std::move(options.error()));
  if (!*options || (*options)->get().is_null())
    return particles;
  auto array = (*options)->get().array_ptr();
  if (!array) return std::unexpected(std::move(array.error()));
  if ((*array)->empty())
    return particles;

  auto density = particle_config->get_float("density", 1);
  if (!density) return std::unexpected(std::move(density.error()));
  if (!is_finite(*density) || *density <= 0)
    return std::unexpected(PlantException("Plant drop particle density divisor must be positive and finite"));
  auto metadata = root().image_metadata_database();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  auto size = (*metadata)->image_size(piece.image);
  if (!size) return std::unexpected(std::move(size.error()));
  particles.image_size = Vec2F(*size) / tile_pixels;
  float count = (particles.image_size.x() * particles.image_size.y()) / *density;
  double rounded_count = std::ceil(static_cast<double>(count));
  if (!is_finite(count) || rounded_count > std::numeric_limits<std::uint32_t>::max())
    return std::unexpected(PlantException("Plant drop particle count is out of range"));
  if (rounded_count == 0)
    return particles;

  auto spaces = (*metadata)->image_spaces(piece.image, piece.offset * tile_pixels, Plant::plant_scan_threshold, piece.flip);
  if (!spaces) return std::unexpected(std::move(spaces.error()));
  if (spaces->empty())
    return particles;
  auto hue_shift = config.get_float("hueshift", 0);
  if (!hue_shift) return std::unexpected(std::move(hue_shift.error()));
  if (!is_finite(*hue_shift))
    return std::unexpected(PlantException("Plant drop particle hue shift must be finite"));
  particles.remaining = static_cast<std::uint32_t>(rounded_count);
  particles.options = std::move(*array);
  particles.spaces = Set<Vec2I>::from(std::move(*spaces));
  particles.hue_shift = *hue_shift / 360.0f;
  return particles;
}

std::expected<void, StarException> PlantDrop::submit_particles(RenderCallback& callback, RenderBurst const& burst,
    PlantDropPiece const& piece, ParticleBurst& particles, std::size_t& budget) {
  std::shared_ptr<ParticleDatabase const> database;
  while (particles.remaining != 0) {
    if (!particles.pending_particle) {
      if (!particles.sampled_position) {
        if (budget == 0)
          break;
        --budget;
        auto x = root().random().normal_float(particles.image_size.x() / 8.0f, 0);
        if (!x) return std::unexpected(std::move(x.error()));
        auto y = root().random().normal_float(particles.image_size.y() / 8.0f, 0);
        if (!y) return std::unexpected(std::move(y.error()));
        Vec2F position = piece.offset + particles.image_size / 2.0f + Vec2F(*x, *y);
        Vec2F tile = position.floor();
        auto representable = [](float coord) {
          return is_finite(coord) && static_cast<double>(coord) >= std::numeric_limits<int>::min()
              && static_cast<double>(coord) <= std::numeric_limits<int>::max();
        };
        if (!representable(tile.x()) || !representable(tile.y()) || !particles.spaces.contains(Vec2I(tile)))
          continue;
        particles.sampled_position = particle_world_position(position, burst.origin, burst.rotation);
      }
      if (!particles.sampled_config) {
        auto selected = root().random().sample_ref(*particles.options);
        if (!selected) return std::unexpected(std::move(selected.error()));
        particles.sampled_config = selected->get();
      }
      if (!database) {
        auto loaded = root().particle_database();
        if (!loaded) return std::unexpected(std::move(loaded.error()));
        database = std::move(*loaded);
      }
      auto particle = database->particle(*particles.sampled_config);
      if (!particle) return std::unexpected(std::move(particle.error()));
      particle->color.hue_shift(particles.hue_shift);
      for (Directives const& directives : piece.image.directives.list())
        particle->directives.append(directives);
      particle->position = *particles.sampled_position;
      particles.pending_particle = std::move(*particle);
    }
    if (auto added = callback.add_particle(std::move(*particles.pending_particle)); !added)
      return added;
    particles.pending_particle.reset();
    particles.sampled_position.reset();
    particles.sampled_config.reset();
    --particles.remaining;
  }
  return {};
}

std::expected<bool, StarException> PlantDrop::submit_render_burst(RenderCallback& callback, RenderBurst& burst,
    String const& mode, std::size_t& budget) {
  for (; burst.sound_index < 2; ++burst.sound_index) {
    if (!burst.pending_audio) {
      if (!burst.selected_sound) {
        auto const& config = burst.sound_index == 0 ? m_stem_config : m_foliage_config;
        auto sounds = config.get("sounds", JsonObject());
        if (!sounds) return std::unexpected(std::move(sounds.error()));
        auto options = sounds->find(mode);
        if (!options) return std::unexpected(std::move(options.error()));
        if (!*options || (*options)->get().is_null())
          continue;
        auto array = (*options)->get().array_ptr();
        if (!array) return std::unexpected(std::move(array.error()));
        if ((*array)->empty())
          continue;
        auto selected = root().random().sample_ref(**array);
        if (!selected) return std::unexpected(std::move(selected.error()));
        burst.selected_sound = selected->get();
      }
      auto file = burst.selected_sound->get_string("file");
      if (!file) return std::unexpected(std::move(file.error()));
      auto volume = burst.selected_sound->get_float("volume", 1.0f);
      if (!volume) return std::unexpected(std::move(volume.error()));
      auto assets = root().assets();
      if (!assets) return std::unexpected(std::move(assets.error()));
      auto audio = (*assets)->audio(*file);
      if (!audio) return std::unexpected(std::move(audio.error()));
      auto instance = AudioInstance::create(**audio);
      if (!instance) return std::unexpected(std::move(instance.error()));
      (*instance)->set_position(collision_rect(burst.rotation).center() + burst.origin);
      (*instance)->set_volume(*volume);
      burst.pending_audio = std::move(*instance);
    }
    if (auto added = callback.add_audio(burst.pending_audio); !added)
      return std::unexpected(std::move(added.error()));
    burst.pending_audio.reset();
    burst.selected_sound.reset();
  }

  for (; burst.piece_index < m_pieces.size(); ++burst.piece_index) {
    auto const& piece = m_pieces[burst.piece_index];
    if (!burst.particles) {
      auto config = piece_config(piece.kind);
      if (!config)
        continue;
      auto prepared = prepare_particles(piece, config->get(), mode);
      if (!prepared) return std::unexpected(std::move(prepared.error()));
      burst.particles = std::move(*prepared);
    }
    if (auto submitted = submit_particles(callback, burst, piece, *burst.particles, budget); !submitted)
      return std::unexpected(std::move(submitted.error()));
    if (burst.particles->remaining != 0)
      return false;
    burst.particles.reset();
  }
  return true;
}

std::uint64_t PlantDrop::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

bool PlantDrop::has_pending_render_events() const {
  return m_break_pending || (m_spawned_drops.get() && !m_hit_finished);
}

std::expected<void, StarException> PlantDrop::collect_render_events(RenderCallback& callback) {
  observe_replica_effects();
  if (m_spawned_drops.get() && !m_hit_finished && !m_impact_pose)
    m_impact_pose.emplace(position(), m_movement_controller->rotation());
  std::size_t budget = particle_sample_budget;
  auto submit = [&](Vec2F origin, float rotation, std::size_t sound_index, String const& mode) {
    if (m_render_burst) {
      auto completed = submit_render_burst(callback, *m_render_burst, mode, budget);
      if (completed && *completed)
        m_render_burst.reset();
      return completed;
    }
    RenderBurst burst{origin, rotation, sound_index};
    auto completed = submit_render_burst(callback, burst, mode, budget);
    if (!completed || !*completed)
      m_render_burst = std::make_unique<RenderBurst>(std::move(burst));
    return completed;
  };
  if (m_break_pending) {
    auto completed = submit(position(), m_movement_controller->rotation(), m_master ? 0u : 2u, "breakTree");
    if (!completed) return std::unexpected(std::move(completed.error()));
    if (!*completed)
      return {};
    m_break_pending = false;
  }
  if (m_impact_pose) {
    auto completed = submit(m_impact_pose->first, m_impact_pose->second, 0, "hitGround");
    if (!completed) return std::unexpected(std::move(completed.error()));
    if (*completed) {
      m_hit_finished = true;
      m_impact_pose.reset();
    }
  }
  return {};
}

std::expected<void, StarException> PlantDrop::render(RenderCallback& callback) {
  if (m_time <= 0 || m_spawned_drops.get() || m_pieces.empty())
    return {};
  float alpha = callback.presentation().interpolation();
  float current_rotation = m_movement_controller->rotation();
  float rotation = isMaster()
      ? current_rotation - m_presentation_rotation_travel * (1.0f - alpha)
      : samplePresentationRotation(current_rotation, alpha);
  Vec2F origin = position();
  auto base_transform = Mat3F::scaling(1.0f / tile_pixels);
  for (auto const& piece : m_pieces) {
    Drawable drawable;
    drawable.part = Drawable::ImagePart{piece.image, base_transform};
    drawable.position = piece.offset;
    if (piece.flip)
      drawable.scale(Vec2F(-1, 1));
    drawable.rotate(rotation);
    drawable.translate(origin);
    if (auto submitted = callback.add_drawable(std::move(drawable), render_layer_plant_drop); !submitted)
      return submitted;
  }
  return {};
}

MovementController& PlantDrop::movement_controller() {
  return *m_movement_controller;
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> PlantDrop::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(from_version, rules);
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> PlantDrop::snapshot_net_state(NetCompatibilityRules rules) const {
  return m_net_group.snapshot_net_state(rules);
}

std::expected<void, StarException> PlantDrop::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  if (auto read = m_net_group.read_net_state(data, interpolation_time, rules); !read)
    return read;
  observe_replica_effects();
  return {};
}

std::expected<void, StarException> PlantDrop::enable_interpolation(float extrapolation_hint) {
  return m_net_group.enable_net_interpolation(extrapolation_hint);
}

std::expected<void, StarException> PlantDrop::disable_interpolation() {
  return m_net_group.disable_net_interpolation();
}

std::expected<bool, StarException> PlantDrop::should_destroy() const {
  // Servers never collect presentation events; their destruction gate is
  // authoritative item publication, not local sound or particle delivery.
  return m_time <= 0.0f && m_spawned_drops.get();
}

}
