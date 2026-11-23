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
import star.optional;
import star.game_types;
import star.poly;
import star.strong_typedef;
import star.list;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.set;
import star.string;
import star.color;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.periodic_function;
import star.audio;
import star.map;
import star.weighted_pool;
import star.periodic;
import star.lua;
import star.ordered_set;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.lua_root;

import star.lua_game_converters;
import star.lua_converters;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
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
import star.drawable;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
import star.animated_part_set;
import star.animation;
import star.particle;
import star.mixer;
import star.networked_animator;
import star.entity_rendering_types;
import star.stat_collection;
import star.status_effect_database;
import star.status_controller;
import star.status_effect_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.wiring;
import star.wire_entity;
import star.inspectable_entity;
import star.entity_rendering;
import star.object;
import star.container_entity;
import star.container_object;
import star.farmable_object;
import star.uuid;
import star.humanoid;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lounging_entities;
import star.emote_entity;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.inventory_types;
import star.ai_types;
import star.player_types;
import star.radio_message_database;
import star.player;
import star.aggressive_entity;
import star.monster_database;
import star.effect_emitter;
import star.monster;
import star.npc_database;
import star.npc;
import star.asset_source;
import star.assets;
import star.configuration;
import star.root;
import star.stagehand;
import star.vehicle;
import star.projectile;
import star.item_drop;



import star.player_inventory;
import star.item_bag;

