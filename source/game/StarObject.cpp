import star.output_proxy;
import star.memory;
import star.json_extra;
import star.json;
import star.id_map;
import star.poly;
import star.list;
import star.bi_map;
import star.data_stream_extra;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.lexical_cast;
import star.logging;
import star.game_types;
import star.message_types;
import star.random;
import star.weighted_pool;
import star.interpolation;
import star.parametric_function;
import star.data_stream;
import star.audio;
import star.thread;
import star.map;
import star.set;
import star.vector;
import star.color;
import star.string;
import star.rect;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.math_common;
import star.periodic;
import star.periodic_function;
import star.lua;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.listener;
import std;
import star.format;

import star.collision_block;
import star.lua_components;
import star.lua_animation_component;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.animated_part_set;
import star.networked_animator;
import star.object;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
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
import star.game_timers;
import star.item_drop;
import star.item_descriptor;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.lua_game_converters;
import star.treasure;

import star.networked_animator_lua_bindings;
import star.entity_lua_bindings;
import star.scripted_animator_lua_bindings;
import star.config_lua_bindings;
import star.particle_database;
import star.object_database;
import star.material_database;



import star.data_stream_devices;

import star.net_compatibility;
import star.byte_array;
import star.algorithm;
namespace Star {

struct Object::RenderEventCollection {
  enum class Stage { Start, Particles, Sound, AnimatorAudio, AnimatorParticles, ScriptedParticles, ScriptedAudio };

  using Audio = std::shared_ptr<AudioInstance>;
  struct Emission {
    Particle particle;
    std::optional<GameTimer> cooldown;
  };

  Stage stage = Stage::Start;
  std::size_t next_item = 0;
  std::size_t emission_timer_revision = 0;
  std::variant<std::monostate, Emission, Audio, List<Particle>, List<Audio>> pending;

