import star.json_extra;
import star.json;
import star.random;
import star.casting;
import star.game_types;
import star.vector;
import star.rect;
import star.bi_map;
import star.data_stream;
import star.list;
import star.set;
import star.color;
import star.string;
import star.directives;
import star.data_stream_extra;
import star.lua;
import star.logging;
import std;
import star.format;
import star.optional;
import star.algorithm;

import star.lua_root;

import star.lua_components;
import star.world_geometry;
import star.entity;
import star.world;
import star.movement_controller;

import star.game_timers;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_drop;
import star.assets;
import star.root;
import star.drawable;
import star.entity_rendering_types;
import star.light_source;
import star.entity_rendering;
import star.player;

import star.material_item;
import star.config_lua_bindings;
import star.entity_lua_bindings;
import star.item_lua_bindings;
import star.movement_controller_lua_bindings;
import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.line;
import star.math_common;
namespace Star {

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create_randomized_drop(Root& root_context, std::shared_ptr<Item> const& item, Vec2F const& position, bool eternal) {
  if (!item)
    return std::shared_ptr<ItemDrop>();
  auto drop = create(root_context, item);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  auto distance = (*drop)->m_config.get_float("randomizedDistance");
  if (!distance)
    return std::unexpected(std::move(distance.error()));
  auto offset = Vec2F(*distance, 0).rotate(std::numbers::pi * 2.0 * root_context.random().uniform_float());
  offset[1] = std::fabs(offset[1]);
  (*drop)->set_position(position + offset / tile_pixels);
  auto speed = (*drop)->m_config.get_float("randomizedSpeed");
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  (*drop)->set_velocity(offset * *speed);
  (*drop)->set_eternal(eternal);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create_randomized_drop(Root& root_context, ItemDescriptor const& descriptor, Vec2F const& position, bool eternal) {
  if (descriptor.is_empty())
    return std::shared_ptr<ItemDrop>();
  auto database = root_context.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  return create_randomized_drop(root_context, *item, position, eternal);
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::throw_drop(Root& root_context, std::shared_ptr<Item> const& item, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal) {
  if (!item)
    return std::shared_ptr<ItemDrop>();
  auto drop = create(root_context, item);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  (*drop)->set_position(position);
  if (direction != Vec2F()) {
    auto speed = (*drop)->m_config.get_float("throwSpeed");
    if (!speed)
      return std::unexpected(std::move(speed.error()));
    (*drop)->set_velocity(velocity + vnorm(direction) * *speed);
  }
  (*drop)->set_eternal(eternal);
  auto intangible_time = (*drop)->m_config.get_float("throwIntangibleTime");
  if (!intangible_time)
    return std::unexpected(std::move(intangible_time.error()));
  (*drop)->set_intangible_time(*intangible_time);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::throw_drop(Root& root_context, ItemDescriptor const& descriptor, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal) {
  if (descriptor.is_empty())
    return std::shared_ptr<ItemDrop>();
  auto database = root_context.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  return throw_drop(root_context, *item, position, velocity, direction, eternal);
}

std::expected<void, StarException> ItemDrop::initialize_item(std::shared_ptr<Item> item) {
  if (!item)
    return std::unexpected(ItemException("Cannot construct an item drop without an item"));
  auto parameters = item->instance_value_of_type("itemDrop", Json::Type::Object, JsonObject{});
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  m_item = std::move(item);
  m_parameters = std::move(*parameters);
  m_item_descriptor.set(m_item->descriptor());
  auto mode_name = config_value("clientEntityMode", "ClientSlaveOnly").and_then([](Json const& value) { return value.to_string(); });
  if (!mode_name)
    return std::unexpected(std::move(mode_name.error()));
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto mode = ClientEntityModeNames->get_left(*mode_name);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  m_client_entity_mode = mode->get();
  return {};
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& root_context, std::shared_ptr<Item> item) {
  auto drop = std::make_shared<ItemDrop>(ConstructionKey{}, root_context);
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto initialized = drop->initialize_item(std::move(item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  drop->m_owning_entity.set(null_entity_id);
  drop->m_mode.set(Mode::Available);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& root_context, Json const& store) {
  auto drop = std::make_shared<ItemDrop>(ConstructionKey{}, root_context);
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  auto database = root_context.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item_store = store.get("item");
  if (!item_store)
    return std::unexpected(std::move(item_store.error()));
  auto item = (*database)->disk_load(*item_store);
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (auto initialized = drop->initialize_item(std::move(*item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  auto position = store.get("position").and_then([](Json const& value) { return json_to_vec2f(value); });
  if (!position)
    return std::unexpected(std::move(position.error()));
  drop->m_movement_controller->setPosition(*position);
  auto mode_name = store.get_string("mode");
  if (!mode_name)
    return std::unexpected(std::move(mode_name.error()));
  if (!mode_names)
    return std::unexpected(mode_names.error());
  auto mode = mode_names->get_left(*mode_name);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  drop->m_mode.set(mode->get());
  auto eternal = store.get_bool("eternal");
  if (!eternal)
    return std::unexpected(std::move(eternal.error()));
  drop->m_eternal = *eternal;
  auto age = store.get("dropAge").and_then(EpochTimer::from_json);
  if (!age)
    return std::unexpected(std::move(age.error()));
  drop->m_drop_age = *age;
  auto aging = store.get("ageItemsTimer").and_then(EpochTimer::from_json);
  if (!aging)
    return std::unexpected(std::move(aging.error()));
  drop->m_age_items_timer = *aging;
  drop->m_owning_entity.set(null_entity_id);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& root_context, ByteArray store, NetCompatibilityRules rules) {
  auto drop = std::make_shared<ItemDrop>(ConstructionKey{}, root_context);
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  DataStreamBuffer ds(std::move(store));
  ds.set_stream_compatibility_version(rules);
  auto descriptor = ds.read<ItemDescriptor>();
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  auto database = root_context.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(*descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  // The master may hold an overfilled stack; a replica is not an inventory slot.
  if (*item)
    (*item)->set_count(descriptor->count(), true);
  if (auto initialized = drop->initialize_item(std::move(*item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto read = ds.read(drop->m_eternal); !read)
    return std::unexpected(std::move(read.error()));
  if (auto read = ds.read(drop->m_drop_age); !read)
    return std::unexpected(std::move(read.error()));
  if (auto read = ds.read(drop->m_intangible_timer); !read)
    return std::unexpected(std::move(read.error()));
  return drop;
}

std::expected<Json, StarException> ItemDrop::disk_store() const {
  auto database = root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto stored = (*database)->disk_store(m_item);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  if (!mode_names)
    return std::unexpected(mode_names.error());
  auto mode = mode_names->get_right(m_mode.get());
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  return Json(JsonObject{
    {"item", std::move(*stored)},
    {"position", json_from_vec2f(m_movement_controller->position())},
    {"mode", mode->get()},
    {"eternal", m_eternal},
    {"dropAge", m_drop_age.to_json()},
    {"ageItemsTimer", m_age_items_timer.to_json()}
  });
}

std::expected<ByteArray, StarException> ItemDrop::net_store(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  if (auto written = ds.write(item_safe_descriptor(m_item)); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_eternal); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_drop_age); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_intangible_timer); !written)
    return std::unexpected(std::move(written.error()));
  return ds.take_data();
}

EntityType ItemDrop::entity_type() const {
  return EntityType::ItemDrop;
}

std::expected<void, StarException> ItemDrop::init(World& world, EntityId entity_id, EntityMode mode) {
  auto initialized = Entity::init(world, entity_id, mode);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_movement_controller->init(world);
  auto setup = [&]() -> std::expected<void, StarException> {
    if (!isMaster())
      return {};
    auto scripts = config_value("scripts").and_then([](Json const& value) { return value.opt_array(); });
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (!*scripts)
      return {};
    auto script_paths = json_to_string_list(Json(**scripts));
    if (!script_paths)
      return std::unexpected(std::move(script_paths.error()));
    if (script_paths->empty())
      return {};
    if (auto set = m_script_component.set_scripts(std::move(*script_paths)); !set)
      return std::unexpected(std::move(set.error()));
    auto delta = config_value("scriptDelta", 1).and_then([](Json const& value) { return value.to_uint(); });
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_script_component.set_update_delta(*delta);
    auto add = [&](String name, std::expected<LuaCallbacks, StarException> callbacks) -> std::expected<void, StarException> {
      if (!callbacks)
        return std::unexpected(std::move(callbacks.error()));
      return m_script_component.add_callbacks(std::move(name), std::move(*callbacks));
    };
    if (auto added = add("itemDrop", make_item_drop_callbacks()); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("item", LuaBindings::make_item_slot_callbacks(std::cref(m_item))); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("config", LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) { return config_value(name, def); })); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("entity", LuaBindings::makeEntityCallbacks(*this)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("mcontroller", LuaBindings::make_movement_controller_callbacks(*m_movement_controller)); !added)
      return std::unexpected(std::move(added.error()));
    return recover_lua_script_error(m_script_component.init(world));
  }();
  if (!setup) {
    auto cleaned = uninit();
    if (!cleaned)
      return std::unexpected(cleaned.error().with_context(Star::print_exception(setup.error(), true)));
  }
  return setup;
}

ClientEntityMode ItemDrop::client_entity_mode() const {
  return m_client_entity_mode;
}

std::expected<void, StarException> ItemDrop::uninit() {
  std::optional<StarException> error;
  auto remember = [&]<typename T>(std::expected<T, StarException> result)
      requires (std::same_as<T, void> || std::same_as<T, bool>) {
    if (!result) {
      if (error)
        error = result.error().with_context(Star::print_exception(*error, true));
      else
        error = std::move(result.error());
    }
  };
  if (isMaster()) {
    remember(recover_lua_script_error(m_script_component.uninit()));
    remember(m_script_component.remove_callbacks("itemDrop"));
    remember(m_script_component.remove_callbacks("item"));
    remember(m_script_component.remove_callbacks("config"));
    remember(m_script_component.remove_callbacks("entity"));
    remember(m_script_component.remove_callbacks("mcontroller"));
  }
  m_movement_controller->uninit();
  remember(Entity::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<String, StarException> ItemDrop::name() const {
  if (m_item)
    return m_item->name();
  return Entity::name();
}

std::expected<String, StarException> ItemDrop::description() const {
  return m_item->description();
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> ItemDrop::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(from_version, rules);
}

std::expected<void, StarException> ItemDrop::read_net_state(ByteArray data, float interpolation_time, NetCompatibilityRules rules) {
  return m_net_group.read_net_state(std::move(data), interpolation_time, rules);
}

std::expected<void, StarException> ItemDrop::enable_interpolation(float extrapolation_hint) {
  if (auto enabled = m_net_group.enable_net_interpolation(extrapolation_hint); !enabled)
    return std::unexpected(std::move(enabled.error()));
  if (auto disabled = m_mode.disable_net_interpolation(); !disabled)
    return std::unexpected(std::move(disabled.error()));
  return m_owning_entity.disable_net_interpolation();
}

std::expected<void, StarException> ItemDrop::disable_interpolation() {
  return m_net_group.disable_net_interpolation();
}

Vec2F ItemDrop::position() const {
  return m_movement_controller->position();
}

RectF ItemDrop::meta_bound_box() const {
  return m_bound_box;
}

bool ItemDrop::ephemeral() const {
  return true;
}

RectF ItemDrop::collision_area() const {
  return m_bound_box;
}

std::expected<void, StarException> ItemDrop::update(float dt, std::uint64_t) {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto& world = current_world->get();
  m_drop_age.update(world.epochTime());
  if (isMaster()) {
    auto updated = recover_lua_script_error(m_script_component.update(m_script_component.update_dt(dt)));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (m_owning_entity.get() != null_entity_id) {
      if (auto taken = update_taken(true); !taken)
        return std::unexpected(std::move(taken.error()));
    } else {
      auto takeable = can_take();
      if (!takeable)
        return std::unexpected(std::move(takeable.error()));
      if (*takeable && root().random().uniform_float() < m_combine_chance && m_combine_radius > 0.0f) {
        auto const radius_squared = m_combine_radius * m_combine_radius;
        auto found = world.findEntity(
          RectF::with_center(position(), Vec2F::filled(2.0f * m_combine_radius)),
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
            auto close_drop_ref = as_ref<ItemDrop>(*entity);
            if (!close_drop_ref)
              return false;
            auto& close_drop = close_drop_ref->get();
            if (std::addressof(close_drop) == this || !close_drop.isMaster())
              return false;
            auto allowed = close_drop.can_take();
            if (!allowed)
              return std::unexpected(std::move(allowed.error()));
            // The source's script can change either drop before the transfer.
            if (!*allowed || !isMaster() || !pickup_available() || m_item == close_drop.m_item)
              return false;
            auto const receiver_position = position();
            auto const displacement = world.geometry().diff(close_drop.position(), receiver_position);
            if (!(displacement.magnitude_squared() < radius_squared))
              return false;
            auto& source = *close_drop.m_item;
            auto const count = source.count();
            if (m_item->could_stack(std::cref(source)) != count || !source.consume(count))
              return false;

            // Capacity was checked for the whole stack; no clone or partial pickup is needed.
            m_item->set_count(m_item->count() + count);
            close_drop.m_mode.set(Mode::Taken);
            m_item_descriptor.set(m_item->descriptor());
            m_drop_age.set_elapsed_time(std::min(m_drop_age.elapsed_time(), close_drop.m_drop_age.elapsed_time()));
            m_movement_controller->setPosition(receiver_position + displacement / 2.0f);
            m_movement_controller->setVelocity((m_movement_controller->velocity() + close_drop.velocity()) / 2.0f);
            return true;
          });
        if (!found)
          return std::unexpected(std::move(found.error()));
      }
      auto const& current = m_movement_controller->parameters();
      if (current.collisionEnabled != true || current.gravityEnabled != true) {
        MovementParameters parameters;
        parameters.collisionEnabled = true;
        parameters.gravityEnabled = true;
        if (auto applied = m_movement_controller->applyParameters(parameters); !applied)
          return std::unexpected(std::move(applied.error()));
      }
    }
    if (auto ticked = m_movement_controller->tickMaster(dt); !ticked)
      return std::unexpected(std::move(ticked.error()));
    m_intangible_timer.tick(dt);
    m_age_items_timer.update(world.epochTime());
    if ((m_mode.get() == Mode::Intangible || m_mode.get() == Mode::Available) && m_movement_controller->atWorldLimit())
      m_mode.set(Mode::Dead);
    if (m_mode.get() == Mode::Intangible && m_intangible_timer.ready())
      m_mode.set(Mode::Available);
    if (!m_eternal && m_mode.get() == Mode::Available && m_drop_age.elapsed_time() > m_item->time_to_live())
      m_mode.set(Mode::Dead);
    if (m_mode.get() == Mode::Taken && m_drop_age.elapsed_time() > m_after_taken_life)
      m_mode.set(Mode::Dead);
    if (m_override_mode)
      m_mode.set(*m_override_mode);
    if (m_client_entity_mode != ClientEntityMode::ClientSlaveOnly && m_mode.get() == Mode::Available)
      m_mode.set(Mode::Intangible);
    if (m_mode.get() <= Mode::Available && m_age_items_timer.elapsed_time() > m_age_items_every) {
      auto database = root().itemDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto replacement = m_item;
      auto aged = (*database)->age_item(replacement, m_age_items_timer.elapsed_time());
      if (!aged)
        return std::unexpected(std::move(aged.error()));
      if (*aged) {
        if (!replacement)
          return std::unexpected(ItemException("Item drop aging produced no item"));
        m_item_descriptor.set(replacement->descriptor());
        replace_item(std::move(replacement));
      }
      m_age_items_timer.set_elapsed_time(0.0);
    }
  } else {
    if (auto ticked = m_net_group.tick_net_interpolation(dt); !ticked)
      return std::unexpected(std::move(ticked.error()));
    // Pulling the field consumes its notification, not the pending load.
    m_item_descriptor_pending |= m_item_descriptor.pull_updated();
    if (m_item_descriptor_pending) {
      ItemDescriptor const& descriptor = m_item_descriptor.get();
      if (descriptor.is_null())
        return std::unexpected(ItemException("Item drop network state contains no item"));
      // Drops may contain authoritative overflow. The inventory loader would
      // clamp an exact match or a new item to its nominal stack limit.
      if (m_item->matches(descriptor, true)) {
        if (m_item->count() != descriptor.count())
          m_item->set_count(descriptor.count(), true);
      } else {
        auto database = root().itemDatabase();
        if (!database)
          return std::unexpected(std::move(database.error()));
        auto replacement = (*database)->item(descriptor);
        if (!replacement)
          return std::unexpected(std::move(replacement.error()));
        if (!*replacement)
          return std::unexpected(ItemException("Item drop network state contains no item"));
        (*replacement)->set_count(descriptor.count(), true);
        replace_item(std::move(*replacement));
      }
      m_item_descriptor_pending = false;
    }
    if (m_owning_entity.get() != null_entity_id) {
      if (m_drop_age.elapsed_time() > 1.0f) {
        m_owning_entity.set(null_entity_id);
      } else {
        if (auto taken = update_taken(false); !taken)
          return std::unexpected(std::move(taken.error()));
        if (auto ticked = m_movement_controller->tickMaster(dt); !ticked)
          return std::unexpected(std::move(ticked.error()));
      }
    } else if (auto ticked = m_movement_controller->tickSlave(dt); !ticked) {
      return std::unexpected(std::move(ticked.error()));
    }
  }
  if (world.isClient() && root().spatialLogger().observed()) {
    auto takeable = can_take();
    if (!takeable)
      return std::unexpected(std::move(takeable.error()));
    root().spatialLogger().log_poly("world", m_movement_controller->collisionBody(),
                                    (*takeable ? Color::palette_green : Color::palette_red).to_rgba());
  }
  return {};
}

void ItemDrop::replace_item(std::shared_ptr<Item> replacement) {
  if (m_item == replacement)
    return;
  m_drawables.reset();
  m_over_foreground = false;
  resetPresentation();
  m_item = std::move(replacement);
}

std::expected<bool, StarException> ItemDrop::should_destroy() const {
  return m_mode.get() == Mode::Dead || (m_item->empty() && m_owning_entity.get() == null_entity_id);
}

std::uint64_t ItemDrop::presentation_epoch() const {
  return Entity::presentation_epoch() + m_movement_controller->presentation_epoch();
}

std::expected<void, StarException> ItemDrop::render(RenderCallback& render_callback) {
  if (m_mode.get() != Mode::Taken && m_draw_rarity_beam) {
    Color beam_color;
    switch (m_item->rarity()) {
    case Rarity::Uncommon:
      beam_color = Color::rgb(87, 255, 81);
      break;
    case Rarity::Rare:
      beam_color = Color::rgb(87, 220, 255);
      break;
    case Rarity::Legendary:
      beam_color = Color::rgb(176, 81, 255);
      break;
    case Rarity::Essential:
      beam_color = Color::rgb(255, 255, 81);
      break;
    default:
      beam_color = Color::palette_white;
    }

    beam_color.set_alpha_f(0.8f);
    Line2F line = { Vec2F(), Vec2F(0.0f, 1.0f + m_bound_box.height() / 2) };
    float width = std::min(2.0f, m_bound_box.width() * tile_pixels);
    auto drawable = Drawable::make_line(line, width, beam_color, position());
    (drawable.line_part().end_color = beam_color)->set_alpha_f(0.0f);
    drawable.fullbright = true;
    if (auto added = render_callback.add_drawable(std::move(drawable), render_layer_item_drop); !added)
      return std::unexpected(std::move(added.error()));
  }

  if (!m_drawables) {
    auto mat = as_ref<MaterialItem>(*m_item);
    List<Drawable> generated;
    if (mat) {
      auto preview = mat->get().generatedPreview(render_callback.tile_drawer(), Vec2I(position().floor()));
      if (!preview)
        return std::unexpected(std::move(preview.error()));
      generated = preview->get();
    } else {
      auto drawables = m_item->drop_drawables(render_callback.tile_drawer());
      if (!drawables)
        return std::unexpected(std::move(drawables.error()));
      generated = std::move(*drawables);
    }
    auto directives = m_config.get_string("directives", "");
    if (!directives)
      return std::unexpected(std::move(directives.error()));
    if (Directives drop_directives = *directives) {
      auto metadata = root().image_metadata_database();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      for (auto& drawable : generated) {
        if (drawable.is_image()) {
          auto added = drawable.image_part().add_directives(**metadata, drop_directives, true);
          if (!added)
            return std::unexpected(std::move(added.error()));
        }
      }
    }
    m_drawables = std::move(generated);
    m_over_foreground = mat.has_value();
  }
  EntityRenderLayer render_layer = m_mode.get() == Mode::Taken || m_over_foreground ? render_layer_foreground_tile : render_layer_item_drop;
  Vec2F drop_position = position();
  for (Drawable drawable : *m_drawables) {
    drawable.position += drop_position;
    if (auto added = render_callback.add_drawable(std::move(drawable), render_layer); !added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> ItemDrop::render_light_sources(RenderCallback& render_callback) {
  LightSource light;
  light.type = LightType::Spread;
  light.color = Vec3F::filled(20.f / 255.f);
  light.position = position();
  return render_callback.add_light_source(std::move(light));
}

MovementController& ItemDrop::movement_controller() {
  return *m_movement_controller;
}

std::shared_ptr<Item> ItemDrop::item() const {
  return m_item;
}

void ItemDrop::set_eternal(bool eternal) {
  m_eternal = eternal;
}

void ItemDrop::set_intangible_time(float intangible_time) {
  m_intangible_timer = GameTimer(intangible_time);
  if (m_mode.get() == Mode::Available)
    m_mode.set(Mode::Intangible);
}

bool ItemDrop::pickup_available() const {
  return !m_checking_pickup && m_mode.get() == Mode::Available && m_owning_entity.get() == null_entity_id
      && m_item && !m_item->empty();
}

std::expected<bool, StarException> ItemDrop::can_take() const {
  if (!pickup_available())
    return false;
  if (!isMaster())
    return true;

  auto result = [&] {
    m_checking_pickup = true;
    auto restore = make_scope_exit([this] { m_checking_pickup = false; });
    return recover_lua_script_error(m_script_component.invoke<bool>("canTake"));
  }();
  if (!result)
    return std::unexpected(std::move(result.error()));
  return result->value_or(true) && isMaster() && pickup_available();
}

std::expected<std::shared_ptr<Item>, StarException> ItemDrop::take_by(EntityId entity_id, float time_offset) {
  auto taken = take();
  if (!taken || !*taken || !m_item->empty())
    return taken;
  m_owning_entity.set(entity_id);
  m_drop_age.set_elapsed_time(time_offset);
  setPersistent(false);
  return taken;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDrop::take() {
  auto takeable = can_take();
  if (!takeable)
    return std::unexpected(std::move(takeable.error()));
  if (!*takeable)
    return std::shared_ptr<Item>();
  auto taken = m_item->take();
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  if (!*taken || (*taken)->empty())
    return std::shared_ptr<Item>();
  if (m_item->empty())
    m_mode.set(Mode::Taken);
  else if (isMaster())
    m_item_descriptor.set(m_item->descriptor());
  return taken;
}

void ItemDrop::set_position(Vec2F const& position) {
  resetPresentation();
  m_movement_controller->setPosition(position);
}

Vec2F ItemDrop::velocity() const {
  return m_movement_controller->velocity();
}

void ItemDrop::set_velocity(Vec2F const& velocity) {
  m_movement_controller->setVelocity(velocity);
}

std::expected<EnumMap<ItemDrop::Mode>, StarException> const ItemDrop::mode_names = EnumMap<Mode>::create({
    {ItemDrop::Mode::Intangible, "Intangible"},
    {ItemDrop::Mode::Available, "Available"},
    {ItemDrop::Mode::Taken, "Taken"},
    {ItemDrop::Mode::Dead, "Dead"}});
    
std::expected<Json, StarException> ItemDrop::config_value(String const& name, Json const& def) const {
  auto config = m_config.query(name, def);
  if (!config)
    return std::unexpected(std::move(config.error()));
  return m_parameters.query(name, std::move(*config));
}

ItemDrop::ItemDrop(ConstructionKey, Root& root_context) : Entity(root_context), m_parameters(JsonObject{}), m_script_component(root_context) {
  setPersistent(true);
}

std::expected<void, StarException> ItemDrop::initialize() {
  auto controller = MovementController::create(root());
  if (!controller)
    return std::unexpected(std::move(controller.error()));
  m_movement_controller = std::move(*controller);
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/itemdrop.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  m_config = std::move(*config);
  auto parameters = m_config.get("movementSettings", JsonObject()).and_then([](Json const& value) { return MovementParameters::create(value); });
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  if (!parameters->physics_effect_categories)
    parameters->physics_effect_categories = StringSet({"itemdrop"});
  if (parameters->collisionPoly)
    m_bound_box = parameters->collisionPoly->bounding_box();
  else
    m_bound_box = RectF{-0.499, -0.499, 0.499, 0.499};
  if (auto applied = m_movement_controller->applyParameters(*parameters); !applied)
    return std::unexpected(std::move(applied.error()));
  if (auto added = m_net_group.add_net_element(m_mode); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_net_group.add_net_element(m_owning_entity); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_net_group.add_net_element(*m_movement_controller); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_net_group.add_net_element(m_item_descriptor); !added)
    return std::unexpected(std::move(added.error()));
  auto load_float = [&](String const& key, float& target) -> std::expected<void, StarException> {
    auto value = m_config.get_float(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target = *value;
    return {};
  };
  if (auto loaded = load_float("afterTakenLife", m_after_taken_life); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("overheadTime", m_overhead_time); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("pickupDistance", m_pickup_distance); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("velocity", m_velocity); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("velocityApproach", m_velocity_approach); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("overheadApproach", m_overhead_approach); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto overhead_distance = m_config.get_float("overheadRandomizedDistance");
  if (!overhead_distance)
    return std::unexpected(std::move(overhead_distance.error()));
  m_overhead_offset = Vec2F(*overhead_distance, 0).rotate(std::numbers::pi * 2.0 * root().random().uniform_float());
  if (auto loaded = load_float("combineChance", m_combine_chance); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = load_float("combineRadius", m_combine_radius); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto age_every = m_config.get_double("ageItemsEvery", 10);
  if (!age_every)
    return std::unexpected(std::move(age_every.error()));
  m_age_items_every = *age_every;
  auto rarity_beam = m_config.get_bool("drawRarityBeam", false);
  if (!rarity_beam)
    return std::unexpected(std::move(rarity_beam.error()));
  m_draw_rarity_beam = *rarity_beam;
  m_eternal = false;
  m_over_foreground = false;
  m_client_entity_mode = ClientEntityMode::ClientSlaveOnly;
  return {};
}


std::expected<void, StarException> ItemDrop::update_taken(bool master) {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto& world = current_world->get();
  if (auto owning_entity = world.entity(m_owning_entity.get())) {
    Vec2F position = m_movement_controller->position();
    bool overhead = m_drop_age.elapsed_time() < m_overhead_time;
    Vec2F target_position = owning_entity->position();
    if (overhead) {
      target_position += m_overhead_offset;
      auto rect = owning_entity->collision_area();
      if (!rect.is_null())
        target_position[1] += rect.y_max() + 1.5f;
      else
        target_position[1] += 1.5f;
    }
    Vec2F diff = world.geometry().diff(target_position, position);
    float magnitude = diff.magnitude();
    Vec2F velocity = diff.normalized() * m_velocity * std::min(1.0f, magnitude);
    if (auto player_entity = as_ref<Player>(*owning_entity))
      velocity += player_entity->get().velocity();
    m_movement_controller->approachVelocity(velocity, overhead ? m_overhead_approach : m_velocity_approach);
    if (master && !overhead && magnitude < m_pickup_distance)
      m_mode.set(Mode::Dead);

  } else if (master) {
    // Our owning entity left, disappear quickly
    m_mode.set(Mode::Dead);
  }

  auto const& current = m_movement_controller->parameters();
  if (current.maxMovementPerStep == 1000.0f && current.collisionEnabled == false && current.gravityEnabled == false)
    return {};
  MovementParameters parameters;
  parameters.maxMovementPerStep = 1000.0f;
  parameters.collisionEnabled = false;
  parameters.gravityEnabled = false;
  return m_movement_controller->applyParameters(parameters);
}

std::expected<std::optional<LuaValue>, StarException> ItemDrop::call_script(String const& func, LuaVariadic<LuaValue> const& args) {
  return recover_lua_script_error(m_script_component.invoke(func, args));
}

std::expected<std::optional<LuaValue>, StarException> ItemDrop::eval_script(String const& code) {
  return recover_lua_script_error(m_script_component.eval(code));
}

std::expected<LuaCallbacks, StarException> ItemDrop::make_item_drop_callbacks() {
  LuaCallbacks callbacks;
  if (auto registered = callbacks.register_callback("takingEntity", [this]() -> std::optional<EntityId> {
    if (m_owning_entity.get() == null_entity_id)
      return {};
    return m_owning_entity.get();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("setEternal", [this](bool eternal) { set_eternal(eternal); }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("eternal", [this]() { return m_eternal; }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("setIntangibleTime", [this](float time) { set_intangible_time(time); }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("intangibleTime", [this]() { return m_intangible_timer.remaining; }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("setOverrideMode", [this](std::optional<String> const& mode) -> std::expected<void, StarException> {
    if (!mode) {
      m_override_mode.reset();
      return {};
    }
    if (!mode_names)
      return std::unexpected(mode_names.error());
    auto value = mode_names->get_left(*mode);
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_override_mode = value->get();
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.register_callback("overrideMode", [this]() -> std::expected<std::optional<String>, StarException> {
    if (!m_override_mode)
      return std::optional<String>();
    if (!mode_names)
      return std::unexpected(mode_names.error());
    auto value = mode_names->get_right(*m_override_mode);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return std::optional<String>(value->get());
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  return callbacks;
}

}