namespace Star {

std::expected<LuaMethods<std::shared_ptr<Entity>>, StarException> LuaUserDataMethods<std::shared_ptr<Entity>>::make() {
    LuaMethods<std::shared_ptr<Entity>> methods;

    // general entity methods
    if (auto registration = methods.register_method("exists",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
        return entity->inWorld();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("id",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<EntityId, StarException> {
        return entity->entityId();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("canDamage",
    [&](std::shared_ptr<Entity> const& entity, EntityId const& otherId) -> std::expected<bool, StarException> {
        if (!entity->inWorld())
            return false;
        auto world = entity->world();
        if (!world)
          return std::unexpected(std::move(world.error()));
        auto other = world->get().entity(otherId);
        return other && entity->getTeam().canDamage(other->getTeam(), false);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("damageTeam",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
        return entity->getTeam().toJson();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("aggressive",
                                 [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
                                   if (auto monster = as_ref<Monster>(*entity))
                                     return monster->get().aggressive();
                                   if (auto npc = as_ref<Npc>(*entity))
                                     return npc->get().aggressive();
                                   return false;
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("type",
    [&](std::shared_ptr<Entity> const& entity, LuaEngine& engine) -> std::expected<LuaString, StarException> {
        if (!EntityTypeNames)
            return std::unexpected(EntityTypeNames.error());
        auto name = EntityTypeNames->get_right(entity->entityType());
        if (!name)
          return std::unexpected(std::move(name.error()));
        return engine.create_string(name->get());
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("typeName",
                                 [&](std::shared_ptr<Entity> const& entity,
                                     LuaEngine&) -> std::expected<std::optional<String>, StarException> {
                                   if (auto monster = as_ref<Monster>(*entity))
                                     return monster->get().typeName();
                                   if (auto npc = as_ref<Npc>(*entity))
                                     return npc->get().npcType();
                                   if (auto vehicle = as_ref<Vehicle>(*entity))
                                     return vehicle->get().name();
                                   if (auto object = as_ref<Object>(*entity))
                                     return object->get().name();
                                   if (auto itemDrop = as_ref<ItemDrop>(*entity)) {
                                     if (itemDrop->get().item())
                                       return itemDrop->get().item()->name();
                                   }
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("position",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<Vec2F, StarException> {
        return entity->position();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("metaBoundBox",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<RectF, StarException> {
        return entity->metaBoundBox();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "velocity",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto mobileEntity = as_ref<MobileEntity>(*entity))
              return mobileEntity->get().movementController().velocity();

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("name",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<String, StarException> {
        return entity->name();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "description",
          [&](std::shared_ptr<Entity> const& entity,
              std::optional<String> const& species) -> std::expected<std::optional<String>, StarException> {
            if (auto inspectableEntity = as_ref<InspectableEntity>(*entity)) {
              if (species)
                return inspectableEntity->get().inspectionDescription(*species);
            }

            return entity->description();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "uniqueId",
          [&](std::shared_ptr<Entity> const& entity) -> std::optional<String> { return entity->uniqueId(); });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("getParameter",
    [&](std::shared_ptr<Entity> const& entity, String const& parameterName, std::optional<Json> const& defaultValue) -> std::expected<Json, StarException> {
        std::expected<Json, StarException> val = Json();
        bool handled = true;
        if (auto object = as_ref<Object>(*entity))
          val = object->get().configValue(parameterName);
        else if (auto npc = as_ref<Npc>(*entity))
          val = npc->get().scriptConfigParameter(parameterName);
        else if (auto projectile = as_ref<Projectile>(*entity))
          val = projectile->get().configValue(parameterName);
        else if (auto stagehand = as_ref<Stagehand>(*entity))
          val = stagehand->get().configValue(parameterName);
        else
            handled = false;
        if (!val)
            return std::unexpected(std::move(val.error()));
        if (!*val && defaultValue && handled)
            return *defaultValue;
        return std::move(*val);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("sendMessage",
    [&](std::shared_ptr<Entity> const& entity, String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
        if (entity->inWorld()) {
            auto world = entity->world();
            if (!world)
              return std::unexpected(std::move(world.error()));
            return world->get().sendEntityMessage(entity->entityId(), message, JsonArray::from(std::move(args)));
        }
        return RpcPromise<Json>::make_failed("Entity not in world");
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    // scripted entity methods
    if (auto registration = methods.register_method(
          "callScript",
          [&](std::shared_ptr<Entity> const& entity, String const& function,
              LuaVariadic<LuaValue> const& args) -> std::expected<std::optional<LuaValue>, StarException> {
            auto scrEntity = as_ref<ScriptedEntity>(*entity);
            if (!scrEntity || !scrEntity->get().isMaster() || !entity->inWorld())
              return std::unexpected(StarException::format(
                "Entity {} does not exist or is not a local master scripted entity", entity->entityId()));
            return scrEntity->get().callScript(function, args);
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // nametag entity methods
    if (auto registration = methods.register_method("nametag",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Json>, StarException> {
        Json result;
        if (auto nametagEntity = as_ref<NametagEntity>(*entity)) {
          auto nametag = nametagEntity->get().nametag();
          if (!nametag)
            return std::unexpected(std::move(nametag.error()));
          auto color = nametagEntity->get().nametagColor();
          if (!color)
            return std::unexpected(std::move(color.error()));
          auto origin = nametagEntity->get().nametagOrigin();
          if (!origin)
            return std::unexpected(std::move(origin.error()));
          result = JsonObject{
            {"nametag", std::move(*nametag)},
            {"displayed", nametagEntity->get().displayNametag()},
            {"color", json_from_color(Color::rgb(*color))},
            {"origin", json_from_vec2f(*origin)},
          };
          if (auto status = nametagEntity->get().statusText()) {
            auto setStatus = result.set("status", *status);
            if (!setStatus)
              return std::unexpected(std::move(setStatus.error()));
            result = std::move(*setStatus);
          }
        }

        return result;
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    // portrait entity methods
    if (auto registration = methods.register_method(
          "portrait",
          [&](std::shared_ptr<Entity> const& entity,
              String const& portraitMode) -> std::expected<std::optional<List<Drawable>>, StarException> {
            if (auto portrait = as_ref<PortraitEntity>(*entity)) {
              auto mode = enum_value<portrait_mode_names>(portraitMode);
              if (!mode)
                return std::unexpected(std::move(mode.error()));
              auto drawables = portrait->get().portrait(*mode);
              if (!drawables)
                return std::unexpected(std::move(drawables.error()));
              return std::move(*drawables);
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));


    // damage bar entity methods
    if (auto registration = methods.register_method(
          "health",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto dmgEntity = as_ref<DamageBarEntity>(*entity)) {
              auto maxHealth = dmgEntity->get().maxHealth();
              if (!maxHealth)
                return std::unexpected(std::move(maxHealth.error()));
              return Vec2F(dmgEntity->get().health(), *maxHealth);
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // interactive entity methods
    if (auto registration = methods.register_method(
          "isInteractive",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto interactEntity = as_ref<InteractiveEntity>(*entity))
              return interactEntity->get().isInteractive();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // chatty entity methods
    if (auto registration = methods.register_method(
          "mouthPosition",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto chatty = as_ref<ChattyEntity>(*entity))
              return chatty->get().mouthPosition();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // actor entity methods

    // status controller methods, they're networked anyway so might as well make them available to read
    if (auto registration =
          methods.register_method("statusProperty",
                                 [&](std::shared_ptr<Entity> entity, String name,
                                     Json const& def = Json()) -> std::expected<std::optional<Json>, StarException> {
                                   if (auto actor = as_ref<ActorEntity>(*entity))
                                     return actor->get().statusController().statusProperty(name, def);
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "stat",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().stat(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "statPositive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().statPositive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resourceNames",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<StringList>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourceNames();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resource",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resource(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "isResource",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().isResource(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resourcePositive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourcePositive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resourceLocked",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity)) {
              auto locked = actor->get().statusController().resourceLocked(name);
              if (!locked)
                return std::unexpected(std::move(locked.error()));
              return std::optional<bool>(*locked);
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resourceMax",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourceMax(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "resourcePercentage",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourcePercentage(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "getPersistentEffects",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<JsonArray>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity)) {
              JsonArray effects;
              for (auto const& effect : actor->get().statusController().getPersistentEffects(name)) {
                auto json = jsonFromPersistentStatusEffect(effect);
                if (!json)
                  return std::unexpected(std::move(json.error()));
                effects.push_back(std::move(*json));
              }
              return effects;
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "activeUniqueStatusEffectSummary",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<List<JsonArray>>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().activeUniqueStatusEffectSummary().transformed(
                [](std::pair<UniqueStatusEffect, std::optional<float>> effect) {
                  JsonArray effectJson = {effect.first};
                  if (effect.second)
                    effectJson.push_back(*effect.second);
                  return effectJson;
                });
            ;
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "uniqueStatusEffectActive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().uniqueStatusEffectActive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // movement controller methods, they're networked anyway so might as well make them available to read

    if (auto registration = methods.register_method(
          "mass",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().mass();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "boundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionPoly().bounding_box();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "collisionPoly",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<PolyF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionPoly();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "collisionBody",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<PolyF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionBody();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "collisionBoundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionBody().bounding_box();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "localBoundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().localBoundBox();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "rotation",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().rotation();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "isColliding",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isColliding();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "isNullColliding",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isNullColliding();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "isCollisionStuck",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isCollisionStuck();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "stickingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().stickingDirection();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "liquidPercentage",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().liquidPercentage();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "liquidId",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<std::uint8_t>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().liquidId();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "onGround",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().onGround();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "zeroG",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().zeroG();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "atWorldLimit",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().atWorldLimit();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "anchorState",
          [&](std::shared_ptr<Entity> entity) -> std::expected<LuaVariadic<LuaValue>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              if (auto anchorState = actor->get().movementController().anchor_state())
                return LuaVariadic<LuaValue>{LuaInt(anchorState->entityId), LuaInt(anchorState->positionIndex)};
            return LuaVariadic<LuaValue>();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    // slightly inconsistent for the sake of being more clear what the function is
    if (auto registration = methods.register_method(
          "baseMovementParameters",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<Json>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().base_parameters().to_json();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    // slightly inconsistent for the sake of being more clear what the function is
    if (auto registration = methods.register_method(
          "movementParameters",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<Json>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().parameters().toJson();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "walking",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().walking();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "running",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().running();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "movingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<int>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return numerical_direction(actor->get().movementController().moving_direction());
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "facingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<int>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return numerical_direction(actor->get().movementController().facing_direction());
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "crouching",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().crouching();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "flying",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().flying();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "falling",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().falling();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "canJump",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().can_jump();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "jumping",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().jumping();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "groundMovement",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().ground_movement();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.register_method(
          "liquidMovement",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().liquid_movement();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error())); 

    // tool user entity methods
    if (auto registration = methods.register_method("handItem",
    [&](std::shared_ptr<Entity> const& entity, String const& handName) -> std::expected<std::optional<String>, StarException> {
        ToolHand toolHand;
        if (handName == "primary") {
            toolHand = ToolHand::Primary;
        } else if (handName == "alt") {
            toolHand = ToolHand::Alt;
        } else {
            return std::unexpected(StarException(Star::format("Unknown tool hand {}", handName)));
        }

        if (auto toolUser = as_ref<ToolUserEntity>(*entity)) {
          if (auto item = toolUser->get().handItem(toolHand)) {
            return item->name();
          }
        }

        return {};
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("handItemDescriptor",
    [&](std::shared_ptr<Entity> const& entity, String const& handName) -> std::expected<Json, StarException> {
        ToolHand toolHand;
        if (handName == "primary") {
            toolHand = ToolHand::Primary;
        } else if (handName == "alt") {
            toolHand = ToolHand::Alt;
        } else {
            return std::unexpected(StarException(Star::format("Unknown tool hand {}", handName)));
        }

        if (auto toolUser = as_ref<ToolUserEntity>(*entity)) {
          if (auto item = toolUser->get().handItem(toolHand)) {
            return item->descriptor().toJson();
          }
        }

        return Json();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "aimPosition",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto toolUser = as_ref<ToolUserEntity>(*entity))
              return toolUser->get().aimPosition();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));


    // humanoid entity methods
    if (auto registration = methods.register_method(
          "species",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<String>, StarException> {
            if (auto player = as_ref<Player>(*entity)) {
              return player->get().species();
            } else if (auto npc = as_ref<Npc>(*entity)) {
              return npc->get().species();
            } else {
              return {};
            }
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("gender",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<String>, StarException> {
        std::optional<Gender> gender;
        if (auto player = as_ref<Player>(*entity))
          gender = player->get().gender();
        else if (auto npc = as_ref<Npc>(*entity))
          gender = npc->get().gender();
        if (!gender)
            return {};
        auto name = enum_name<gender_names>(*gender);
        if (!name)
          return std::unexpected(std::move(name.error()));
        return String(*name);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    // player methods
    if (auto registration = methods.register_method(
          "currency",
          [&](std::shared_ptr<Entity> const& entity,
              String const& currencyType) -> std::expected<std::optional<std::uint64_t>, StarException> {
            if (auto player = as_ref<Player>(*entity)) {
              return player->get().currency(currencyType);
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "hasCountOfItem",
          [&](std::shared_ptr<Entity> const& entity, Json descriptor,
              std::optional<bool> exactMatch) -> std::expected<std::optional<std::uint64_t>, StarException> {
            if (auto player = as_ref<Player>(*entity)) {
              auto item = ItemDescriptor::create(descriptor);
              if (!item)
                return std::unexpected(std::move(item.error()));
              return player->get().inventory()->hasCountOfItem(*item, exactMatch.value_or(false));
            }
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // loungeable entity methods
    if (auto registration = methods.register_method("loungingEntities",
    [&](std::shared_ptr<Entity> const& entity, std::optional<std::size_t> anchorIndex) -> std::expected<std::optional<List<EntityId>>, StarException> {
        if (!entity->inWorld())
            return {};
        if (auto loungeable = as_ref<LoungeableEntity>(*entity)) {
          auto entities = loungeable->get().entitiesLoungingIn(anchorIndex.value_or(std::size_t{}));
          if (!entities)
            return std::unexpected(std::move(entities.error()));
          return entities->values();
        }
        return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("loungeableOccupied",
    [&](std::shared_ptr<Entity> const& entity, std::optional<std::size_t> anchorIndex) -> std::expected<std::optional<bool>, StarException> {
        if (!entity->inWorld())
            return {};
        auto loungeable = as_ref<LoungeableEntity>(*entity);
        std::size_t anchor = anchorIndex.value_or(std::size_t{});
        if (loungeable && loungeable->get().anchorCount() > anchor) {
            auto entities = loungeable->get().entitiesLoungingIn(anchor);
            if (!entities)
              return std::unexpected(std::move(entities.error()));
            return !entities->empty();
        }
        return {};
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("loungeableAnchorCount",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<std::size_t>, StarException> {
        if (!entity->inWorld())
            return {};
        if (auto loungeable = as_ref<LoungeableEntity>(*entity))
          return loungeable->get().anchorCount();
        return {};
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    // object methods
    if (auto registration = methods.register_method(
          "objectSpaces",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<List<Vec2I>, StarException> {
            if (auto tileEntity = as_ref<TileEntity>(*entity))
              return tileEntity->get().spaces();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // farmables
    if (auto registration = methods.register_method(
          "farmableStage",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<int>, StarException> {
            if (auto farmable = as_ref<FarmableObject>(*entity)) {
              return farmable->get().stage();
            }

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // containers
    if (auto registration = methods.register_method(
          "containerSize",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<int>, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity))
              return container->get().containerSize();

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerClose",
                                 [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto result = container->get().containerClose();
                                     if (!result)
                                       return std::unexpected(std::move(result.error()));
                                     return true;
                                   }
                                   return false;
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerOpen",
                                 [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto result = container->get().containerOpen();
                                     if (!result)
                                       return std::unexpected(std::move(result.error()));
                                     return true;
                                   }
                                   return false;
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerItems",
                                 [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto itemDb = entity->root().itemDatabase();
                                     if (!itemDb)
                                       return std::unexpected(std::move(itemDb.error()));
                                     JsonArray result;
                                     for (auto const& item : container->get().itemBag()->items())
                                       result.push_back((*itemDb)->toJson(item));
                                     return result;
                                   }
                                   return Json();
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "containerItemAt",
          [&](std::shared_ptr<Entity> const& entity, std::size_t offset) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              auto const& items = container->get().itemBag()->items();
              if (offset < items.size())
                return (*itemDb)->toJson(items[offset]);
            }
            return Json();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerConsume",
                                 [&](std::shared_ptr<Entity> const& entity,
                                     Json const& items) -> std::expected<std::optional<bool>, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto descriptor = ItemDescriptor::create(items);
                                     if (!descriptor)
                                       return std::unexpected(std::move(descriptor.error()));
                                     auto promise = container->get().consumeItems(*descriptor);
                                     if (!promise)
                                       return std::unexpected(std::move(promise.error()));
                                     return promise->result();
                                   }
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("containerConsumeAt",
                                                   [&](std::shared_ptr<Entity> const& entity, std::size_t offset,
                                                       int count) -> std::expected<std::optional<bool>, StarException> {
                                                     if (auto container = as_ref<ContainerObject>(*entity)) {
                                                       if (offset < container->get().containerSize()) {
                                                         auto promise = container->get().consumeItems(offset, count);
                                                         if (!promise)
                                                           return std::unexpected(std::move(promise.error()));
                                                         return promise->result();
                                                       }
                                                     }
                                                     return {};
                                                   });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerAvailable",
                                 [&](std::shared_ptr<Entity> const& entity,
                                     Json const& items) -> std::expected<std::optional<std::size_t>, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto descriptor = ItemDescriptor::create(items);
                                     if (!descriptor)
                                       return std::unexpected(std::move(descriptor.error()));
                                     return container->get().itemBag()->available(*descriptor);
                                   }
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("containerTakeAll",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
        auto itemDb = entity->root().itemDatabase();
        if (!itemDb)
          return std::unexpected(std::move(itemDb.error()));
        if (auto container = as_ref<ContainerObject>(*entity)) {
          auto promise = container->get().clearContainer();
          if (!promise)
            return std::unexpected(std::move(promise.error()));
          auto items = promise->result();
          if (!items)
            return std::unexpected(std::move(items.error()));
          if (*items) {
            JsonArray result;
            for (auto const& item : **items)
              result.push_back((*itemDb)->toJson(item));
            return result;
          }
        }
        return Json();
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "containerTakeAt",
          [&](std::shared_ptr<Entity> const& entity, std::size_t offset) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              if (offset < container->get().containerSize()) {
                auto promise = container->get().takeItems(offset);
                if (!promise)
                  return std::unexpected(std::move(promise.error()));
                auto item = promise->result();
                if (!item)
                  return std::unexpected(std::move(item.error()));
                if (*item)
                  return (*itemDb)->toJson(**item);
              }
            }
            return Json();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("containerTakeNumItemsAt",
                                                   [&](std::shared_ptr<Entity> const& entity, std::size_t offset,
                                                       int const& count) -> std::expected<Json, StarException> {
                                                     if (auto container = as_ref<ContainerObject>(*entity)) {
                                                       auto itemDb = entity->root().itemDatabase();
                                                       if (!itemDb)
                                                         return std::unexpected(std::move(itemDb.error()));
                                                       if (offset < container->get().containerSize()) {
                                                         auto promise = container->get().takeItems(offset, count);
                                                         if (!promise)
                                                           return std::unexpected(std::move(promise.error()));
                                                         auto item = promise->result();
                                                         if (!item)
                                                           return std::unexpected(std::move(item.error()));
                                                         if (*item)
                                                           return (*itemDb)->toJson(**item);
                                                       }
                                                     }
                                                     return Json();
                                                   });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerItemsCanFit",
                                 [&](std::shared_ptr<Entity> const& entity,
                                     Json const& items) -> std::expected<std::optional<std::size_t>, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto itemDb = entity->root().itemDatabase();
                                     if (!itemDb)
                                       return std::unexpected(std::move(itemDb.error()));
                                     auto item = (*itemDb)->fromJson(items);
                                     if (!item)
                                       return std::unexpected(std::move(item.error()));
                                     return container->get().itemBag()->itemsCanFit(optional_ref(*item));
                                   }
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "containerItemsFitWhere",
          [&](std::shared_ptr<Entity> const& entity, Json const& items) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              auto item = (*itemDb)->fromJson(items);
              if (!item)
                return std::unexpected(std::move(item.error()));
              auto result = container->get().itemBag()->itemsFitWhere(optional_ref(*item));
              auto slots = json_from_list<std::size_t>(result.slots);
              if (!slots)
                return std::unexpected(std::move(slots.error()));
              return JsonObject{{"leftover", result.leftover}, {"slots", std::move(*slots)}};
            }
            return Json();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "containerAddItems",
          [&](std::shared_ptr<Entity> const& entity, Json const& items) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              auto item = (*itemDb)->fromJson(items);
              if (!item)
                return std::unexpected(std::move(item.error()));
              auto promise = container->get().addItems(*item);
              if (!promise)
                return std::unexpected(std::move(promise.error()));
              auto result = promise->result();
              if (!result)
                return std::unexpected(std::move(result.error()));
              if (*result)
                return (*itemDb)->toJson(**result);
            }
            return items;
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "containerStackItems",
          [&](std::shared_ptr<Entity> const& entity, Json const& items) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              auto item = (*itemDb)->fromJson(items);
              if (!item)
                return std::unexpected(std::move(item.error()));
              auto promise = container->get().addItems(*item);
              if (!promise)
                return std::unexpected(std::move(promise.error()));
              auto result = promise->result();
              if (!result)
                return std::unexpected(std::move(result.error()));
              if (*result)
                return (*itemDb)->toJson(**result);
            }
            return items;
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("containerPutItemsAt",
                                                   [&](std::shared_ptr<Entity> const& entity, Json const& items,
                                                       std::size_t offset) -> std::expected<Json, StarException> {
                                                     if (auto container = as_ref<ContainerObject>(*entity)) {
                                                       auto itemDb = entity->root().itemDatabase();
                                                       if (!itemDb)
                                                         return std::unexpected(std::move(itemDb.error()));
                                                       auto item = (*itemDb)->fromJson(items);
                                                       if (!item)
                                                         return std::unexpected(std::move(item.error()));
                                                       if (offset < container->get().containerSize()) {
                                                         auto promise = container->get().putItems(offset, *item);
                                                         if (!promise)
                                                           return std::unexpected(std::move(promise.error()));
                                                         auto result = promise->result();
                                                         if (!result)
                                                           return std::unexpected(std::move(result.error()));
                                                         if (*result)
                                                           return (*itemDb)->toJson(**result);
                                                       }
                                                     }
                                                     return items;
                                                   });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.register_method("containerSwapItems",
                                 [&](std::shared_ptr<Entity> const& entity, Json const& items, std::size_t offset,
                                     bool noCombine) -> std::expected<Json, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto itemDb = entity->root().itemDatabase();
                                     if (!itemDb)
                                       return std::unexpected(std::move(itemDb.error()));
                                     auto item = (*itemDb)->fromJson(items);
                                     if (!item)
                                       return std::unexpected(std::move(item.error()));
                                     if (offset < container->get().containerSize()) {
                                       auto promise = container->get().swapItems(offset, *item, !noCombine);
                                       if (!promise)
                                         return std::unexpected(std::move(promise.error()));
                                       auto result = promise->result();
                                       if (!result)
                                         return std::unexpected(std::move(result.error()));
                                       if (*result)
                                         return (*itemDb)->toJson(**result);
                                     }
                                   }
                                   return items;
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method("containerItemApply",
                                                   [&](std::shared_ptr<Entity> const& entity, Json const& items,
                                                       std::size_t offset) -> std::expected<Json, StarException> {
                                                     if (auto container = as_ref<ContainerObject>(*entity)) {
                                                       auto itemDb = entity->root().itemDatabase();
                                                       if (!itemDb)
                                                         return std::unexpected(std::move(itemDb.error()));
                                                       auto item = (*itemDb)->fromJson(items);
                                                       if (!item)
                                                         return std::unexpected(std::move(item.error()));
                                                       if (offset < container->get().containerSize()) {
                                                         auto promise = container->get().applyAugment(offset, *item);
                                                         if (!promise)
                                                           return std::unexpected(std::move(promise.error()));
                                                         auto result = promise->result();
                                                         if (!result)
                                                           return std::unexpected(std::move(result.error()));
                                                         if (*result)
                                                           return (*itemDb)->toJson(**result);
                                                       }
                                                     }
                                                     return items;
                                                   });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "movingCollisionCount",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::size_t, StarException> {
            if (auto phys = as_ref<PhysicsEntity>(*entity)) {
              return phys->get().movingCollisionCount();
            }

            return 0;
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.register_method(
          "movingCollision",
          [&](std::shared_ptr<Entity> const& entity,
              std::size_t index) -> std::expected<std::optional<PhysicsMovingCollision>, StarException> {
            if (auto phys = as_ref<PhysicsEntity>(*entity)) {
              return phys->get().movingCollision(index);
            }

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    return methods;
} 

}