  template <typename Pull>
    requires std::invocable<Pull&> && (std::same_as<std::invoke_result_t<Pull&>, List<Particle>> ||
                                       std::same_as<std::invoke_result_t<Pull&>, List<Audio>>)
  std::expected<void, StarException> submit_batch(RenderCallback& sink, Pull pull) {
    using Batch = std::invoke_result_t<Pull&>;
    if (!std::holds_alternative<Batch>(pending)) {
      auto batch = pull();
      if (batch.empty())
        return {};
      pending.emplace<Batch>(std::move(batch));
    }
    auto& batch = std::get<Batch>(pending);
    for (; next_item < batch.size(); ++next_item) {
      auto& item = batch[next_item];
      if constexpr (std::same_as<Batch, List<Particle>>) {
        if (auto submitted = sink.add_particle(std::move(item)); !submitted)
          return submitted;
      } else {
        if (auto submitted = sink.add_audio(item); !submitted)
          return submitted;
        item.reset();
      }
    }
    pending.emplace<std::monostate>();
    next_item = 0;
    return {};
  }
};

Object::Object(Root& root_context) : Entity(root_context), m_script_component(root_context), m_scripted_animator(root_context), m_animation_timer(0), m_current_frame(-1), m_broken(false), m_unbreakable(false), m_orientation_index(npos), m_animation_center_line(0), m_client_entity_mode(ClientEntityMode::ClientSlaveOnly) {}

Object::Object(ConstructionKey, Root& root) : Object(root) {}

Object::~Object() = default;

std::expected<std::shared_ptr<Object>, StarException> Object::create(Root& root_context, std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  auto object = std::make_shared<Object>(ConstructionKey{}, root_context);
  if (auto initialized = object->initialize(std::move(config), parameters); !initialized)
    return std::unexpected(std::move(initialized.error()));
  return object;
}

std::expected<void, StarException> Object::initialize(std::shared_ptr<ObjectConfig const> config, Json const& parameters) {
  m_config = std::move(config);
  if (!parameters.is_null()) {
    auto object_parameters_result = parameters.to_object();
    if (!object_parameters_result)
      return std::unexpected(std::move(object_parameters_result.error()));
    auto object_parameters = std::move(*object_parameters_result);
    m_parameters.reset(std::move(object_parameters));
  }
  auto custom = ObjectDatabase::parse_custom_orientations(root(), *m_config, parameters, true);
  if (!custom)
    return std::unexpected(std::move(custom.error()));
  m_orientations = std::move(*custom);
  m_light_flickering = m_config->light_flickering;
  auto tile_damage_status = EntityTileDamageStatus::create();
  if (!tile_damage_status)
    return std::unexpected(std::move(tile_damage_status.error()));
  m_tile_damage_status = std::move(*tile_damage_status);
  auto action_result = config_value("interactAction", Json());
  if (!action_result)
    return std::unexpected(std::move(action_result.error()));
  auto action = std::move(*action_result);
  m_interactive.set(!action.is_null());
  m_unbreakable = m_config->unbreakable;
  if (!m_unbreakable) {
    auto unbreakable_config_result = config_value("unbreakable", false);
    if (!unbreakable_config_result)
      return std::unexpected(std::move(unbreakable_config_result.error()));
    auto unbreakable_config = std::move(*unbreakable_config_result);
    auto unbreakable_result = unbreakable_config.to_bool();
    if (!unbreakable_result)
      return std::unexpected(std::move(unbreakable_result.error()));
    auto unbreakable = std::move(*unbreakable_result);
    m_unbreakable = unbreakable;
  }
  m_direction.set(Direction::Left);
  m_health.set(m_config->health);
  if (m_config->animation_config) {
    auto animator_result = NetworkedAnimator::create(root(), m_config->animation_config, m_config->path);
    if (!animator_result)
      return std::unexpected(std::move(animator_result.error()));
    auto animator = std::move(*animator_result);
    m_networked_animator = std::move(animator);
  } else {
    auto animator = NetworkedAnimator::create(root());
    if (!animator)
      return std::unexpected(std::move(animator.error()));
    m_networked_animator = std::move(*animator);
  }
  if (m_config->damage_team.type != TeamType::Null)
    setTeam(m_config->damage_team);
  else
    setTeam(EntityDamageTeam(TeamType::Environment));
  auto initialize_nodes = [&](WireDirection direction, String const& positions_key, String const& config_key)
      -> std::expected<void, StarException> {
    auto positions = config_value(positions_key, JsonArray());
    if (!positions)
      return std::unexpected(std::move(positions.error()));
    auto configs = config_value(config_key, JsonArray());
    if (!configs)
      return std::unexpected(std::move(configs.error()));
    auto count = positions->size();
    if (!count)
      return std::unexpected(std::move(count.error()));
    auto& entries = nodes(direction);
    entries.reserve(*count);
    for (std::size_t i = 0; i < *count; ++i) {
      auto position = positions->get(i);
      if (!position)
        return std::unexpected(std::move(position.error()));
      auto config = configs->get(i, JsonObject());
      if (!config)
        return std::unexpected(std::move(config.error()));
      auto entry = Node::create(*position, *config, direction);
      if (!entry)
        return std::unexpected(std::move(entry.error()));
      entries.push_back(std::move(*entry));
    }
    return {};
  };
  if (auto input = initialize_nodes(WireDirection::Input, "inputNodes", "inputNodesConfig"); !input)
    return std::unexpected(std::move(input.error()));
  if (auto output = initialize_nodes(WireDirection::Output, "outputNodes", "outputNodesConfig"); !output)
    return std::unexpected(std::move(output.error()));
  auto offered_config_result = config_value("offeredQuests", JsonArray());
  if (!offered_config_result)
    return std::unexpected(std::move(offered_config_result.error()));
  auto offered_config = std::move(*offered_config_result);
  auto offered_result = offered_config.to_array();
  if (!offered_result)
    return std::unexpected(std::move(offered_result.error()));
  auto offered = std::move(*offered_result);
  List<QuestArcDescriptor> quests;
  for (auto const& entry : offered) {
    auto quest_result = QuestArcDescriptor::fromJson(root(), entry);
    if (!quest_result)
      return std::unexpected(std::move(quest_result.error()));
    auto quest = std::move(*quest_result);
    quests.push_back(std::move(quest));
  }
  m_offered_quests.set(std::move(quests));
  auto turn_in_config_result = config_value("turnInQuests", JsonArray());
  if (!turn_in_config_result)
    return std::unexpected(std::move(turn_in_config_result.error()));
  auto turn_in_config = std::move(*turn_in_config_result);
  auto turn_in_result = json_to_string_set(turn_in_config);
  if (!turn_in_result)
    return std::unexpected(std::move(turn_in_result.error()));
  auto turn_in = std::move(*turn_in_result);
  m_turn_in_quests.set(std::move(turn_in));
  if (!m_offered_quests.get().empty() || !m_turn_in_quests.get().empty())
    m_interactive.set(true);
  auto color_config_result = config_value("color", "default");
  if (!color_config_result)
    return std::unexpected(std::move(color_config_result.error()));
  auto color_config = std::move(*color_config_result);
  auto color_result = color_config.to_string();
  if (!color_result)
    return std::unexpected(std::move(color_result.error()));
  auto color = std::move(*color_result);
  m_image_keys.set("color", color.take_utf8());
  auto keys_config_result = config_value("imageKeys", JsonObject());
  if (!keys_config_result)
    return std::unexpected(std::move(keys_config_result.error()));
  auto keys_config = std::move(*keys_config_result);
  auto keys_result = keys_config.to_object();
  if (!keys_result)
    return std::unexpected(std::move(keys_result.error()));
  auto keys = std::move(*keys_result);
  for (auto const& pair : keys) {
    auto value_result = pair.second.to_string();
    if (!value_result)
      return std::unexpected(std::move(value_result.error()));
    auto value = std::move(*value_result);
    m_image_keys.set(pair.first, value);
  }
  auto unique_id_config_result = config_value("uniqueId");
  if (!unique_id_config_result)
    return std::unexpected(std::move(unique_id_config_result.error()));
  auto unique_id_config = std::move(*unique_id_config_result);
  auto unique_id_result = unique_id_config.opt_string();
  if (!unique_id_result)
    return std::unexpected(std::move(unique_id_result.error()));
  auto unique_id = std::move(*unique_id_result);
  set_unique_id(std::move(unique_id));
  auto operation2 = m_net_group.add_net_element(m_parameters);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = m_net_group.add_net_element(m_unique_id_net_state);
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = m_net_group.add_net_element(m_interactive);
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation5 = m_net_group.add_net_element(m_material_spaces);
  if (!operation5)
    return std::unexpected(std::move(operation5.error()));
  auto operation6 = m_net_group.add_net_element(m_x_tile_position);
  if (!operation6)
    return std::unexpected(std::move(operation6.error()));
  auto operation7 = m_net_group.add_net_element(m_y_tile_position);
  if (!operation7)
    return std::unexpected(std::move(operation7.error()));
  auto operation8 = m_net_group.add_net_element(m_direction);
  if (!operation8)
    return std::unexpected(std::move(operation8.error()));
  auto operation9 = m_net_group.add_net_element(m_health);
  if (!operation9)
    return std::unexpected(std::move(operation9.error()));
  auto operation10 = m_net_group.add_net_element(m_orientation_index_net_state);
  if (!operation10)
    return std::unexpected(std::move(operation10.error()));
  auto operation11 = m_net_group.add_net_element(m_net_image_keys);
  if (!operation11)
    return std::unexpected(std::move(operation11.error()));
  auto operation12 = m_net_group.add_net_element(m_sound_effect_enabled);
  if (!operation12)
    return std::unexpected(std::move(operation12.error()));
  auto operation13 = m_net_group.add_net_element(m_light_source_color);
  if (!operation13)
    return std::unexpected(std::move(operation13.error()));
  auto operation14 = m_net_group.add_net_element(m_new_chat_message_event);
  if (!operation14)
    return std::unexpected(std::move(operation14.error()));
  auto operation15 = m_net_group.add_net_element(m_chat_message);
  if (!operation15)
    return std::unexpected(std::move(operation15.error()));
  auto operation16 = m_net_group.add_net_element(m_chat_portrait);
  if (!operation16)
    return std::unexpected(std::move(operation16.error()));
  auto operation17 = m_net_group.add_net_element(m_chat_config);
  if (!operation17)
    return std::unexpected(std::move(operation17.error()));
  for (auto& node : m_input_nodes) {
    auto operation18 = m_net_group.add_net_element(node.connections);
    if (!operation18)
      return std::unexpected(std::move(operation18.error()));
    auto operation19 = m_net_group.add_net_element(node.state);
    if (!operation19)
      return std::unexpected(std::move(operation19.error()));
  }
  for (auto& node : m_output_nodes) {
    auto operation20 = m_net_group.add_net_element(node.connections);
    if (!operation20)
      return std::unexpected(std::move(operation20.error()));
    auto operation21 = m_net_group.add_net_element(node.state);
    if (!operation21)
      return std::unexpected(std::move(operation21.error()));
  }
  auto operation22 = m_net_group.add_net_element(m_offered_quests);
  if (!operation22)
    return std::unexpected(std::move(operation22.error()));
  auto operation23 = m_net_group.add_net_element(m_turn_in_quests);
  if (!operation23)
    return std::unexpected(std::move(operation23.error()));
  auto operation24 = m_net_group.add_net_element(m_damage_sources);
  if (!operation24)
    return std::unexpected(std::move(operation24.error()));
  // Scripted animation parameters deliberately do not interpolate.
  auto operation25 = m_net_group.add_net_element(m_scripted_animation_parameters, false);
  if (!operation25)
    return std::unexpected(std::move(operation25.error()));
  auto operation26 = m_net_group.add_net_element(*m_tile_damage_status);
  if (!operation26)
    return std::unexpected(std::move(operation26.error()));
  auto operation27 = m_net_group.add_net_element(*m_networked_animator);
  if (!operation27)
    return std::unexpected(std::move(operation27.error()));
  m_net_group.set_needs_load_callback([this](bool initial) { return get_net_states(initial); });
  m_net_group.set_needs_store_callback([this]() { return set_net_states(); });
  auto mode_config_result = config_value("clientEntityMode", "ClientSlaveOnly");
  if (!mode_config_result)
    return std::unexpected(std::move(mode_config_result.error()));
  auto mode_config = std::move(*mode_config_result);
  auto mode_name_result = mode_config.to_string();
  if (!mode_name_result)
    return std::unexpected(std::move(mode_name_result.error()));
  auto mode_name = std::move(*mode_name_result);
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto mode_result = ClientEntityModeNames->get_left(mode_name);
  if (!mode_result)
    return std::unexpected(std::move(mode_result.error()));
  auto mode = std::move(*mode_result);
  m_client_entity_mode = mode.get();
  return {};
}

std::expected<Json, StarException> Object::disk_store() const {
  auto stored_result = write_stored_data();
  if (!stored_result)
    return std::unexpected(std::move(stored_result.error()));
  auto stored = std::move(*stored_result);
  return stored.set_all({{"name", m_config->name}, {"parameters", m_parameters.base_map()}});
}

std::expected<ByteArray, StarException> Object::net_store(NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  auto operation28 = ds.write(m_config->name);
  if (!operation28)
    return std::unexpected(std::move(operation28.error()));
  auto operation29 = ds.write<Json>(m_parameters.base_map());
  if (!operation29)
    return std::unexpected(std::move(operation29.error()));
  return ds.take_data();
}

EntityType Object::entity_type() const {
  return EntityType::Object;
}

ClientEntityMode Object::client_entity_mode() const {
  return m_client_entity_mode;
}

std::expected<void, StarException> Object::init(World& world, EntityId entity_id, EntityMode mode) {
  auto operation1 = Entity::init(world, entity_id, mode);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto initialized = [&]() -> std::expected<void, StarException> {
    if (m_orientation_index == npos) {
      auto operation2 = update_orientation();
      if (!operation2)
        return std::unexpected(std::move(operation2.error()));
    } else if (auto orientation = current_orientation()) {
      if (orientation->direction_affinity)
        m_direction.set(*orientation->direction_affinity);
      m_material_spaces.set(orientation->material_spaces);
    }
    m_orientation_drawables_cache.reset();
    // Preserve the legacy color/directive split, including its byte offsets.
    auto color_config_result = config_value("color", "default");
    if (!color_config_result)
      return std::unexpected(std::move(color_config_result.error()));
    auto color_config = std::move(*color_config_result);
    auto color_result = color_config.to_string();
    if (!color_result)
      return std::unexpected(std::move(color_result.error()));
    auto color = std::move(*color_result);
    auto color_name = color.take_utf8();
    auto color_end = color_name.find('?');
    if (color_end != npos) {
      std::size_t suffix_begin = color_name.rfind('?');
      String color_directives;
      std::string color_suffix = suffix_begin == npos ? "" : color_name.substr(suffix_begin);
      if (color_suffix.empty() && color_suffix.rfind("?replace", 0) != 0)
        color_directives = color_name.substr(color_end);
      else
        color_directives = color_name.substr(color_end, suffix_begin - color_end);
      m_color_suffix = std::move(color_suffix);
      m_color_directives = std::move(color_directives);
    } else {
      m_color_directives = "";
      m_color_suffix = "";
    }
    m_directives = "";
    auto directives_result = config_value("");
    if (!directives_result)
      return std::unexpected(std::move(directives_result.error()));
    auto directives = std::move(*directives_result);
    if (directives.is_type(Json::Type::String)) {
      auto directive_string_result = directives.to_string();
      if (!directive_string_result)
        return std::unexpected(std::move(directive_string_result.error()));
      auto directive_string = std::move(*directive_string_result);
      m_directives = Directives(std::move(directive_string));
    }
    if (isMaster()) {
      set_image_key("color", color_name);
      auto keys_config_result = config_value("imageKeys", JsonObject());
      if (!keys_config_result)
        return std::unexpected(std::move(keys_config_result.error()));
      auto keys_config = std::move(*keys_config_result);
      auto keys_result = keys_config.to_object();
      if (!keys_result)
        return std::unexpected(std::move(keys_result.error()));
      auto keys = std::move(*keys_result);
      for (auto const& pair : keys) {
        auto value_result = pair.second.to_string();
        if (!value_result)
          return std::unexpected(std::move(value_result.error()));
        auto value = std::move(*value_result);
        set_image_key(pair.first, value);
      }
      if (auto light = m_config->light_colors.find_ref(color_name)) {
        m_light_source_color.set(light->get());
      } else {
        m_light_source_color.set(Color::palette_clear);
      }
      m_sound_effect_enabled.set(true);
      m_liquid_check_timer = GameTimer(m_config->liquid_check_interval);
      m_liquid_check_timer.set_done();
      auto keep_alive_config_result = config_value("keepAlive", false);
      if (!keep_alive_config_result)
        return std::unexpected(std::move(keep_alive_config_result.error()));
      auto keep_alive_config = std::move(*keep_alive_config_result);
      auto keep_alive_result = keep_alive_config.to_bool();
      if (!keep_alive_result)
        return std::unexpected(std::move(keep_alive_result.error()));
      auto keep_alive = std::move(*keep_alive_result);
      setKeepAlive(keep_alive);
      auto scripts_config_result = config_value("scripts", JsonArray());
      if (!scripts_config_result)
        return std::unexpected(std::move(scripts_config_result.error()));
      auto scripts_config = std::move(*scripts_config_result);
      if (scripts_config.is_type(Json::Type::Array)) {
        auto scripts_result = json_to_string_list(scripts_config);
        if (!scripts_result)
          return std::unexpected(std::move(scripts_result.error()));
        auto scripts = std::move(*scripts_result);
        std::ranges::for_each(scripts,
                              [&](String& script) { script = AssetPath::relative_to(m_config->path, script); });
        auto operation3 = m_script_component.set_scripts(std::move(scripts));
        if (!operation3)
          return std::unexpected(std::move(operation3.error()));
      } else {
        auto operation4 = m_script_component.set_scripts(m_config->scripts);
        if (!operation4)
          return std::unexpected(std::move(operation4.error()));
      }
      auto delta_config_result = config_value("scriptDelta", 5);
      if (!delta_config_result)
        return std::unexpected(std::move(delta_config_result.error()));
      auto delta_config = std::move(*delta_config_result);
      auto delta_result = delta_config.to_int();
      if (!delta_result)
        return std::unexpected(std::move(delta_result.error()));
      auto delta = std::move(*delta_result);
      m_script_component.set_update_delta(delta);
      auto object_callbacks_result = make_object_callbacks();
      if (!object_callbacks_result)
        return std::unexpected(std::move(object_callbacks_result.error()));
      auto object_callbacks = std::move(*object_callbacks_result);
      auto operation5 = m_script_component.add_callbacks("object", std::move(object_callbacks));
      if (!operation5)
        return std::unexpected(std::move(operation5.error()));
      auto config_callbacks_result = LuaBindings::makeConfigCallbacks(
        [this](String const& name, Json const& def) { return config_value(name, def); });
      if (!config_callbacks_result)
        return std::unexpected(std::move(config_callbacks_result.error()));
      auto config_callbacks = std::move(*config_callbacks_result);
      auto operation6 = m_script_component.add_callbacks("config", std::move(config_callbacks));
      if (!operation6)
        return std::unexpected(std::move(operation6.error()));
      auto entity_callbacks_result = LuaBindings::makeEntityCallbacks(*this);
      if (!entity_callbacks_result)
        return std::unexpected(std::move(entity_callbacks_result.error()));
      auto entity_callbacks = std::move(*entity_callbacks_result);
      auto operation7 = m_script_component.add_callbacks("entity", std::move(entity_callbacks));
      if (!operation7)
        return std::unexpected(std::move(operation7.error()));
      auto animator_callbacks_result = LuaBindings::make_networked_animator_callbacks(*m_networked_animator);
      if (!animator_callbacks_result)
        return std::unexpected(std::move(animator_callbacks_result.error()));
      auto animator_callbacks = std::move(*animator_callbacks_result);
      auto operation8 = m_script_component.add_callbacks("animator", std::move(animator_callbacks));
      if (!operation8)
        return std::unexpected(std::move(operation8.error()));
      auto operation9 = recover_lua_script_error(m_script_component.init(world));
      if (!operation9)
        return std::unexpected(std::move(operation9.error()));
    }
    if (world.isClient()) {
      auto operation10 = m_scripted_animator.set_scripts(m_config->animation_scripts);
      if (!operation10)
        return std::unexpected(std::move(operation10.error()));
      auto animation_callbacks_result = LuaBindings::make_scripted_animator_callbacks(*m_networked_animator, [this](String const& name, Json const& def) -> std::expected<Json, StarException> { return m_scripted_animation_parameters.value(name, def); });
      if (!animation_callbacks_result)
        return std::unexpected(std::move(animation_callbacks_result.error()));
      auto animation_callbacks = std::move(*animation_callbacks_result);
      auto operation11 = m_scripted_animator.add_callbacks("animationConfig", std::move(animation_callbacks));
      if (!operation11)
        return std::unexpected(std::move(operation11.error()));
      auto object_animator_callbacks_result = make_animator_object_callbacks();
      if (!object_animator_callbacks_result)
        return std::unexpected(std::move(object_animator_callbacks_result.error()));
      auto object_animator_callbacks = std::move(*object_animator_callbacks_result);
      auto operation12 = m_scripted_animator.add_callbacks("objectAnimator", std::move(object_animator_callbacks));
      if (!operation12)
        return std::unexpected(std::move(operation12.error()));
      auto config_callbacks_result = LuaBindings::makeConfigCallbacks(
        [this](String const& name, Json const& def) { return config_value(name, def); });
      if (!config_callbacks_result)
        return std::unexpected(std::move(config_callbacks_result.error()));
      auto config_callbacks = std::move(*config_callbacks_result);
      auto operation13 = m_scripted_animator.add_callbacks("config", std::move(config_callbacks));
      if (!operation13)
        return std::unexpected(std::move(operation13.error()));
      auto entity_callbacks_result = LuaBindings::makeEntityCallbacks(*this);
      if (!entity_callbacks_result)
        return std::unexpected(std::move(entity_callbacks_result.error()));
      auto entity_callbacks = std::move(*entity_callbacks_result);
      auto operation14 = m_scripted_animator.add_callbacks("entity", std::move(entity_callbacks));
      if (!operation14)
        return std::unexpected(std::move(operation14.error()));
      auto operation15 = recover_lua_script_error(m_scripted_animator.init(world));
      if (!operation15)
        return std::unexpected(std::move(operation15.error()));
    }
    m_x_tile_position.set(world.geometry().xwrap(static_cast<int>(m_x_tile_position.get())));
    auto parts_config_result = config_value("animationParts", JsonObject());
    if (!parts_config_result)
      return std::unexpected(std::move(parts_config_result.error()));
    auto parts_config = std::move(*parts_config_result);
    auto parts_result = parts_config.to_object();
    if (!parts_result)
      return std::unexpected(std::move(parts_result.error()));
    auto parts = std::move(*parts_result);
    for (auto const& pair : parts) {
      auto image_result = pair.second.to_string();
      if (!image_result)
        return std::unexpected(std::move(image_result.error()));
      auto image = std::move(*image_result);
      m_networked_animator->set_part_tag(pair.first, "partImage", image);
    }
    auto animation_position_config_result = config_value("animationPosition", JsonArray{0, 0});
    if (!animation_position_config_result)
      return std::unexpected(std::move(animation_position_config_result.error()));
    auto animation_position_config = std::move(*animation_position_config_result);
    auto animation_position_result = json_to_vec2f(animation_position_config);
    if (!animation_position_result)
      return std::unexpected(std::move(animation_position_result.error()));
    auto animation_position = std::move(*animation_position_result);
    m_animation_position = animation_position / tile_pixels;
    m_networked_animator->set_flipped(false);
    auto image_metadata_result = root().image_metadata_database();
    if (!image_metadata_result)
      return std::unexpected(std::move(image_metadata_result.error()));
    auto image_metadata = std::move(*image_metadata_result);
    auto drawables_result = m_networked_animator->drawables();
    if (!drawables_result)
      return std::unexpected(std::move(drawables_result.error()));
    auto drawables = std::move(*drawables_result);
    auto bounds_result = Drawable::bound_box_all(*image_metadata, drawables, false);
    if (!bounds_result)
      return std::unexpected(std::move(bounds_result.error()));
    auto bounds = std::move(*bounds_result);
    auto center_config_result = config_value("animationCenterLine", bounds.center()[0]);
    if (!center_config_result)
      return std::unexpected(std::move(center_config_result.error()));
    auto center_config = std::move(*center_config_result);
    auto center_result = center_config.to_float();
    if (!center_result)
      return std::unexpected(std::move(center_result.error()));
    auto center = std::move(*center_result);
    m_animation_center_line = center;
    m_networked_animator->set_flipped(direction() == Direction::Left, m_animation_center_line);
    // Do not animate initial state transitions unless explicitly requested.
    auto finish_result = config_value("forceFinishAnimationsInInit", true);
    if (!finish_result)
      return std::unexpected(std::move(finish_result.error()));
    auto finish = std::move(*finish_result);
    if (finish != false) {
      auto operation16 = m_networked_animator->finish_animations();
      if (!operation16)
        return std::unexpected(std::move(operation16.error()));
    }
    return {};
  }();
  if (!initialized) {
    auto cleaned = uninit();
    if (!cleaned)
      return std::unexpected(initialized.error().with_context(Star::format("Object initialization cleanup failed: {}", Star::output_exception(cleaned.error(), false))));
    return std::unexpected(std::move(initialized.error()));
  }
  return {};
}

std::expected<void, StarException> Object::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) { if (!result && !error) error = std::move(result.error()); };
  if (isMaster()) {
    remember(recover_lua_script_error(m_script_component.uninit()));
    remember(m_script_component.remove_callbacks("object"));
    remember(m_script_component.remove_callbacks("config"));
    remember(m_script_component.remove_callbacks("entity"));
    remember(m_script_component.remove_callbacks("animator"));
  }
  if (auto attached_world = maybeWorld(); attached_world && attached_world->get().isClient()) {
    remember(recover_lua_script_error(m_scripted_animator.uninit()));
    remember(m_scripted_animator.remove_callbacks("animationConfig"));
    remember(m_scripted_animator.remove_callbacks("objectAnimator"));
    remember(m_scripted_animator.remove_callbacks("config"));
    remember(m_scripted_animator.remove_callbacks("entity"));
  }
  if (m_sound_effect)
    m_sound_effect->stop();
  m_render_event_collection.reset();
  remember(Entity::uninit());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<List<LightSource>, StarException> Object::light_sources() const {
  auto lights = m_networked_animator->light_sources(position() + m_animation_position);
  if (!lights)
    return std::unexpected(std::move(lights.error()));
  append_configured_light_source(*lights);
  return lights;
}

