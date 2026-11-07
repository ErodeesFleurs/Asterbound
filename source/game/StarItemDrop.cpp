import star.json_extra;
import star.json;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.audio;
import star.list;
import star.map;
import star.set;
import star.color;
import star.string;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.data_stream_extra;
import star.lua;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.ordered_set;
import star.either;
import star.periodic_function;
import star.optional;

import star.lua_root;

import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
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
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.mobile_entity;
import star.scripted_entity;
import star.item_drop;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;
import star.uuid;
import star.animated_part_set;
import star.networked_animator;
import star.humanoid;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inspectable_entity;
import star.inventory_types;
import star.ai_types;
import star.player_types;
import star.radio_message_database;
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

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::createRandomizedDrop(Root& rootContext, std::shared_ptr<Item> const& item, Vec2F const& position, bool eternal) {
  if (!item)
    return std::shared_ptr<ItemDrop>();
  auto assets = rootContext.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/itemdrop.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto drop = create(rootContext, item);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  auto distance = config->getFloat("randomizedDistance");
  if (!distance)
    return std::unexpected(std::move(distance.error()));
  auto offset = Vec2F(*distance, 0).rotate(std::numbers::pi * 2.0 * rootContext.random().randf());
  offset[1] = std::fabs(offset[1]);
  (*drop)->setPosition(position + offset / TilePixels);
  auto speed = config->getFloat("randomizedSpeed");
  if (!speed)
    return std::unexpected(std::move(speed.error()));
  (*drop)->setVelocity(offset * *speed);
  (*drop)->setEternal(eternal);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::createRandomizedDrop(Root& rootContext, ItemDescriptor const& descriptor, Vec2F const& position, bool eternal) {
  if (!descriptor || descriptor.isEmpty())
    return std::shared_ptr<ItemDrop>();
  auto database = rootContext.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  auto drop = createRandomizedDrop(rootContext, *item, position, false);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  (*drop)->setEternal(eternal);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::throwDrop(Root& rootContext, std::shared_ptr<Item> const& item, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal) {
  if (!item)
    return std::shared_ptr<ItemDrop>();
  auto assets = rootContext.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/itemdrop.config");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto drop = create(rootContext, item);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  (*drop)->setPosition(position);
  if (direction != Vec2F()) {
    auto speed = config->getFloat("throwSpeed");
    if (!speed)
      return std::unexpected(std::move(speed.error()));
    (*drop)->setVelocity(velocity + vnorm(direction) * *speed);
  }
  (*drop)->setEternal(eternal);
  auto intangibleTime = config->getFloat("throwIntangibleTime");
  if (!intangibleTime)
    return std::unexpected(std::move(intangibleTime.error()));
  (*drop)->setIntangibleTime(*intangibleTime);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::throwDrop(Root& rootContext, ItemDescriptor const& descriptor, Vec2F const& position, Vec2F const& velocity, Vec2F const& direction, bool eternal) {
  if (!descriptor || descriptor.isEmpty())
    return std::shared_ptr<ItemDrop>();
  auto database = rootContext.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  auto drop = throwDrop(rootContext, *item, position, velocity, direction, false);
  if (!drop)
    return std::unexpected(std::move(drop.error()));
  (*drop)->setEternal(eternal);
  return drop;
}

std::expected<void, StarException> ItemDrop::initializeItem(std::shared_ptr<Item> item) {
  if (!item)
    return std::unexpected(ItemException("Cannot construct an item drop without an item"));
  auto parameters = item->instanceValueOfType("itemDrop", Json::Type::Object, JsonObject{});
  if (!parameters)
    return std::unexpected(std::move(parameters.error()));
  m_item = std::move(item);
  m_parameters = std::move(*parameters);
  m_itemDescriptor.set(m_item->descriptor());
  auto modeName = configValue("clientEntityMode", "ClientSlaveOnly").and_then([](Json const& value) { return value.toString(); });
  if (!modeName)
    return std::unexpected(std::move(modeName.error()));
  if (!ClientEntityModeNames)
    return std::unexpected(ClientEntityModeNames.error());
  auto mode = ClientEntityModeNames->get_left(*modeName);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  m_clientEntityMode = mode->get();
  return {};
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& rootContext, std::shared_ptr<Item> item) {
  auto drop = std::shared_ptr<ItemDrop>(new ItemDrop(rootContext));
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto initialized = drop->initializeItem(std::move(item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  drop->m_owningEntity.set(NullEntityId);
  drop->m_mode.set(Mode::Available);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& rootContext, Json const& store) {
  auto drop = std::shared_ptr<ItemDrop>(new ItemDrop(rootContext));
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  auto database = rootContext.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto itemStore = store.get("item");
  if (!itemStore)
    return std::unexpected(std::move(itemStore.error()));
  auto item = (*database)->diskLoad(*itemStore);
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (auto initialized = drop->initializeItem(std::move(*item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  auto position = store.get("position").and_then([](Json const& value) { return jsonToVec2F(value); });
  if (!position)
    return std::unexpected(std::move(position.error()));
  drop->m_movementController->setPosition(*position);
  auto modeName = store.getString("mode");
  if (!modeName)
    return std::unexpected(std::move(modeName.error()));
  if (!ModeNames)
    return std::unexpected(ModeNames.error());
  auto mode = ModeNames->get_left(*modeName);
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  drop->m_mode.set(mode->get());
  auto eternal = store.getBool("eternal");
  if (!eternal)
    return std::unexpected(std::move(eternal.error()));
  drop->m_eternal = *eternal;
  auto age = store.get("dropAge").and_then([](Json const& value) { return EpochTimer::create(value); });
  if (!age)
    return std::unexpected(std::move(age.error()));
  drop->m_dropAge = std::move(*age);
  auto aging = store.get("ageItemsTimer").and_then([](Json const& value) { return EpochTimer::create(value); });
  if (!aging)
    return std::unexpected(std::move(aging.error()));
  drop->m_ageItemsTimer = std::move(*aging);
  drop->m_owningEntity.set(NullEntityId);
  return drop;
}

std::expected<std::shared_ptr<ItemDrop>, StarException> ItemDrop::create(Root& rootContext, ByteArray store, NetCompatibilityRules rules) {
  auto drop = std::shared_ptr<ItemDrop>(new ItemDrop(rootContext));
  if (auto initialized = drop->initialize(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  DataStreamBuffer ds(std::move(store));
  ds.setStreamCompatibilityVersion(rules);
  auto descriptor = ds.read<ItemDescriptor>();
  if (!descriptor)
    return std::unexpected(std::move(descriptor.error()));
  auto database = rootContext.itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto item = (*database)->item(*descriptor);
  if (!item)
    return std::unexpected(std::move(item.error()));
  if (auto initialized = drop->initializeItem(std::move(*item)); !initialized)
    return std::unexpected(std::move(initialized.error()));
  if (auto read = ds.read(drop->m_eternal); !read)
    return std::unexpected(std::move(read.error()));
  if (auto read = ds.read(drop->m_dropAge); !read)
    return std::unexpected(std::move(read.error()));
  if (auto read = ds.read(drop->m_intangibleTimer); !read)
    return std::unexpected(std::move(read.error()));
  return drop;
}

std::expected<Json, StarException> ItemDrop::diskStore() const {
  auto database = root().itemDatabase();
  if (!database)
    return std::unexpected(std::move(database.error()));
  auto stored = (*database)->diskStore(m_item);
  if (!stored)
    return std::unexpected(std::move(stored.error()));
  if (!ModeNames)
    return std::unexpected(ModeNames.error());
  auto mode = ModeNames->get_right(m_mode.get());
  if (!mode)
    return std::unexpected(std::move(mode.error()));
  return Json(JsonObject{
    {"item", std::move(*stored)},
    {"position", jsonFromVec2F(m_movementController->position())},
    {"mode", mode->get()},
    {"eternal", m_eternal},
    {"dropAge", m_dropAge.toJson()},
    {"ageItemsTimer", m_ageItemsTimer.toJson()}
  });
}

std::expected<ByteArray, StarException> ItemDrop::netStore(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.setStreamCompatibilityVersion(rules);
  if (auto written = ds.write(itemSafeDescriptor(m_item)); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_eternal); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_dropAge); !written)
    return std::unexpected(std::move(written.error()));
  if (auto written = ds.write(m_intangibleTimer); !written)
    return std::unexpected(std::move(written.error()));
  return ds.takeData();
}

EntityType ItemDrop::entityType() const {
  return EntityType::ItemDrop;
}

std::expected<void, StarException> ItemDrop::init(World& world, EntityId entityId, EntityMode mode) {
  auto initialized = Entity::init(world, entityId, mode);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  m_movementController->init(world);
  auto setup = [&]() -> std::expected<void, StarException> {
    if (!isMaster())
      return {};
    auto scripts = configValue("scripts").and_then([](Json const& value) { return value.optArray(); });
    if (!scripts)
      return std::unexpected(std::move(scripts.error()));
    if (!*scripts)
      return {};
    auto scriptPaths = jsonToStringList(Json(**scripts));
    if (!scriptPaths)
      return std::unexpected(std::move(scriptPaths.error()));
    if (scriptPaths->empty())
      return {};
    if (auto set = m_scriptComponent.setScripts(std::move(*scriptPaths)); !set)
      return std::unexpected(std::move(set.error()));
    auto delta = configValue("scriptDelta", 1).and_then([](Json const& value) { return value.toUInt(); });
    if (!delta)
      return std::unexpected(std::move(delta.error()));
    m_scriptComponent.setUpdateDelta(*delta);
    auto add = [&](String name, std::expected<LuaCallbacks, StarException> callbacks) -> std::expected<void, StarException> {
      if (!callbacks)
        return std::unexpected(std::move(callbacks.error()));
      return m_scriptComponent.addCallbacks(std::move(name), std::move(*callbacks));
    };
    if (auto added = add("itemDrop", makeItemDropCallbacks()); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("item", LuaBindings::makeItemCallbacks(*m_item)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("config", LuaBindings::makeConfigCallbacks([this](String const& name, Json const& def) { return configValue(name, def); })); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("entity", LuaBindings::makeEntityCallbacks(*this)); !added)
      return std::unexpected(std::move(added.error()));
    if (auto added = add("mcontroller", LuaBindings::makeMovementControllerCallbacks(*m_movementController)); !added)
      return std::unexpected(std::move(added.error()));
    return m_scriptComponent.init(world);
  }();
  if (!setup) {
    auto cleaned = uninit();
    if (!cleaned)
      return std::unexpected(cleaned.error().withContext(printException(setup.error(), true)));
  }
  return setup;
}

ClientEntityMode ItemDrop::clientEntityMode() const {
  return m_clientEntityMode;
}

std::expected<void, StarException> ItemDrop::uninit() {
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (error)
        error = result.error().withContext(printException(*error, true));
      else
        error = std::move(result.error());
    }
  };
  if (isMaster()) {
    remember(m_scriptComponent.uninit());
    remember(m_scriptComponent.removeCallbacks("itemDrop"));
    remember(m_scriptComponent.removeCallbacks("item"));
    remember(m_scriptComponent.removeCallbacks("config"));
    remember(m_scriptComponent.removeCallbacks("entity"));
    remember(m_scriptComponent.removeCallbacks("mcontroller"));
  }
  m_movementController->uninit();
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

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> ItemDrop::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  return m_netGroup.writeNetState(fromVersion, rules);
}

std::expected<void, StarException> ItemDrop::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  return m_netGroup.readNetState(data, interpolationTime, rules);
}

std::expected<void, StarException> ItemDrop::enableInterpolation(float extrapolationHint) {
  if (auto enabled = m_netGroup.enableNetInterpolation(extrapolationHint); !enabled)
    return std::unexpected(std::move(enabled.error()));
  if (auto disabled = m_mode.disableNetInterpolation(); !disabled)
    return std::unexpected(std::move(disabled.error()));
  return m_owningEntity.disableNetInterpolation();
}

std::expected<void, StarException> ItemDrop::disableInterpolation() {
  return m_netGroup.disableNetInterpolation();
}

Vec2F ItemDrop::position() const {
  return m_movementController->position();
}

RectF ItemDrop::metaBoundBox() const {
  return m_boundBox;
}

bool ItemDrop::ephemeral() const {
  return true;
}

RectF ItemDrop::collisionArea() const {
  return m_boundBox;
}

std::expected<void, StarException> ItemDrop::update(float dt, std::uint64_t) {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  auto& world = currentWorld->get();
  m_dropAge.update(world.epochTime());
  if (isMaster()) {
    auto updated = m_scriptComponent.update(m_scriptComponent.updateDt(dt));
    if (!updated)
      return std::unexpected(std::move(updated.error()));
    if (m_owningEntity.get() != NullEntityId) {
      if (auto taken = updateTaken(true); !taken)
        return std::unexpected(std::move(taken.error()));
    } else {
      auto takeable = canTake();
      if (!takeable)
        return std::unexpected(std::move(takeable.error()));
      if (*takeable && m_mode.get() == Mode::Available && root().random().randf() < m_combineChance) {
        auto found = world.findEntity(RectF::withCenter(position(), Vec2F::filled(m_combineRadius)),
            [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
          auto closeDropRef = asRef<ItemDrop>(*entity);
          if (!closeDropRef)
            return false;
          auto& closeDrop = closeDropRef->get();
          if (std::addressof(closeDrop) == this)
            return false;
          auto closeTakeable = closeDrop.canTake();
          if (!closeTakeable)
            return std::unexpected(std::move(closeTakeable.error()));
          if (!*closeTakeable || vmag(position() - closeDrop.position()) >= m_combineRadius || !closeDrop.isMaster())
            return false;
          auto const& closeItem = closeDrop.m_item;
          if (m_item->couldStack(optionalRef(closeItem)) != closeItem->count())
            return false;
          auto taken = closeDrop.take();
          if (!taken)
            return std::unexpected(std::move(taken.error()));
          m_item->stackWith(optionalRef(*taken));
          m_dropAge.setElapsedTime(std::min(m_dropAge.elapsedTime(), closeDrop.m_dropAge.elapsedTime()));
          m_movementController->setPosition(m_movementController->position()
              + world.geometry().diff(closeDrop.position(), m_movementController->position()) / 2.0f);
          m_movementController->setVelocity((m_movementController->velocity() + closeDrop.velocity()) / 2.0f);
          return true;
        });
        if (!found)
          return std::unexpected(std::move(found.error()));
      }
      MovementParameters parameters;
      parameters.collisionEnabled = true;
      parameters.gravityEnabled = true;
      if (auto applied = m_movementController->applyParameters(parameters); !applied)
        return std::unexpected(std::move(applied.error()));
    }
    if (auto ticked = m_movementController->tickMaster(dt); !ticked)
      return std::unexpected(std::move(ticked.error()));
    m_intangibleTimer.tick(dt);
    m_ageItemsTimer.update(world.epochTime());
    if ((m_mode.get() == Mode::Intangible || m_mode.get() == Mode::Available) && m_movementController->atWorldLimit())
      m_mode.set(Mode::Dead);
    if (m_mode.get() == Mode::Intangible && m_intangibleTimer.ready())
      m_mode.set(Mode::Available);
    if (!m_eternal && m_mode.get() == Mode::Available && m_dropAge.elapsedTime() > m_item->timeToLive())
      m_mode.set(Mode::Dead);
    if (m_mode.get() == Mode::Taken && m_dropAge.elapsedTime() > m_afterTakenLife)
      m_mode.set(Mode::Dead);
    if (m_overrideMode)
      m_mode.set(*m_overrideMode);
    if (m_clientEntityMode != ClientEntityMode::ClientSlaveOnly && m_mode.get() == Mode::Available)
      m_mode.set(Mode::Intangible);
    if (m_mode.get() <= Mode::Available && m_ageItemsTimer.elapsedTime() > m_ageItemsEvery) {
      auto database = root().itemDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto aged = (*database)->ageItem(m_item, m_ageItemsTimer.elapsedTime());
      if (!aged)
        return std::unexpected(std::move(aged.error()));
      if (*aged)
        m_itemDescriptor.set(m_item->descriptor());
      m_ageItemsTimer.setElapsedTime(0.0);
    }
  } else {
    if (m_itemDescriptor.pullUpdated()) {
      auto database = root().itemDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto loaded = (*database)->loadItem(m_itemDescriptor.get(), m_item);
      if (!loaded)
        return std::unexpected(std::move(loaded.error()));
      if (!m_item)
        return std::unexpected(ItemException("Item drop network state contains no item"));
    }
    if (auto ticked = m_netGroup.tickNetInterpolation(dt); !ticked)
      return std::unexpected(std::move(ticked.error()));
    if (m_owningEntity.get() != NullEntityId) {
      if (m_dropAge.elapsedTime() > 1.0f) {
        m_owningEntity.set(NullEntityId);
      } else {
        if (auto taken = updateTaken(false); !taken)
          return std::unexpected(std::move(taken.error()));
        if (auto ticked = m_movementController->tickMaster(dt); !ticked)
          return std::unexpected(std::move(ticked.error()));
      }
    } else if (auto ticked = m_movementController->tickSlave(dt); !ticked) {
      return std::unexpected(std::move(ticked.error()));
    }
  }
  if (world.isClient()) {
    auto takeable = canTake();
    if (!takeable)
      return std::unexpected(std::move(takeable.error()));
    root().spatialLogger().logPoly("world", m_movementController->collisionBody(), (*takeable ? Color::Green : Color::Red).toRgba());
  }
  return {};
}

std::expected<bool, StarException> ItemDrop::shouldDestroy() const {
  return m_mode.get() == Mode::Dead || (m_item->empty() && m_owningEntity.get() == NullEntityId);
}

std::uint64_t ItemDrop::presentationEpoch() const {
  return Entity::presentationEpoch() + m_movementController->presentationEpoch();
}

std::expected<void, StarException> ItemDrop::render(RenderCallback& renderCallback) {
  if (m_mode.get() != Mode::Taken && m_drawRarityBeam) {
    Color beamColor;
    switch (m_item->rarity()) {
    case Rarity::Uncommon:
      beamColor = Color::rgb(87, 255, 81);
      break;
    case Rarity::Rare:
      beamColor = Color::rgb(87, 220, 255);
      break;
    case Rarity::Legendary:
      beamColor = Color::rgb(176, 81, 255);
      break;
    case Rarity::Essential:
      beamColor = Color::rgb(255, 255, 81);
      break;
    default:
      beamColor = Color::White;
    }

    beamColor.setAlphaF(0.8f);
    Line2F line = { Vec2F(), Vec2F(0.0f, 1.0f + m_boundBox.height() / 2) };
    float width = std::min(2.0f, m_boundBox.width() * TilePixels);
    auto drawable = Drawable::makeLine(line, width, beamColor, position());
    (drawable.linePart().endColor = beamColor)->setAlphaF(0.0f);
    drawable.fullbright = true;
    if (auto added = renderCallback.addDrawable(std::move(drawable), RenderLayerItemDrop); !added)
      return std::unexpected(std::move(added.error()));
  }

  if (!m_drawables) {
    auto mat = asRef<MaterialItem>(*m_item);
    List<Drawable> generated;
    if (mat) {
      auto preview = mat->get().generatedPreview(renderCallback.tileDrawer(), Vec2I(position().floor()));
      if (!preview)
        return std::unexpected(std::move(preview.error()));
      generated = preview->get();
    } else {
      auto drawables = m_item->dropDrawables(renderCallback.tileDrawer());
      if (!drawables)
        return std::unexpected(std::move(drawables.error()));
      generated = std::move(*drawables);
    }
    auto directives = m_config.getString("directives", "");
    if (!directives)
      return std::unexpected(std::move(directives.error()));
    if (Directives dropDirectives = *directives) {
      auto metadata = root().imageMetadataDatabase();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      for (auto& drawable : generated) {
        if (drawable.isImage()) {
          auto added = drawable.imagePart().addDirectives(**metadata, dropDirectives, true);
          if (!added)
            return std::unexpected(std::move(added.error()));
        }
      }
    }
    m_drawables = std::move(generated);
    if (mat)
      m_overForeground = true;
  }
  EntityRenderLayer renderLayer = m_mode.get() == Mode::Taken || m_overForeground ? RenderLayerForegroundTile : RenderLayerItemDrop;
  Vec2F dropPosition = position();
  for (Drawable drawable : *m_drawables) {
    drawable.position += dropPosition;
    if (auto added = renderCallback.addDrawable(std::move(drawable), renderLayer); !added)
      return std::unexpected(std::move(added.error()));
  }
  return {};
}

std::expected<void, StarException> ItemDrop::renderLightSources(RenderCallback& renderCallback) {
  LightSource light;
  light.type = LightType::Spread;
  light.color = Vec3F::filled(20.f / 255.f);
  light.position = position();
  return renderCallback.addLightSource(std::move(light));
}

MovementController& ItemDrop::movementController() {
  return *m_movementController;
}

std::shared_ptr<Item> ItemDrop::item() const {
  return m_item;
}

void ItemDrop::setEternal(bool eternal) {
  m_eternal = eternal;
}

void ItemDrop::setIntangibleTime(float intangibleTime) {
  m_intangibleTimer = GameTimer(intangibleTime);
  if (m_mode.get() == Mode::Available)
    m_mode.set(Mode::Intangible);
}

std::expected<bool, StarException> ItemDrop::canTake() const {
  if (m_mode.get() != Mode::Available || m_owningEntity.get() != NullEntityId || m_item->empty())
    return false;
  if (isMaster()) {
    auto result = m_scriptComponent.invoke<bool>("canTake");
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (*result)
      return **result;
  }
  return true;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDrop::takeBy(EntityId entityId, float timeOffset) {
  auto takeable = canTake();
  if (!takeable)
    return std::unexpected(std::move(takeable.error()));
  if (!*takeable)
    return std::shared_ptr<Item>();
  auto taken = m_item->take();
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  m_owningEntity.set(entityId);
  m_dropAge.setElapsedTime(timeOffset);
  m_mode.set(Mode::Taken);
  setPersistent(false);
  return taken;
}

std::expected<std::shared_ptr<Item>, StarException> ItemDrop::take() {
  auto takeable = canTake();
  if (!takeable)
    return std::unexpected(std::move(takeable.error()));
  if (!*takeable)
    return std::shared_ptr<Item>();
  auto taken = m_item->take();
  if (!taken)
    return std::unexpected(std::move(taken.error()));
  m_mode.set(Mode::Taken);
  return taken;
}

void ItemDrop::setPosition(Vec2F const& position) {
  resetPresentation();
  m_movementController->setPosition(position);
}

Vec2F ItemDrop::velocity() const {
  return m_movementController->velocity();
}

void ItemDrop::setVelocity(Vec2F const& velocity) {
  m_movementController->setVelocity(velocity);
}

std::expected<EnumMap<ItemDrop::Mode>, StarException> const ItemDrop::ModeNames = EnumMap<Mode>::create({
    {ItemDrop::Mode::Intangible, "Intangible"},
    {ItemDrop::Mode::Available, "Available"},
    {ItemDrop::Mode::Taken, "Taken"},
    {ItemDrop::Mode::Dead, "Dead"}});
    
std::expected<Json, StarException> ItemDrop::configValue(String const& name, Json const& def) const {
  auto config = m_config.query(name, def);
  if (!config)
    return std::unexpected(std::move(config.error()));
  return m_parameters.query(name, std::move(*config));
}

ItemDrop::ItemDrop(Root& rootContext) : Entity(rootContext), m_parameters(JsonObject{}), m_scriptComponent(rootContext) {
  setPersistent(true);
}

std::expected<void, StarException> ItemDrop::initialize() {
  auto controller = MovementController::create(root());
  if (!controller)
    return std::unexpected(std::move(controller.error()));
  m_movementController = std::move(*controller);
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
  if (!parameters->physicsEffectCategories)
    parameters->physicsEffectCategories = StringSet({"itemdrop"});
  if (parameters->collisionPoly)
    m_boundBox = parameters->collisionPoly->boundBox();
  else
    m_boundBox = RectF{-0.499, -0.499, 0.499, 0.499};
  if (auto applied = m_movementController->applyParameters(*parameters); !applied)
    return std::unexpected(std::move(applied.error()));
  if (auto added = m_netGroup.addNetElement(m_mode); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_netGroup.addNetElement(m_owningEntity); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_netGroup.addNetElement(*m_movementController); !added)
    return std::unexpected(std::move(added.error()));
  if (auto added = m_netGroup.addNetElement(m_itemDescriptor); !added)
    return std::unexpected(std::move(added.error()));
  auto loadFloat = [&](String const& key, float& target) -> std::expected<void, StarException> {
    auto value = m_config.getFloat(key);
    if (!value)
      return std::unexpected(std::move(value.error()));
    target = *value;
    return {};
  };
  if (auto loaded = loadFloat("afterTakenLife", m_afterTakenLife); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("overheadTime", m_overheadTime); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("pickupDistance", m_pickupDistance); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("velocity", m_velocity); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("velocityApproach", m_velocityApproach); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("overheadApproach", m_overheadApproach); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto overheadDistance = m_config.getFloat("overheadRandomizedDistance");
  if (!overheadDistance)
    return std::unexpected(std::move(overheadDistance.error()));
  m_overheadOffset = Vec2F(*overheadDistance, 0).rotate(std::numbers::pi * 2.0 * root().random().randf());
  if (auto loaded = loadFloat("combineChance", m_combineChance); !loaded)
    return std::unexpected(std::move(loaded.error()));
  if (auto loaded = loadFloat("combineRadius", m_combineRadius); !loaded)
    return std::unexpected(std::move(loaded.error()));
  auto ageEvery = m_config.getDouble("ageItemsEvery", 10);
  if (!ageEvery)
    return std::unexpected(std::move(ageEvery.error()));
  m_ageItemsEvery = *ageEvery;
  auto rarityBeam = m_config.getBool("drawRarityBeam", false);
  if (!rarityBeam)
    return std::unexpected(std::move(rarityBeam.error()));
  m_drawRarityBeam = *rarityBeam;
  m_eternal = false;
  m_overForeground = false;
  m_clientEntityMode = ClientEntityMode::ClientSlaveOnly;
  return {};
}


std::expected<void, StarException> ItemDrop::updateTaken(bool master) {
  auto currentWorld = world();
  if (!currentWorld)
    return std::unexpected(std::move(currentWorld.error()));
  auto& world = currentWorld->get();
  if (auto owningEntity = world.entity(m_owningEntity.get())) {
    Vec2F position = m_movementController->position();
    bool overhead = m_dropAge.elapsedTime() < m_overheadTime;
    Vec2F targetPosition = owningEntity->position();
    if (overhead) {
      targetPosition += m_overheadOffset;
      auto rect = owningEntity->collisionArea();
      if (!rect.isNull())
        targetPosition[1] += rect.yMax() + 1.5f;
      else
        targetPosition[1] += 1.5f;
    }
    Vec2F diff = world.geometry().diff(targetPosition, position);
    float magnitude = diff.magnitude();
    Vec2F velocity = diff.normalized() * m_velocity * std::min(1.0f, magnitude);
    if (auto playerEntity = asRef<Player>(*owningEntity))
      velocity += playerEntity->get().velocity();
    m_movementController->approachVelocity(velocity, overhead ? m_overheadApproach : m_velocityApproach);
    if (master && !overhead && magnitude < m_pickupDistance)
      m_mode.set(Mode::Dead);

  } else if (master) {
    // Our owning entity left, disappear quickly
    m_mode.set(Mode::Dead);
  }

  MovementParameters parameters;
  parameters.maxMovementPerStep = 1000.0f;
  parameters.collisionEnabled = false;
  parameters.gravityEnabled = false;
  return m_movementController->applyParameters(parameters);
}

std::expected<std::optional<LuaValue>, StarException> ItemDrop::callScript(String const& func, LuaVariadic<LuaValue> const& args) {
  return m_scriptComponent.invoke(func, args);
}

std::expected<std::optional<LuaValue>, StarException> ItemDrop::evalScript(String const& code) {
  return m_scriptComponent.eval(code);
}

std::expected<LuaCallbacks, StarException> ItemDrop::makeItemDropCallbacks() {
  LuaCallbacks callbacks;
  if (auto registered = callbacks.registerCallback("takingEntity", [this]() -> std::optional<EntityId> {
    if (m_owningEntity.get() == NullEntityId)
      return {};
    return m_owningEntity.get();
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setEternal", [this](bool eternal) { setEternal(eternal); }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("eternal", [this]() { return m_eternal; }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setIntangibleTime", [this](float time) { setIntangibleTime(time); }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("intangibleTime", [this]() { return m_intangibleTimer.timer; }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setOverrideMode", [this](std::optional<String> const& mode) -> std::expected<void, StarException> {
    if (!mode) {
      m_overrideMode.reset();
      return {};
    }
    if (!ModeNames)
      return std::unexpected(ModeNames.error());
    auto value = ModeNames->get_left(*mode);
    if (!value)
      return std::unexpected(std::move(value.error()));
    m_overrideMode = value->get();
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("overrideMode", [this]() -> std::expected<std::optional<String>, StarException> {
    if (!m_overrideMode)
      return std::optional<String>();
    if (!ModeNames)
      return std::unexpected(ModeNames.error());
    auto value = ModeNames->get_right(*m_overrideMode);
    if (!value)
      return std::unexpected(std::move(value.error()));
    return std::optional<String>(value->get());
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  return callbacks;
}

}