void Object::append_configured_light_source(List<LightSource>& lights) const {
  auto orientation = current_orientation();
  if (!m_light_source_color.get().is_clear() && orientation) {
    Color color = m_light_source_color.get();
    if (m_light_flickering)
      color.set_value(std::clamp(color.value() * m_light_flickering->value(SinWeightOperator<float>()), 0.0f, 1.0f));

    lights.push_back(LightSource{
        .position = position() + center_of_tile(orientation->light_position),
        .color = color.to_rgb_f(),
        .type = m_config->light_type,
        .point_beam = m_config->point_beam,
        .beam_angle = orientation->beam_angle,
        .beam_ambience = m_config->beam_ambience});
  }
}

Vec2F Object::position() const {
  return Vec2F(m_x_tile_position.get(), m_y_tile_position.get());
}

RectF Object::meta_bound_box() const {
  if (auto orientation = current_orientation()) {
    // default metaboundbox extends the bounding box of the orientation's
    // spaces by one block
    return orientation->meta_bound_box.value_or(RectF(Vec2F(orientation->bound_box.min()) - Vec2F(1, 1), Vec2F(orientation->bound_box.max()) + Vec2F(2, 2)));
  } else {
    return RectF(-1, -1, 1, 1);
  }
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Object::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  return m_net_group.write_net_state(from_version, rules);
}

std::expected<void, StarException> Object::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  return m_net_group.read_net_state(data, interpolation_time, rules);
}

Vec2I Object::tile_position() const {
  return Vec2I(m_x_tile_position.get(), m_y_tile_position.get());
}

std::expected<void, StarException> Object::set_tile_position(Vec2I const& pos) {
  if (m_x_tile_position.get() != pos[0] || m_y_tile_position.get() != pos[1]) {
    m_x_tile_position.set(pos[0]);
    m_y_tile_position.set(pos[1]);
    if (inWorld())
      return update_orientation();
  }
  return {};
}

Direction Object::direction() const {
  return m_direction.get();
}

void Object::set_direction(Direction direction) {
  m_direction.set(direction);
}

std::expected<void, StarException> Object::update_orientation() {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto& attached_world = current_world->get();
  auto index = ObjectConfig::find_valid_orientation(get_orientations(), attached_world,
      attached_world.geometry().xwrap(tile_position()), m_direction.get());
  if (!index)
    return std::unexpected(std::move(index.error()));
  if (auto changed = set_orientation_index(*index); !changed)
    return std::unexpected(std::move(changed.error()));
  if (auto orientation = current_orientation()) {
    if (orientation->direction_affinity)
      m_direction.set(*orientation->direction_affinity);
    m_material_spaces.set(orientation->material_spaces);
  } else {
    m_material_spaces.set({});
  }
  reset_emission_timers();
  return {};
}

List<Vec2I> Object::anchor_positions() const {
  if (auto orientation = current_orientation()) {
    List<Vec2I> positions;
    for (auto anchor : orientation->anchors)
      positions.push_back(anchor.position + tile_position());
    return positions;
  } else {
    return {};
  }
}

List<Vec2I> Object::spaces() const {
  if (auto orientation = current_orientation())
    return orientation->spaces;
  else
    return {};
}

List<MaterialSpace> Object::material_spaces() const {
  return m_material_spaces.get();
}

List<Vec2I> Object::roots() const {
  if (m_config->rooting) {
    if (auto orientation = current_orientation()) {
      List<Vec2I> res;
      for (auto anchor : orientation->anchors)
        res.push_back(anchor.position);
      return res;
    }
  }
  return {};
}

namespace {

#pragma float_control(precise, on, push)
int object_animation_frame(float timer, ObjectOrientation const& orientation) {
  int last = orientation.frames - 1;
  if (!(timer > 0.0f) || last == 0)
    return 0;
  if (timer >= orientation.animation_cycle)
    return last;
  auto frame = timer / orientation.animation_cycle * orientation.frames;
  // The float quotient can round up to 2^31 even for a valid int frame count.
  return static_cast<double>(frame) >= last ? last : static_cast<int>(frame);
}

float advance_object_animation(float timer, float elapsed, float cycle) {
  float next = timer + elapsed;
  if (next >= 0.0f && next < cycle)
    return next;
  if (!is_finite(next))
    return static_cast<float>(std::fmod(static_cast<double>(timer) + elapsed, cycle));
  return std::fmod(next, cycle);
}
#pragma float_control(pop)

}

std::expected<void, StarException> Object::update(float dt, std::uint64_t) {
  if (!inWorld())
    return {};
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  if (isMaster()) {
    m_tile_damage_status->recover(m_config->tile_damage_parameters, dt);
    if (m_liquid_check_timer.wrap_tick(root().timing().time_step)) {
      auto operation1 = check_liquid_broken();
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
    if (auto orientation = current_orientation()) {
      // Sample before advancing the clock, as with the legacy object frames.
      int frame = object_animation_frame(m_animation_timer, *orientation);
      if (m_current_frame != frame) {
        m_current_frame = frame;
        set_image_key("frame", Star::to_string(frame));
      }
      m_animation_timer = advance_object_animation(m_animation_timer, dt, orientation->animation_cycle);
    }
    auto operation2 = m_networked_animator->update(dt, std::nullopt);
    if (!operation2)
      return std::unexpected(std::move(operation2.error()));
    m_networked_animator->set_flipped(direction() == Direction::Left, m_animation_center_line);
    auto operation3 = recover_lua_script_error(m_script_component.update(m_script_component.update_dt(dt)));
    if (!operation3)
      return std::unexpected(std::move(operation3.error()));
  } else {
    auto operation4 = m_networked_animator->update(dt, std::ref(m_networked_animator_dynamic_target));
    if (!operation4)
      return std::unexpected(std::move(operation4.error()));
    m_networked_animator_dynamic_target.update_position(position() + m_animation_position);
  }
  if (m_light_flickering)
    m_light_flickering->update(dt);
  for (auto& timer : m_emission_timers)
    if (timer)
      timer->tick(dt);
  if (current_world.isClient()) {
    auto operation5 = recover_lua_script_error(m_scripted_animator.update());
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
  }
  return {};
}

std::expected<void, StarException> Object::collect_render_events(RenderCallback& render_callback) {
  // The successful path needs no persistent allocation. On failure keep the
  // exact rejected item and resume the first unconfirmed stage next time.
  RenderEventCollection fresh;
  auto& events = m_render_event_collection ? *m_render_event_collection : fresh;
  using Stage = RenderEventCollection::Stage;
  auto collect = [&]() -> std::expected<void, StarException> {
    switch (events.stage) {
      case Stage::Start: {
        for (auto const& [name, value] : m_image_keys)
          m_networked_animator->set_global_tag(name, value);
        auto shake = compute_damage_shake();
        if (!shake)
          return std::unexpected(std::move(shake.error()));
        m_presentation_damage_shake = *shake;
        events.stage = Stage::Particles;
        [[fallthrough]];
      }
      case Stage::Particles:
        if (auto submitted = collect_particles(render_callback, events); !submitted)
          return submitted;
        events.stage = Stage::Sound;
        [[fallthrough]];
      case Stage::Sound:
        if (auto submitted = collect_sounds(render_callback, events); !submitted)
          return submitted;
        events.stage = Stage::AnimatorAudio;
        [[fallthrough]];
      case Stage::AnimatorAudio:
        if (auto submitted = events.submit_batch(render_callback, [&] { return m_networked_animator_dynamic_target.pull_new_audios(); }); !submitted)
          return submitted;
        events.stage = Stage::AnimatorParticles;
        [[fallthrough]];
      case Stage::AnimatorParticles:
        if (auto submitted = events.submit_batch(render_callback, [&] { return m_networked_animator_dynamic_target.pull_new_particles(); }); !submitted)
          return submitted;
        events.stage = Stage::ScriptedParticles;
        [[fallthrough]];
      case Stage::ScriptedParticles:
        if (auto submitted = events.submit_batch(render_callback, [&] { return m_scripted_animator.pull_new_particles(); }); !submitted)
          return submitted;
        events.stage = Stage::ScriptedAudio;
        [[fallthrough]];
      case Stage::ScriptedAudio:
        return events.submit_batch(render_callback, [&] { return m_scripted_animator.pull_new_audios(); });
    }
    std::unreachable();
  };
  auto result = collect();
  if (result)
    m_render_event_collection.reset();
  else if (!m_render_event_collection && fresh.stage != Stage::Start)
    m_render_event_collection = std::make_unique<RenderEventCollection>(std::move(fresh));
  return result;
}

std::expected<void, StarException> Object::render(RenderCallback& render_callback) {
  if (!m_networked_animator->const_parts().empty()) {
    auto sample = m_networked_animator->sample(render_callback.presentation().interpolation());
    auto shake_result = damage_shake();
    if (!shake_result)
      return std::unexpected(std::move(shake_result.error()));
    auto shake = std::move(*shake_result);
    auto drawables_result = sample.drawables(position() + m_animation_position + shake);
    if (!drawables_result)
      return std::unexpected(std::move(drawables_result.error()));
    auto drawables = std::move(*drawables_result);
    auto operation5 = render_callback.add_drawables(std::move(drawables), render_layer());
    if (!operation5)
      return std::unexpected(std::move(operation5.error()));
  } else if (m_orientation_index != npos) {
    auto layers = orientation_layers(m_orientation_index);
    if (!layers)
      return std::unexpected(std::move(layers.error()));
    auto shake = damage_shake();
    if (!shake)
      return std::unexpected(std::move(shake.error()));
    auto const local_offset = get_orientations()[m_orientation_index]->image_position + *shake;
    auto const world_position = position();
    auto const layer = render_layer();
    // Keep the current layers alive even if a sink invalidates the cache.
    for (auto drawable : **layers) {
      drawable.translate(local_offset);
      if (world_position != Vec2F())
        drawable.translate(world_position);
      if (auto added = render_callback.add_drawable(std::move(drawable), layer); !added)
        return std::unexpected(std::move(added.error()));
    }
  }
  auto const& drawables = m_scripted_animator.drawables();
  for (auto const& pair : drawables) {
    auto operation7 = render_callback.add_drawable(pair.first, pair.second.value_or(render_layer()));
    if (!operation7)
      return std::unexpected(std::move(operation7.error()));
  }
  return {};
}

std::expected<void, StarException> Object::render_light_sources(RenderCallback& render_callback) {
  List<LightSource> lights;
  if (m_config->animation_config) {
    auto sample = m_networked_animator->sample(render_callback.presentation().interpolation());
    if (auto appended = sample.append_light_sources(lights, position() + m_animation_position); !appended)
      return std::unexpected(std::move(appended.error()));
  }
  append_configured_light_source(lights);
  if (auto added = render_callback.add_light_sources(lights); !added)
    return std::unexpected(std::move(added.error()));
  return render_callback.add_light_sources(m_scripted_animator.light_sources());
}

std::expected<bool, StarException> Object::damage_tiles(List<Vec2I> const&, Vec2F const&, TileDamage const& tile_damage) {
  if (m_unbreakable)
    return false;
  m_tile_damage_status->damage(m_config->tile_damage_parameters, tile_damage);
  if (m_tile_damage_status->dead())
    m_broken = true;
  return m_broken;
}

std::expected<bool, StarException> Object::can_be_damaged() const {
  return !m_unbreakable;
}

std::expected<bool, StarException> Object::check_broken() {
  if (!m_broken && !m_unbreakable) {
    if (auto orientation = current_orientation()) {
      auto current_world_result = world();
      if (!current_world_result)
        return std::unexpected(std::move(current_world_result.error()));
      auto& current_world = current_world_result->get();
      auto valid_result = orientation->anchors_valid(current_world, tile_position());
      if (!valid_result)
        return std::unexpected(std::move(valid_result.error()));
      auto valid = std::move(*valid_result);
      if (!valid)
        m_broken = true;
    } else {
      m_broken = true;
    }
  }
  return m_broken;
}

std::expected<bool, StarException> Object::should_destroy() const {
  return m_broken || (m_health.get() <= 0);
}

std::expected<void, StarException> Object::destroy(std::optional<std::reference_wrapper<RenderCallback>> render_callback) {
  bool do_smash = m_health.get() <= 0;
  if (!do_smash) {
    auto smash_config_result = config_value("smashOnBreak", m_config->smash_on_break);
    if (!smash_config_result)
      return std::unexpected(std::move(smash_config_result.error()));
    auto smash_config = std::move(*smash_config_result);
    auto smash_result = smash_config.to_bool();
    if (!smash_result)
      return std::unexpected(std::move(smash_result.error()));
    auto smash = std::move(*smash_result);
    do_smash = smash;
  }
  if (isMaster()) {
    auto operation1 = recover_lua_script_error(m_script_component.invoke("die", m_health.get() <= 0));
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    auto dropped = [&]() -> std::expected<void, StarException> {
      auto current_world_result = world();
      if (!current_world_result)
        return std::unexpected(std::move(current_world_result.error()));
      auto& current_world = current_world_result->get();
      auto drop_item = [&]<class Drop>(Drop const& item) -> std::expected<void, StarException>
          requires (std::same_as<Drop, ItemDescriptor> || std::same_as<Drop, std::shared_ptr<Item>>) {
        auto drop_result = ItemDrop::create_randomized_drop(root(), item, position());
        if (!drop_result)
          return std::unexpected(std::move(drop_result.error()));
        auto drop = std::move(*drop_result);
        auto operation2 = current_world.addEntity(std::move(drop));
        if (!operation2)
          return std::unexpected(std::move(operation2.error()));
        return {};
      };
      if (do_smash) {
        auto pool_config_result = config_value("smashDropPool", "");
        if (!pool_config_result)
          return std::unexpected(std::move(pool_config_result.error()));
        auto pool_config = std::move(*pool_config_result);
        auto pool_result = pool_config.to_string();
        if (!pool_result)
          return std::unexpected(std::move(pool_result.error()));
        auto pool = std::move(*pool_result);
        if (!pool.empty()) {
          auto treasure_result = root().treasureDatabase();
          if (!treasure_result)
            return std::unexpected(std::move(treasure_result.error()));
          auto treasure = std::move(*treasure_result);
          auto level_result = current_world.threatLevel();
          if (!level_result)
            return std::unexpected(std::move(level_result.error()));
          auto level = std::move(*level_result);
          auto items_result = treasure->createTreasure(pool, level);
          if (!items_result)
            return std::unexpected(std::move(items_result.error()));
          auto items = std::move(*items_result);
          for (auto const& item : items) {
            auto operation3 = drop_item(item);
            if (!operation3)
              return std::unexpected(std::move(operation3.error()));
          }
        } else if (!m_config->smash_drop_options.empty()) {
          auto options_result = root().random().sample_ref(m_config->smash_drop_options);
          if (!options_result)
            return std::unexpected(std::move(options_result.error()));
          auto const& options = options_result->get();
          for (auto const& item : options) {
            auto operation4 = drop_item(item);
            if (!operation4)
              return std::unexpected(std::move(operation4.error()));
          }
        }
      } else {
        auto pool_config_result = config_value("breakDropPool", "");
        if (!pool_config_result)
          return std::unexpected(std::move(pool_config_result.error()));
        auto pool_config = std::move(*pool_config_result);
        auto pool_result = pool_config.to_string();
        if (!pool_result)
          return std::unexpected(std::move(pool_result.error()));
        auto pool = std::move(*pool_result);
        if (!pool.empty()) {
          auto treasure_result = root().treasureDatabase();
          if (!treasure_result)
            return std::unexpected(std::move(treasure_result.error()));
          auto treasure = std::move(*treasure_result);
          auto level_result = current_world.threatLevel();
          if (!level_result)
            return std::unexpected(std::move(level_result.error()));
          auto level = std::move(*level_result);
          auto items_result = treasure->createTreasure(pool, level);
          if (!items_result)
            return std::unexpected(std::move(items_result.error()));
          auto items = std::move(*items_result);
          for (auto const& item : items) {
            auto operation5 = drop_item(item);
            if (!operation5)
              return std::unexpected(std::move(operation5.error()));
          }
        } else if (!m_config->break_drop_options.empty()) {
          auto options_result = root().random().sample_ref(m_config->break_drop_options);
          if (!options_result)
            return std::unexpected(std::move(options_result.error()));
          auto const& options = options_result->get();
          for (auto const& item : options) {
            auto operation6 = drop_item(item);
            if (!operation6)
              return std::unexpected(std::move(operation6.error()));
          }
        } else if (m_config->has_object_item) {
          auto item_result = ItemDescriptor::create(m_config->name, 1);
          if (!item_result)
            return std::unexpected(std::move(item_result.error()));
          auto item = std::move(*item_result);
          auto retain_config_result = config_value("retainObjectParametersInItem", m_config->retain_object_parameters_in_item);
          if (!retain_config_result)
            return std::unexpected(std::move(retain_config_result.error()));
          auto retain_config = std::move(*retain_config_result);
          auto retain_result = retain_config.opt_bool();
          if (!retain_result)
            return std::unexpected(std::move(retain_result.error()));
          auto retain = std::move(*retain_result);
          if (retain.value_or(false)) {
            auto parameters = m_parameters.base_map();
            parameters.remove("owner");
            auto storage_result = m_script_component.get_script_storage();
            if (!storage_result)
              return std::unexpected(std::move(storage_result.error()));
            auto storage = std::move(*storage_result);
            parameters["scriptStorage"] = std::move(storage);
            auto retained = item.apply_parameters(parameters);
            if (!retained)
              return std::unexpected(std::move(retained.error()));
            item = std::move(*retained);
          }
          auto operation7 = drop_item(item);
          if (!operation7)
            return std::unexpected(std::move(operation7.error()));
        }
      }
      return {};
    }();
    if (!dropped) {
      auto operation8 =
        root().logger().warn_exception(Star::format("Failed to create treasure for object '{}'", m_config->name),
                                       ExceptionDiagnostic(dropped.error()));
      if (!operation8)
        return std::unexpected(std::move(operation8.error()));
    }
  }
  if (render_callback && do_smash && !m_config->smash_sound_options.empty()) {
    auto assets_result = root().assets();
    if (!assets_result)
      return std::unexpected(std::move(assets_result.error()));
    auto assets = std::move(*assets_result);
    auto sound_result = root().random().sample_ref(m_config->smash_sound_options);
    if (!sound_result)
      return std::unexpected(std::move(sound_result.error()));
    auto const& sound = sound_result->get();
    auto audio_result = assets->audio(sound);
    if (!audio_result)
      return std::unexpected(std::move(audio_result.error()));
    auto audio = std::move(*audio_result);
    auto instance_result = AudioInstance::create(*audio);
    if (!instance_result)
      return std::unexpected(std::move(instance_result.error()));
    auto instance = std::move(*instance_result);
    instance->translate(position());
    auto operation9 = render_callback->get().add_audio(std::move(instance));
    if (!operation9)
      return std::unexpected(std::move(operation9.error()));
  }
  if (render_callback && do_smash && !m_config->smash_particles.empty()) {
    auto particles_database_result = root().particleDatabase();
    if (!particles_database_result)
      return std::unexpected(std::move(particles_database_result.error()));
    auto particles_database = std::move(*particles_database_result);
    List<Particle> particles;
    for (auto const& config : m_config->smash_particles) {
      auto particle_config_result = config.get("particle");
      if (!particle_config_result)
        return std::unexpected(std::move(particle_config_result.error()));
      auto particle_config = std::move(*particle_config_result);
      auto creator_result = particles_database->particleCreator(particle_config, m_config->path);
      if (!creator_result)
        return std::unexpected(std::move(creator_result.error()));
      auto creator = std::move(*creator_result);
      auto count_result = config.get_uint("count", 1);
      if (!count_result)
        return std::unexpected(std::move(count_result.error()));
      auto count = std::move(*count_result);
      auto offset_config_result = config.get("offset", JsonArray{0, 0});
      if (!offset_config_result)
        return std::unexpected(std::move(offset_config_result.error()));
      auto offset_config = std::move(*offset_config_result);
      auto offset_result = json_to_vec2f(offset_config);
      if (!offset_result)
        return std::unexpected(std::move(offset_result.error()));
      auto offset = std::move(*offset_result);
      auto flip_result = config.get_bool("flip", false);
      if (!flip_result)
        return std::unexpected(std::move(flip_result.error()));
      auto flip = std::move(*flip_result);
      for (unsigned i = 0; i < count; ++i) {
        Particle particle = creator();
        particle.position += offset;
        if (flip)
          particle.flip = !particle.flip;
        if (m_direction.get() == Direction::Left) {
          particle.position[0] *= -1;
          particle.velocity[0] *= -1;
          particle.flip = !particle.flip;
        }
        particle.translate(position() + volume().center());
        particles.push_back(std::move(particle));
      }
    }
    auto operation10 = render_callback->get().add_particles(std::move(particles));
    if (!operation10)
      return std::unexpected(std::move(operation10.error()));
  }
  if (m_sound_effect)
    m_sound_effect->stop(1.0f);
  return {};
}

std::expected<String, StarException> Object::name() const {
  return m_config->name;
}

std::expected<String, StarException> Object::short_description() const {
  auto value_result = config_value("shortdescription", m_config->name);
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  return value.to_string();
}

std::expected<String, StarException> Object::description() const {
  auto fallback_result = short_description();
  if (!fallback_result)
    return std::unexpected(std::move(fallback_result.error()));
  auto fallback = std::move(*fallback_result);
  auto value_result = config_value("description", fallback);
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  return value.to_string();
}

bool Object::inspectable() const {
  return m_config->scannable;
}

std::expected<std::optional<String>, StarException> Object::inspection_log_name() const {
  auto value_result = config_value("inspectionLogName");
  if (!value_result)
    return std::unexpected(std::move(value_result.error()));
  auto value = std::move(*value_result);
  auto name_result = value.opt_string();
  if (!name_result)
    return std::unexpected(std::move(name_result.error()));
  auto name = std::move(*name_result);
  return name.value_or(m_config->name);
}

std::expected<std::optional<String>, StarException> Object::inspection_description(String const& species) const {
  auto general_config_result = config_value("inspectionDescription");
  if (!general_config_result)
    return std::unexpected(std::move(general_config_result.error()));
  auto general_config = std::move(*general_config_result);
  auto general_result = general_config.opt_string();
  if (!general_result)
    return std::unexpected(std::move(general_result.error()));
  auto general = std::move(*general_result);
  auto specific_config_result = config_value(Star::format("{}Description", species));
  if (!specific_config_result)
    return std::unexpected(std::move(specific_config_result.error()));
  auto specific_config = std::move(*specific_config_result);
  auto specific_result = specific_config.opt_string();
  if (!specific_result)
    return std::unexpected(std::move(specific_result.error()));
  auto specific = std::move(*specific_result);
  auto fallback_result = description();
  if (!fallback_result)
    return std::unexpected(std::move(fallback_result.error()));
  auto fallback = std::move(*fallback_result);
  return (general ? general : specific).value_or(fallback);
}

String Object::category() const {
  return m_config->category;
}

std::shared_ptr<ObjectOrientation> Object::current_orientation() const {
  if (m_orientation_index != npos)
    return get_orientations()[m_orientation_index];
  else
    return {};
}

std::expected<List<Drawable>, StarException> Object::cursor_hint_drawables() const {
  auto placement_config_result = config_value("placementImage");
  if (!placement_config_result)
    return std::unexpected(std::move(placement_config_result.error()));
  auto placement_config = std::move(*placement_config_result);
  if (placement_config) {
    auto placement_image_result = placement_config.to_string();
    if (!placement_image_result)
      return std::unexpected(std::move(placement_image_result.error()));
    auto placement_image = std::move(*placement_image_result);
    if (m_direction.get() == Direction::Left)
      placement_image += "?flipx";
    auto metadata_result = root().image_metadata_database();
    if (!metadata_result)
      return std::unexpected(std::move(metadata_result.error()));
    auto metadata = std::move(*metadata_result);
    auto position_config_result = config_value("placementImagePosition", json_from_vec2f(Vec2F()));
    if (!position_config_result)
      return std::unexpected(std::move(position_config_result.error()));
    auto position_config = std::move(*position_config_result);
    auto image_position_result = json_to_vec2f(position_config);
    if (!image_position_result)
      return std::unexpected(std::move(image_position_result.error()));
    auto image_position = std::move(*image_position_result);
    auto drawable_result = Drawable::make_image(*metadata, AssetPath::relative_to(m_config->path, placement_image),
                                              1.0 / tile_pixels, false, image_position / tile_pixels);
    if (!drawable_result)
      return std::unexpected(std::move(drawable_result.error()));
    auto drawable = std::move(*drawable_result);
    return List<Drawable>{std::move(drawable)};
  }
  if (m_orientation_index != npos)
    return orientation_drawables(m_orientation_index);
  List<Drawable> result;
  auto const& orientations = get_orientations();
  for (std::size_t i = 0; i < orientations.size(); ++i) {
    if (orientations[i]->direction_affinity && *orientations[i]->direction_affinity == m_direction.get()) {
      auto drawables_result = orientation_drawables(i);
      if (!drawables_result)
        return std::unexpected(std::move(drawables_result.error()));
      auto drawables = std::move(*drawables_result);
      result = std::move(drawables);
      break;
    }
  }
  if (result.empty())
    return orientation_drawables(0);
  return result;
}

std::expected<void, StarException> Object::get_net_states(bool initial) {
  set_unique_id(m_unique_id_net_state.get());
  if (m_orientation_index != m_orientation_index_net_state.get()) {
    auto operation1 = set_orientation_index(m_orientation_index_net_state.get());
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
  }
  if (m_new_chat_message_event.pull_occurred() && !initial) {
    auto mouth_result = mouth_position();
    if (!mouth_result)
      return std::unexpected(std::move(mouth_result.error()));
    auto mouth = std::move(*mouth_result);
    if (m_chat_portrait.get().empty())
      m_pending_chat_actions.push_back(SayChatAction{entityId(), m_chat_message.get(), mouth});
    else
      m_pending_chat_actions.push_back(
        PortraitChatAction{entityId(), m_chat_portrait.get(), m_chat_message.get(), mouth, m_chat_config.get()});
  }
  if (m_net_image_keys.pull_updated()) {
    m_image_keys.merge(m_net_image_keys.base_map(), true);
    m_orientation_drawables_cache.reset();
  }
  return {};
}

std::expected<void, StarException> Object::set_net_states() {
  m_unique_id_net_state.set(uniqueId());
  m_orientation_index_net_state.set(m_orientation_index);
  return {};
}

std::expected<List<QuestArcDescriptor>, StarException> Object::offered_quests() const {
  return m_offered_quests.get();
}

std::expected<StringSet, StarException> Object::turn_in_quests() const {
  return m_turn_in_quests.get();
}

std::expected<Vec2F, StarException> Object::quest_indicator_position() const {
  if (auto orientation = current_orientation()) {
    auto pos = position() + Vec2F(orientation->bound_box.center()[0], orientation->bound_box.max()[1] + 2.5);
    if (!(orientation->bound_box.size()[0] % 2))
      pos[0] += 0.5;
    auto config_position_result = config_value("questIndicatorPosition", Json());
    if (!config_position_result)
      return std::unexpected(std::move(config_position_result.error()));
    auto config_position = std::move(*config_position_result);
    if (config_position) {
      auto offset_result = json_to_vec2f(config_position);
      if (!offset_result)
        return std::unexpected(std::move(offset_result.error()));
      auto offset = std::move(*offset_result);
      if (m_direction.get() == Direction::Left)
        offset[0] = -offset[0];
      pos += offset;
    }
    return pos;
  }
  return position();
}

MessageResponseResult Object::receive_message(ConnectionId sending_connection, String const& message, JsonArray const& args) {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  return recover_lua_script_error(m_script_component.handle_message(message, sending_connection == current_world.connection(), args));
}

std::expected<Json, StarException> Object::config_value(String const& name, Json const& def) const {
  if (auto orientation = current_orientation())
    return json_merge_query_def(name, def, m_config->config, orientation->config, m_parameters.base_map());
  else
    return json_merge_query_def(name, def, m_config->config, m_parameters.base_map());
}

std::shared_ptr<ObjectConfig const> Object::config() const {
  return m_config;
}

std::expected<void, StarException> Object::read_stored_data(Json const& disk_store) {
  auto id_result = disk_store.opt_string("uniqueId");
  if (!id_result)
    return std::unexpected(std::move(id_result.error()));
  auto id = std::move(*id_result);
  set_unique_id(std::move(id));
  auto position_config_result = disk_store.get("tilePosition");
  if (!position_config_result)
    return std::unexpected(std::move(position_config_result.error()));
  auto position_config = std::move(*position_config_result);
  auto position_result = json_to_vec2i(position_config);
  if (!position_result)
    return std::unexpected(std::move(position_result.error()));
  auto position = std::move(*position_result);
  auto operation1 = set_tile_position(position);
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto orientation_config_result = disk_store.get("orientationIndex");
  if (!orientation_config_result)
    return std::unexpected(std::move(orientation_config_result.error()));
  auto orientation_config = std::move(*orientation_config_result);
  auto index_result = json_to_size(orientation_config);
  if (!index_result)
    return std::unexpected(std::move(index_result.error()));
  auto index = std::move(*index_result);
  auto operation2 = set_orientation_index(index);
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto direction_result = disk_store.get_string("direction");
  if (!direction_result)
    return std::unexpected(std::move(direction_result.error()));
  auto direction = std::move(*direction_result);
  auto direction_value_result = enum_value<direction_names>(direction);
  if (!direction_value_result)
    return std::unexpected(std::move(direction_value_result.error()));
  auto direction_value = std::move(*direction_value_result);
  m_direction.set(direction_value);
  auto action_result = config_value("interactAction", Json());
  if (!action_result)
    return std::unexpected(std::move(action_result.error()));
  auto action = std::move(*action_result);
  auto interactive_result = disk_store.get_bool("interactive", !action.is_null());
  if (!interactive_result)
    return std::unexpected(std::move(interactive_result.error()));
  auto interactive = std::move(*interactive_result);
  m_interactive.set(interactive);
  auto storage_result = disk_store.get_object("scriptStorage", JsonObject());
  if (!storage_result)
    return std::unexpected(std::move(storage_result.error()));
  auto storage = std::move(*storage_result);
  auto operation3 = m_script_component.set_script_storage(std::move(storage));
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto read_nodes = [&](WireDirection direction, String const& key) -> std::expected<void, StarException> {
    auto stored_nodes = disk_store.get_array(key);
    if (!stored_nodes)
      return std::unexpected(std::move(stored_nodes.error()));
    auto& entries = nodes(direction);
    auto count = std::min(entries.size(), stored_nodes->size());
    for (std::size_t i = 0; i < count; ++i) {
      auto stored_connections = (*stored_nodes)[i].get_array("connections");
      if (!stored_connections)
        return std::unexpected(std::move(stored_connections.error()));
      List<WireConnection> connections;
      connections.reserve(stored_connections->size());
      for (auto const& connection : *stored_connections) {
        auto location = connection.get(0).and_then(json_to_vec2i);
        if (!location)
          return std::unexpected(std::move(location.error()));
        auto node_index = connection.get_uint(1);
        if (!node_index)
          return std::unexpected(std::move(node_index.error()));
        connections.push_back(WireConnection{.entity_location = *location, .node_index = static_cast<std::size_t>(*node_index)});
      }
      auto& entry = entries[i];
      entry.connections.set(std::move(connections));
      auto state = (*stored_nodes)[i].get_bool("state");
      if (!state)
        return std::unexpected(std::move(state.error()));
      entry.state.set(*state);
    }
    return {};
  };
  if (auto input = read_nodes(WireDirection::Input, "inputWireNodes"); !input)
    return std::unexpected(std::move(input.error()));
  return read_nodes(WireDirection::Output, "outputWireNodes");
}

std::expected<Json, StarException> Object::write_stored_data() const {
  auto write_nodes = [&](WireDirection direction) {
    auto const& entries = nodes(direction);
    JsonArray result;
    result.reserve(entries.size());
    for (auto const& entry : entries) {
      auto const& source = entry.connections.get();
      JsonArray connections;
      connections.reserve(source.size());
      for (auto const& connection : source)
        connections.push_back(JsonArray{json_from_vec2i(connection.entity_location), connection.node_index});
      result.push_back(JsonObject{{"connections", std::move(connections)}, {"state", entry.state.get()}});
    }
    return result;
  };
  auto input_nodes = write_nodes(WireDirection::Input);
  auto output_nodes = write_nodes(WireDirection::Output);

  auto direction = enum_name<direction_names>(m_direction.get());
  if (!direction)
    return std::unexpected(std::move(direction.error()));
  auto storage = m_script_component.get_script_storage();
  if (!storage)
    return std::unexpected(std::move(storage.error()));
  return JsonObject{{"uniqueId", json_from_optional(uniqueId())},
                    {"tilePosition", json_from_vec2i(tile_position())},
                    {"orientationIndex", json_from_size(m_orientation_index)},
                    {"direction", String(*direction)},
                    {"scriptStorage", std::move(*storage)},
                    {"interactive", m_interactive.get()},
                    {"inputWireNodes", std::move(input_nodes)},
                    {"outputWireNodes", std::move(output_nodes)}};
}

std::expected<void, StarException> Object::break_object(bool smash) {
  m_broken = true;
  if (smash)
    m_health.set(0.0f);
  return {};
}

List<Object::Node>& Object::nodes(WireDirection direction) {
  return direction == WireDirection::Input ? m_input_nodes : m_output_nodes;
}

List<Object::Node> const& Object::nodes(WireDirection direction) const {
  return direction == WireDirection::Input ? m_input_nodes : m_output_nodes;
}

std::expected<std::reference_wrapper<Object::Node>, StarException> Object::node(WireNode wire_node) {
  return nodes(wire_node.direction).at(wire_node.node_index);
}

std::expected<std::reference_wrapper<Object::Node const>, StarException> Object::node(WireNode wire_node) const {
  return nodes(wire_node.direction).at(wire_node.node_index);
}

std::size_t Object::node_count(WireDirection direction) const {
  return nodes(direction).size();
}

std::expected<Vec2I, StarException> Object::node_position(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) { return entry.get().position; });
}

std::expected<List<WireConnection>, StarException> Object::connections_for_node(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) { return entry.get().connections.get(); });
}

std::expected<bool, StarException> Object::node_state(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) { return entry.get().state.get(); });
}

std::expected<String, StarException> Object::node_icon(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) { return entry.get().icon; });
}

std::expected<Color, StarException> Object::node_color(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) { return entry.get().color; });
}

std::expected<bool, StarException> Object::node_connected(WireNode wire_node) const {
  return node(wire_node).transform([](std::reference_wrapper<Node const> entry) {
    return !entry.get().connections.get().empty();
  });
}

std::expected<LuaTable, StarException> Object::node_ids(LuaEngine& engine, WireNode wire_node) const {
  auto table = engine.create_table();
  if (!table)
    return std::unexpected(std::move(table.error()));
  auto entry = node(wire_node);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  for (auto const& connection : entry->get().connections.get()) {
    auto entities = current_world->get().atTile<WireEntity>(connection.entity_location);
    if (!entities)
      return std::unexpected(std::move(entities.error()));
    for (auto const& entity : *entities) {
      if (auto set = table->set(entity->entityId(), connection.node_index); !set)
        return std::unexpected(std::move(set.error()));
    }
  }
  return table;
}

std::expected<void, StarException> Object::add_node_connection(WireNode wire_node, WireConnection connection) {
  if (nodes(wire_node.direction).empty()) {
    std::string_view direction = wire_node.direction == WireDirection::Input ? "input" : "output";
    return root().logger().info("Tried to add wire connection to {} node on object with no {} nodes", direction, direction);
  }
  auto entry = node(wire_node);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  entry->get().connections.update([&](List<WireConnection>& connections) {
    if (connections.contains(connection))
      return false;
    connections.push_back(connection);
    return true;
  });
  auto notified = recover_lua_script_error(m_script_component.invoke("onNodeConnectionChange"));
  if (!notified)
    return std::unexpected(std::move(notified.error()));
  return {};
}

std::expected<void, StarException> Object::remove_node_connection(WireNode wire_node, WireConnection connection) {
  auto entry = node(wire_node);
  if (!entry)
    return std::unexpected(std::move(entry.error()));
  entry->get().connections.update([&](List<WireConnection>& connections) { return connections.remove(connection); });
  auto notified = recover_lua_script_error(m_script_component.invoke("onNodeConnectionChange"));
  if (!notified)
    return std::unexpected(std::move(notified.error()));
  return {};
}

std::expected<void, StarException> Object::evaluate(WireCoordinator& coordinator) {
  for (std::size_t i = 0; i < m_input_nodes.size(); ++i) {
    auto& node = m_input_nodes[i];
    bool next_state = false;
    // Read every connection: a later lookup may fail even after finding a high input.
    for (auto const& connection : node.connections.get()) {
      auto state_result = coordinator.read_input_connection(connection);
      if (!state_result)
        return std::unexpected(std::move(state_result.error()));
      auto state = std::move(*state_result);
      next_state |= state;
    }
    if (node.state.get() != next_state) {
      node.state.set(next_state);
      auto operation1 = recover_lua_script_error(m_script_component.invoke("onInputNodeChange", JsonObject{{"node", i}, {"level", next_state}}));
      if (!operation1)
        return std::unexpected(std::move(operation1.error()));
    }
  }
  return {};
}

void Object::set_image_key(String const& name, String const& value) {
  if (!isSlave())
    m_net_image_keys.set(name, value);

  if (auto p = m_image_keys.find_ref(name)) {
    if (p->get() != value) {
      p->get() = value;
      m_orientation_drawables_cache.reset();
    }
  } else {
    m_image_keys.set(name, value);
    m_orientation_drawables_cache.reset();
  }
}

void Object::reset_emission_timers() {
  ++m_emission_timer_revision;
  m_emission_timers.clear();
  if (auto orientation = current_orientation())
    m_emission_timers.resize(orientation->particle_emitters.size(), std::optional{GameTimer{}});
}

std::size_t Object::orientation_index() const {
  return m_orientation_index;
}

std::expected<void, StarException> Object::set_orientation_index(std::size_t orientation_index) {
  if (orientation_index != npos && orientation_index >= get_orientations().size())
    return std::unexpected(ObjectException::format("Invalid orientation {} for object '{}'", orientation_index, m_config->name));
  if (m_orientation_index != orientation_index)
    ++m_emission_timer_revision;
  m_orientation_index = orientation_index;
  return {};
}

PolyF Object::volume() const {
  if (auto orientation = current_orientation()) {
    RectF box = RectF(orientation->bound_box);
    box.max()[0]++;
    box.max()[1]++;
    return PolyF(box);
  } else {
    return PolyF(RectF(0, 0, 1, 1));
  }
}

std::expected<float, StarException> Object::liquid_fill_level() const {
  if (auto orientation = current_orientation())
    return spacesLiquidFillLevel(orientation->spaces);

  return 0;
}

bool Object::biome_placed() const {
  return m_config->biome_placed;
}

std::expected<LuaCallbacks, StarException> Object::make_object_callbacks() {
  LuaCallbacks callbacks;
  auto operation1 = callbacks.register_callback("name", [this]() {
  return name();
});
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto operation2 = callbacks.register_callback("direction", [this]() {
  return numerical_direction(direction());
});
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = callbacks.register_callback("position", [this]() {
  return position();
});
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  auto operation4 = callbacks.register_callback("uniqueId", [this]() {
  return uniqueId();
});
  if (!operation4)
    return std::unexpected(std::move(operation4.error()));
  auto operation5 = callbacks.register_callback("boundBox", [this]() {
  return meta_bound_box().translated(position());
});
  if (!operation5)
    return std::unexpected(std::move(operation5.error()));
  auto operation6 = callbacks.register_callback("spaces", [this]() {
  return spaces();
});
  if (!operation6)
    return std::unexpected(std::move(operation6.error()));
  auto operation7 = callbacks.register_callback("getLightColor", [this]() {
  return m_light_source_color.get();
});
  if (!operation7)
    return std::unexpected(std::move(operation7.error()));
  auto operation8 = callbacks.register_callback("inputNodeCount", [this]() {
  return m_input_nodes.size();
});
  if (!operation8)
    return std::unexpected(std::move(operation8.error()));
  auto operation9 = callbacks.register_callback("outputNodeCount", [this]() {
  return m_output_nodes.size();
});
  if (!operation9)
    return std::unexpected(std::move(operation9.error()));
  auto operation10 = callbacks.register_callback("health", [this]() {
  return m_health.get();
});
  if (!operation10)
    return std::unexpected(std::move(operation10.error()));
  auto operation11 = callbacks.register_callback("setInteractive", [this](bool interactive) {
  m_interactive.set(interactive);
});
  if (!operation11)
    return std::unexpected(std::move(operation11.error()));
  auto operation12 = callbacks.register_callback("setUniqueId", [this](std::optional<String> id) {
  set_unique_id(std::move(id));
});
  if (!operation12)
    return std::unexpected(std::move(operation12.error()));
  auto operation13 = callbacks.register_callback("setProcessingDirectives", [this](String const& directives) {
  m_networked_animator->set_processing_directives(directives);
});
  if (!operation13)
    return std::unexpected(std::move(operation13.error()));
  auto operation14 = callbacks.register_callback("setSoundEffectEnabled", [this](bool enabled) {
  m_sound_effect_enabled.set(enabled);
});
  if (!operation14)
    return std::unexpected(std::move(operation14.error()));
  auto operation15 = callbacks.register_callback("setLightColor", [this](Color const& color) {
  m_light_source_color.set(color);
});
  if (!operation15)
    return std::unexpected(std::move(operation15.error()));
  auto operation16 = callbacks.register_callback("setTurnInQuests", [this](std::optional<StringList> const& quests) {
  m_turn_in_quests.set(StringSet::from(quests.value_or(StringList{})));
});
  if (!operation16)
    return std::unexpected(std::move(operation16.error()));
  auto operation17 = callbacks.register_callback("setConfigParameter", [this](String key, Json value) {
  m_parameters.set(std::move(key), std::move(value));
});
  if (!operation17)
    return std::unexpected(std::move(operation17.error()));
  auto operation18 = callbacks.register_callback("setAnimationParameter", [this](String key, Json value) {
  m_scripted_animation_parameters.set(std::move(key), std::move(value));
});
  if (!operation18)
    return std::unexpected(std::move(operation18.error()));
  auto operation19 = callbacks.register_callback("setHealth", [this](float health) {
  m_health.set(health);
});
  if (!operation19)
    return std::unexpected(std::move(operation19.error()));
  auto operation20 = callbacks.register_callback("smash", [this](std::optional<bool> smash) {
  return break_object(smash.value_or(false));
});
  if (!operation20)
    return std::unexpected(std::move(operation20.error()));
  auto operation21 = callbacks.register_callback("level", [this]() -> std::expected<Json, StarException> {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto level_result = current_world.threatLevel();
  if (!level_result)
    return std::unexpected(std::move(level_result.error()));
  auto level = std::move(*level_result);
  return config_value("level", level);
});
  if (!operation21)
    return std::unexpected(std::move(operation21.error()));
  auto operation22 = callbacks.register_callback("toAbsolutePosition", [this](Vec2F const& p) {
  return p + position();
});
  if (!operation22)
    return std::unexpected(std::move(operation22.error()));
  auto operation23 = callbacks.register_callback("say", [this](String line, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
  if (tags)
    line = line.replace_tags(*tags, false);
  if (!line.empty()) {
    auto operation1 = add_chat_message(line, config);
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    return true;
  }
  return false;
});
  if (!operation23)
    return std::unexpected(std::move(operation23.error()));
  auto operation24 = callbacks.register_callback("sayPortrait", [this](String line, String portrait, std::optional<StringMap<String>> const& tags, Json const& config) -> std::expected<bool, StarException> {
  if (tags)
    line = line.replace_tags(*tags, false);
  if (!line.empty()) {
    auto operation1 = add_chat_message(line, config, portrait);
    if (!operation1)
      return std::unexpected(std::move(operation1.error()));
    return true;
  }
  return false;
});
  if (!operation24)
    return std::unexpected(std::move(operation24.error()));
  auto operation25 = callbacks.register_callback("isTouching", [this](EntityId entity_id) -> std::expected<bool, StarException> {
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  if (auto entity = current_world.entity(entity_id))
    return !entity->collision_area().overlap(volume().bounding_box()).is_empty();
  return false;
});
  if (!operation25)
    return std::unexpected(std::move(operation25.error()));
  auto operation26 = callbacks.register_callback("getInputNodePosition", [this](std::size_t i) {
  return node_position(WireNode{WireDirection::Input, i});
});
  if (!operation26)
    return std::unexpected(std::move(operation26.error()));
  auto operation27 = callbacks.register_callback("getInputNodeLevel", [this](std::size_t i) {
  return node_state(WireNode{WireDirection::Input, i});
});
  if (!operation27)
    return std::unexpected(std::move(operation27.error()));
  auto operation28 = callbacks.register_callback("isInputNodeConnected", [this](std::size_t i) {
    return node_connected({WireDirection::Input, i});
  });
  if (!operation28)
    return std::unexpected(std::move(operation28.error()));
  auto operation29 = callbacks.register_callback("getInputNodeIds", [this](LuaEngine& engine, std::size_t i) {
    return node_ids(engine, {WireDirection::Input, i});
  });
  if (!operation29)
    return std::unexpected(std::move(operation29.error()));
  auto operation30 = callbacks.register_callback("getOutputNodePosition", [this](std::size_t i) {
  return node_position(WireNode{WireDirection::Output, i});
});
  if (!operation30)
    return std::unexpected(std::move(operation30.error()));
  auto operation31 = callbacks.register_callback("getOutputNodeLevel", [this](std::size_t i) {
  return node_state(WireNode{WireDirection::Output, i});
});
  if (!operation31)
    return std::unexpected(std::move(operation31.error()));
  auto operation32 = callbacks.register_callback("isOutputNodeConnected", [this](std::size_t i) {
    return node_connected({WireDirection::Output, i});
  });
  if (!operation32)
    return std::unexpected(std::move(operation32.error()));
  auto operation33 = callbacks.register_callback("getOutputNodeIds", [this](LuaEngine& engine, std::size_t i) {
    return node_ids(engine, {WireDirection::Output, i});
  });
  if (!operation33)
    return std::unexpected(std::move(operation33.error()));
  auto operation34 = callbacks.register_callback("setOutputNodeLevel", [this](std::size_t i, bool level) -> std::expected<void, StarException> {
  auto node_result = node({WireDirection::Output, i});
  if (!node_result)
    return std::unexpected(std::move(node_result.error()));
  auto& node = node_result->get();
  node.state.set(level);
  return {};
});
  if (!operation34)
    return std::unexpected(std::move(operation34.error()));
  auto operation35 = callbacks.register_callback("setAllOutputNodes", [this](bool level) {
  for (auto& node : m_output_nodes)
    node.state.set(level);
});
  if (!operation35)
    return std::unexpected(std::move(operation35.error()));
  auto operation36 = callbacks.register_callback("setOfferedQuests", [this](std::optional<JsonArray> const& offered_quests) -> std::expected<void, StarException> {
  List<QuestArcDescriptor> quests;
  for (auto const& entry : offered_quests.value_or(JsonArray{})) {
    auto quest_result = QuestArcDescriptor::fromJson(root(), entry);
    if (!quest_result)
      return std::unexpected(std::move(quest_result.error()));
    auto quest = std::move(*quest_result);
    quests.push_back(std::move(quest));
  }
  m_offered_quests.set(std::move(quests));
  return {};
});
  if (!operation36)
    return std::unexpected(std::move(operation36.error()));
  auto operation37 = callbacks.register_callback("setMaterialSpaces", [this](std::optional<JsonArray> const& new_spaces) -> std::expected<void, StarException> {
  List<MaterialSpace> spaces;
  auto materials_result = root().material_database();
  if (!materials_result)
    return std::unexpected(std::move(materials_result.error()));
  auto materials = std::move(*materials_result);
  for (auto const& space : new_spaces.value_or(JsonArray{})) {
    auto position_config_result = space.get(0);
    if (!position_config_result)
      return std::unexpected(std::move(position_config_result.error()));
    auto position_config = std::move(*position_config_result);
    auto position_result = json_to_vec2i(position_config);
    if (!position_result)
      return std::unexpected(std::move(position_result.error()));
    auto position = std::move(*position_result);
    auto name_result = space.get_string(1);
    if (!name_result)
      return std::unexpected(std::move(name_result.error()));
    auto name = std::move(*name_result);
    auto material_result = materials->material_id(name);
    if (!material_result)
      return std::unexpected(std::move(material_result.error()));
    auto material = std::move(*material_result);
    spaces.push_back({position, material});
  }
  m_material_spaces.set(std::move(spaces));
  return {};
});
  if (!operation37)
    return std::unexpected(std::move(operation37.error()));
  auto operation38 = callbacks.register_callback("setDamageSources", [this](std::optional<JsonArray> damage_sources) -> std::expected<void, StarException> {
  List<DamageSource> sources;
  for (auto const& entry : damage_sources.value_or(JsonArray{})) {
    auto source_result = DamageSource::create(entry);
    if (!source_result)
      return std::unexpected(std::move(source_result.error()));
    auto source = std::move(*source_result);
    sources.push_back(std::move(source));
  }
  m_damage_sources.set(std::move(sources));
  return {};
});
  if (!operation38)
    return std::unexpected(std::move(operation38.error()));
  return callbacks;
}

std::expected<LuaCallbacks, StarException> Object::make_animator_object_callbacks() {
  LuaCallbacks callbacks;
  auto operation1 = callbacks.register_callback("getParameter", [this](String const& name, Json const& def) {
  return config_value(name, def);
});
  if (!operation1)
    return std::unexpected(std::move(operation1.error()));
  auto operation2 = callbacks.register_callback("direction", [this]() {
  return numerical_direction(direction());
});
  if (!operation2)
    return std::unexpected(std::move(operation2.error()));
  auto operation3 = callbacks.register_callback("position", [this]() {
  return position();
});
  if (!operation3)
    return std::unexpected(std::move(operation3.error()));
  return callbacks;
}

std::expected<List<DamageSource>, StarException> Object::damage_sources() const {
  auto const& runtime_sources = m_damage_sources.get();
  auto orientation = current_orientation();
  if (!orientation || (m_config->touch_damage_config.is_null() && orientation->touch_damage_config.is_null()))
    return runtime_sources;

  auto entry = m_touch_damage_templates.find(m_orientation_index);
  if (entry == m_touch_damage_templates.end()) {
    auto parsed = DamageSource::create(json_merge(m_config->touch_damage_config, orientation->touch_damage_config));
    if (!parsed)
      return std::unexpected(std::move(parsed.error()));
    entry = m_touch_damage_templates.try_emplace(m_orientation_index, std::move(*parsed)).first;
  }

  List<DamageSource> sources;
  sources.reserve(runtime_sources.size() + 1);
  sources.append_range(runtime_sources);
  auto& source = sources.emplace_back(entry->second);
  source.source_entity_id = entityId();
  source.team = getTeam();
  return sources;
}

std::expected<List<PersistentStatusEffect>, StarException> Object::status_effects() const {
  return m_config->status_effects;
}

PolyF Object::status_effect_area() const {
  if (auto orientation = current_orientation()) {
    if (orientation->status_effect_area)
      return *orientation->status_effect_area;
  }
  return volume();
}

std::expected<std::optional<HitType>, StarException> Object::query_hit(DamageSource const& source) const {
  if (!m_config->smashable || !inWorld() || m_health.get() <= 0.0f || m_unbreakable)
    return std::nullopt;
  auto current_world_result = world();
  if (!current_world_result)
    return std::unexpected(std::move(current_world_result.error()));
  auto& current_world = current_world_result->get();
  auto poly = volume();
  poly.translate(position());
  auto intersects_result = source.intersects_with_poly(current_world.geometry(), poly);
  if (!intersects_result)
    return std::unexpected(std::move(intersects_result.error()));
  auto intersects = std::move(*intersects_result);
  if (intersects)
    return HitType::Hit;
  return std::nullopt;
}

std::expected<std::optional<PolyF>, StarException> Object::hit_poly() const {
  auto poly = volume();
  poly.translate(position());
  return poly;
}

std::expected<List<DamageNotification>, StarException> Object::apply_damage(DamageRequest const& damage) {
  if (!m_config->smashable || !inWorld() || m_health.get() <= 0.0f)
    return List<DamageNotification>();
  auto notifications_result = recover_lua_script_error(m_script_component.invoke<List<DamageNotification>>("applyDamageRequest", damage));
  if (!notifications_result)
    return std::unexpected(std::move(notifications_result.error()));
  auto notifications = std::move(*notifications_result);
  if (notifications) {
    float total_damage = 0.0f;
    for (auto const& notification : *notifications)
      total_damage += notification.health_lost;
    float dmg = std::min(m_health.get(), total_damage);
    m_health.set(m_health.get() - dmg);
    return std::move(*notifications);
  }
  float dmg = std::min(m_health.get(), damage.damage);
  m_health.set(m_health.get() - dmg);
  return List<DamageNotification>{{damage.source_entity_id, entityId(), position(), damage.damage, dmg, m_health.get() <= 0 ? HitType::Kill : HitType::Hit, damage.damage_source_kind, m_config->damage_material_kind}};
}

RectF Object::interactive_bound_box() const {
  if (auto orientation = current_orientation()) {
    auto rect = RectF(orientation->bound_box);
    rect.set_max(Vec2F(orientation->bound_box.x_max() + 1, orientation->bound_box.y_max() + 1));
    return rect;
  } else {
    return RectF::null();
  }
}

bool Object::is_interactive() const {
  return m_interactive.get();
}

std::expected<InteractAction, StarException> Object::interact(InteractRequest const& request) {
  auto current_world = world();
  if (!current_world)
    return std::unexpected(std::move(current_world.error()));
  auto diff = current_world->get().geometry().diff(request.source_position, position());
  auto result = recover_lua_script_error(m_script_component.invoke<Json>("onInteraction",
      JsonObject{{"source", json_from_vec2f(diff)}, {"sourceId", request.source_id}}));
  if (!result)
    return std::unexpected(std::move(result.error()));
  if (*result) {
    if ((*result)->is_null())
      return {};
    return interact_action_from_script(**result, entityId());
  }
  auto action = config_value("interactAction", Json());
  if (!action)
    return std::unexpected(std::move(action.error()));
  if (action->is_null())
    return {};
  auto type = action->string_ptr();
  if (!type)
    return std::unexpected(std::move(type.error()));
  auto data = config_value("interactData", Json());
  if (!data)
    return std::unexpected(std::move(data.error()));
  return InteractAction::create(**type, entityId(), std::move(*data));
}

List<Vec2I> Object::interactive_spaces() const {
  if (auto orientation = current_orientation()) {
    if (auto i_spaces = orientation->interactive_spaces)
      return *i_spaces;
  }
  return spaces();
}

std::expected<std::optional<LuaValue>, StarException> Object::call_script(String const& func, LuaVariadic<LuaValue> const& args) {
  return recover_lua_script_error(m_script_component.invoke(func, args));
}

std::expected<std::optional<LuaValue>, StarException> Object::eval_script(String const& code) {
  return recover_lua_script_error(m_script_component.eval(code));
}

std::expected<Vec2F, StarException> Object::mouth_position() const {
  if (auto orientation = current_orientation()) {
    auto pos = position() + Vec2F(orientation->bound_box.center()[0], orientation->bound_box.max()[1]);
    if (!(orientation->bound_box.size()[0] % 2))
      pos[0] += 0.5;
    auto config_position_result = config_value("mouthPosition", Json());
    if (!config_position_result)
      return std::unexpected(std::move(config_position_result.error()));
    auto config_position = std::move(*config_position_result);
    if (config_position) {
      auto offset_result = json_to_vec2f(config_position);
      if (!offset_result)
        return std::unexpected(std::move(offset_result.error()));
      auto offset = std::move(*offset_result);
      if (m_direction.get() == Direction::Left)
        offset[0] = -offset[0];
      pos += offset;
    }
    return pos;
  }
  return position();
}

std::expected<Vec2F, StarException> Object::mouth_position(bool) const {
  return mouth_position();
}

List<ChatAction> Object::pull_pending_chat_actions() {
  return std::move(m_pending_chat_actions);
}

std::expected<void, StarException> Object::add_chat_message(String const& message, Json const& config, String const& portrait) {
  if (isSlave())
    return std::unexpected(ObjectException("Cannot add a chat message from a slave object"));
  auto mouth_result = mouth_position();
  if (!mouth_result)
    return std::unexpected(std::move(mouth_result.error()));
  auto mouth = std::move(*mouth_result);
  m_chat_message.set(message);
  m_chat_portrait.set(portrait);
  m_chat_config.set(config);
  m_new_chat_message_event.trigger();
  if (portrait.empty())
    m_pending_chat_actions.push_back(SayChatAction{entityId(), message, mouth});
  else
    m_pending_chat_actions.push_back(PortraitChatAction{entityId(), portrait, message, mouth});
  return {};
}

std::expected<List<Drawable>, StarException> Object::orientation_drawables(std::size_t orientation_index) const {
  if (orientation_index == npos)
    return List<Drawable>();
  auto layers = orientation_layers(orientation_index);
  if (!layers)
    return std::unexpected(std::move(layers.error()));
  auto shake = damage_shake();
  if (!shake)
    return std::unexpected(std::move(shake.error()));
  List<Drawable> drawables(std::from_range, **layers);
  Drawable::translate_all(drawables, get_orientations()[orientation_index]->image_position + *shake);
  return drawables;
}

std::expected<std::shared_ptr<Object::DrawableLayers const>, StarException> Object::orientation_layers(std::size_t orientation_index) const {
  if (m_orientation_drawables_cache && orientation_index == m_orientation_drawables_cache->first)
    return m_orientation_drawables_cache->second;

  auto orientation_entry = get_orientations().at(orientation_index);
  if (!orientation_entry)
    return std::unexpected(std::move(orientation_entry.error()));
  auto const& orientation = *orientation_entry->get();

  auto metadata = root().image_metadata_database();
  if (!metadata)
    return std::unexpected(std::move(metadata.error()));

  std::optional<String> base_color;
  if (!orientation.image_layers.empty() && (m_color_directives || !m_color_suffix.empty())) {
    if (auto color = m_image_keys.find_ref("color")) {
      auto color_end = color->get().find('?');
      if (!color_end)
        return std::unexpected(std::move(color_end.error()));
      auto color_name = color->get().substr(0, *color_end);
      if (!color_name)
        return std::unexpected(std::move(color_name.error()));
      base_color = std::move(*color_name);
    }
  }

  auto generated = std::make_shared<DrawableLayers>();
  generated->reserve(orientation.image_layers.size());
  for (auto const& layer : orientation.image_layers) {
    Drawable drawable = layer;
    auto& image_part = drawable.image_part();
    image_part.image.directives.clear();
    String image_path = AssetPath::join(image_part.image);
    auto const& path = image_path.utf8();
    std::size_t suffix = npos;
    if (base_color) {
      auto lookup_base_tag = [&](String const& key) -> String {
        if (key == "color")
          return *base_color;
        if (auto tag = m_image_keys.find_ref(key))
          return tag->get();
        return "default";
      };
      if (!m_color_suffix.empty() && (suffix = path.rfind("<color>")) != npos)
        image_part.image = String(path.substr(0, (suffix += 7))).lookup_tags(lookup_base_tag);
      else
        image_part.image = image_path.lookup_tags(lookup_base_tag);
    } else {
      image_part.image = image_path.replace_tags(m_image_keys, true, "default");
    }
    image_part.image.directives = layer.image_part().image.directives;
    if (base_color && m_color_directives) {
      if (auto added = image_part.add_directives(**metadata, m_color_directives); !added)
        return std::unexpected(std::move(added.error()));
    }
    if (suffix != npos) {
      auto suffix_directives = m_color_suffix + String(path.substr(suffix)).replace_tags(m_image_keys, true, "default");
      if (auto added = image_part.add_directives(**metadata, std::move(suffix_directives)); !added)
        return std::unexpected(std::move(added.error()));
    }
    if (auto added = image_part.add_directives(**metadata, m_directives); !added)
      return std::unexpected(std::move(added.error()));
    if (orientation.flip_images) {
      auto bounds = drawable.bound_box(**metadata, false);
      if (!bounds)
        return std::unexpected(std::move(bounds.error()));
      drawable.scale(Vec2F(-1, 1), bounds->center() - drawable.position);
    }
    generated->push_back(std::move(drawable));
  }
  std::shared_ptr<DrawableLayers const> layers = std::move(generated);
  m_orientation_drawables_cache.emplace(orientation_index, layers);
  return layers;
}

EntityRenderLayer Object::render_layer() const {
  if (auto orientation = current_orientation())
    return orientation->render_layer;
  else
    return render_layer_object;
}

std::expected<void, StarException> Object::collect_particles(RenderCallback& render_callback, RenderEventCollection& events) {
  if (!inWorld())
    return {};
  auto orientation = current_orientation();
  if (orientation && m_emission_timers.size() != orientation->particle_emitters.size())
    reset_emission_timers();

  using Emission = RenderEventCollection::Emission;
  if (std::holds_alternative<Emission>(events.pending)) {
    auto& emission = std::get<Emission>(events.pending);
    if (auto submitted = render_callback.add_particle(std::move(emission.particle)); !submitted)
      return submitted;
    // A reposition or orientation change can replace timers while a born
    // particle is in flight. Publish it, but never overwrite the new timers.
    if (events.emission_timer_revision != m_emission_timer_revision) {
      events.pending.emplace<std::monostate>();
      events.next_item = 0;
      return {};
    }
    m_emission_timers[events.next_item++] = emission.cooldown;
    events.pending.emplace<std::monostate>();
  }
  if (events.emission_timer_revision != m_emission_timer_revision) {
    events.emission_timer_revision = m_emission_timer_revision;
    events.next_item = 0;
  }
  if (orientation) {
    for (; events.next_item < orientation->particle_emitters.size(); ++events.next_item) {
      auto const& emitter = orientation->particle_emitters[events.next_item];
      auto const& timer = m_emission_timers[events.next_item];
      if (emitter.particle_emission_rate <= 0.0f || !timer || !timer->ready())
        continue;
      if (emitter.place_in_spaces && orientation->spaces.empty())
        return std::unexpected(ObjectException("Cannot emit a particle in an empty object space list"));
      auto particle = emitter.particle;
      particle.applyVariance(root(), emitter.particle_variance);
      if (emitter.place_in_spaces) {
        auto space = root().random().sample_ref(orientation->spaces);
        if (!space)
          return std::unexpected(std::move(space.error()));
        particle.translate(Vec2F(space->get()) + Vec2F(0.5, 0.5));
      }
      particle.translate(position());
      auto variance = root().random().uniform_float(-emitter.particle_emission_rate_variance, emitter.particle_emission_rate_variance);
      if (!variance)
        return std::unexpected(std::move(variance.error()));
      float sampled_rate = emitter.particle_emission_rate + *variance;
      constexpr double min_rate = 1.0 / static_cast<double>(std::numeric_limits<float>::max());
      std::optional<GameTimer> cooldown;
      if (std::abs(static_cast<double>(sampled_rate)) >= min_rate)
        cooldown.emplace(1.0f / sampled_rate);
      if (auto submitted = render_callback.add_particle(std::move(particle)); !submitted) {
        events.pending.emplace<Emission>(std::move(particle), cooldown);
        return submitted;
      }
      m_emission_timers[events.next_item] = cooldown;
    }
  }
  events.next_item = 0;
  return {};
}

std::expected<void, StarException> Object::collect_sounds(RenderCallback& render_callback, RenderEventCollection& events) {
  using Audio = RenderEventCollection::Audio;
  if (!m_sound_effect_enabled.get()) {
    if (m_sound_effect)
      m_sound_effect->stop();
    if (std::holds_alternative<Audio>(events.pending))
      events.pending.emplace<std::monostate>();
    return {};
  }
  if (std::holds_alternative<Audio>(events.pending)) {
    auto& sound = std::get<Audio>(events.pending);
    if (auto submitted = render_callback.add_audio(sound); !submitted)
      return submitted;
    m_sound_effect = std::move(sound);
    events.pending.emplace<std::monostate>();
    return {};
  }
  if (m_config->sound_effect.empty() || (m_sound_effect && !m_sound_effect->finished()))
    return {};
  auto assets = root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto audio = (*assets)->audio(m_config->sound_effect);
  if (!audio)
    return std::unexpected(std::move(audio.error()));
  auto sound_result = AudioInstance::create(**audio);
  if (!sound_result)
    return std::unexpected(std::move(sound_result.error()));
  auto sound = std::move(*sound_result);
  sound->set_loops(-1);
  auto duration = sound->total_time();
  if (!duration)
    return std::unexpected(std::move(duration.error()));
  if (auto sought = sound->seek_time(root().random().uniform_float() * *duration); !sought)
    return sought;
  sound->set_range_multiplier(m_config->sound_effect_range_multiplier);
  sound->set_position(meta_bound_box().center() + position());
  sound->set_volume(0);
  sound->set_volume(1, 1.0);
  if (auto submitted = render_callback.add_audio(sound); !submitted) {
    events.pending.emplace<Audio>(std::move(sound));
    return submitted;
  }
  m_sound_effect = std::move(sound);
  return {};
}

List<std::shared_ptr<ObjectOrientation>> const& Object::get_orientations() const {
  return m_orientations ? *m_orientations : m_config->orientations;
}

std::expected<Vec2F, StarException> Object::damage_shake() const {
  return m_presentation_damage_shake;
}

std::expected<Vec2F, StarException> Object::compute_damage_shake() const {
  if (m_tile_damage_status->damaged() && !m_tile_damage_status->damage_protected()) {
    auto x_result = root().random().uniform_float(-1, 1);
    if (!x_result)
      return std::unexpected(std::move(x_result.error()));
    auto x = std::move(*x_result);
    auto y_result = root().random().uniform_float(-1, 1);
    if (!y_result)
      return std::unexpected(std::move(y_result.error()));
    auto y = std::move(*y_result);
    return Vec2F(x, y) * m_tile_damage_status->damage_effect_percentage() * m_config->damage_shake_magnitude;
  }
  return Vec2F();
}

std::expected<void, StarException> Object::check_liquid_broken() {
  if (m_config->minimum_liquid_level || m_config->maximum_liquid_level) {
    auto level_result = liquid_fill_level();
    if (!level_result)
      return std::unexpected(std::move(level_result.error()));
    auto level = std::move(*level_result);
    if (m_config->minimum_liquid_level && level < *m_config->minimum_liquid_level)
      m_broken = true;
    if (m_config->maximum_liquid_level && level > *m_config->maximum_liquid_level)
      m_broken = true;
  }
  return {};
}

std::expected<Object::Node, StarException> Object::Node::create(Json const& position_config, Json const& config, WireDirection direction) {
  auto position = json_to_vec2i(position_config);
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto color = config.get("color", json_from_color(Color::palette_red)).and_then(json_to_color);
  if (!color)
    return std::unexpected(std::move(color.error()));
  auto icon = config.get_string("icon", direction == WireDirection::Input
    ? String("/interface/wires/inbound.png")
    : String("/interface/wires/outbound.png"));
  if (!icon)
    return std::unexpected(std::move(icon.error()));
  Node node;
  node.position = *position;
  node.color = *color;
  node.icon = std::move(*icon);
  return node;
}
}// namespace Star
