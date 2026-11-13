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
import star.observer_stream;
import star.either;
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
import star.version;

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
    if (auto registration = methods.registerMethod("exists",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<bool, StarException> {
        return entity->inWorld();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("id",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<EntityId, StarException> {
        return entity->entityId();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("canDamage",
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

    if (auto registration = methods.registerMethod("damageTeam",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
        return entity->getTeam().toJson();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.registerMethod("aggressive",
                                 [&](std::shared_ptr<Entity> const& entity) -> std::expected<Json, StarException> {
                                   if (auto monster = as_ref<Monster>(*entity))
                                     return monster->get().aggressive();
                                   if (auto npc = as_ref<Npc>(*entity))
                                     return npc->get().aggressive();
                                   return false;
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("type",
    [&](std::shared_ptr<Entity> const& entity, LuaEngine& engine) -> std::expected<LuaString, StarException> {
        if (!EntityTypeNames)
            return std::unexpected(EntityTypeNames.error());
        auto name = EntityTypeNames->get_right(entity->entityType());
        if (!name)
          return std::unexpected(std::move(name.error()));
        return engine.createString(name->get());
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.registerMethod("typeName",
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

    if (auto registration = methods.registerMethod("position",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<Vec2F, StarException> {
        return entity->position();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("metaBoundBox",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<RectF, StarException> {
        return entity->metaBoundBox();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
          "velocity",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto mobileEntity = as_ref<MobileEntity>(*entity))
              return mobileEntity->get().movementController().velocity();

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("name",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<String, StarException> {
        return entity->name();
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod(
          "uniqueId",
          [&](std::shared_ptr<Entity> const& entity) -> std::optional<String> { return entity->uniqueId(); });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod("getParameter",
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

    if (auto registration = methods.registerMethod("sendMessage",
    [&](std::shared_ptr<Entity> const& entity, String const& message, LuaVariadic<Json> args) -> std::expected<RpcPromise<Json>, StarException> {
        if (entity->inWorld()) {
            auto world = entity->world();
            if (!world)
              return std::unexpected(std::move(world.error()));
            return world->get().sendEntityMessage(entity->entityId(), message, JsonArray::from(std::move(args)));
        }
        return RpcPromise<Json>::createFailed("Entity not in world");
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    // scripted entity methods
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod("nametag",
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
            {"color", jsonFromColor(Color::rgb(*color))},
            {"origin", jsonFromVec2F(*origin)},
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
    if (auto registration = methods.registerMethod(
          "portrait",
          [&](std::shared_ptr<Entity> const& entity,
              String const& portraitMode) -> std::expected<std::optional<List<Drawable>>, StarException> {
            if (auto portrait = as_ref<PortraitEntity>(*entity)) {
              auto mode = enumValue<PortraitModeNames>(portraitMode);
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
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod(
          "isInteractive",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto interactEntity = as_ref<InteractiveEntity>(*entity))
              return interactEntity->get().isInteractive();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // chatty entity methods
    if (auto registration = methods.registerMethod(
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
          methods.registerMethod("statusProperty",
                                 [&](std::shared_ptr<Entity> entity, String name,
                                     Json const& def = Json()) -> std::expected<std::optional<Json>, StarException> {
                                   if (auto actor = as_ref<ActorEntity>(*entity))
                                     return actor->get().statusController().statusProperty(name, def);
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "stat",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().stat(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "statPositive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().statPositive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "resourceNames",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<StringList>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourceNames();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "resource",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resource(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "isResource",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().isResource(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "resourcePositive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourcePositive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod(
          "resourceMax",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourceMax(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "resourcePercentage",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<float>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().resourcePercentage(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod(
          "uniqueStatusEffectActive",
          [&](std::shared_ptr<Entity> entity, String name) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().statusController().uniqueStatusEffectActive(name);
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // movement controller methods, they're networked anyway so might as well make them available to read

    if (auto registration = methods.registerMethod(
          "mass",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().mass();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "boundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionPoly().boundBox();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "collisionPoly",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<PolyF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionPoly();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "collisionBody",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<PolyF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionBody();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "collisionBoundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().collisionBody().boundBox();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "localBoundBox",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<RectF>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().localBoundBox();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "rotation",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().rotation();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "isColliding",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isColliding();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "isNullColliding",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isNullColliding();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "isCollisionStuck",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().isCollisionStuck();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "stickingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().stickingDirection();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "liquidPercentage",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<float>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().liquidPercentage();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "liquidId",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<std::uint8_t>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().liquidId();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "onGround",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().onGround();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "zeroG",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().zeroG();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "atWorldLimit",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().atWorldLimit();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "anchorState",
          [&](std::shared_ptr<Entity> entity) -> std::expected<LuaVariadic<LuaValue>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              if (auto anchorState = actor->get().movementController().anchorState())
                return LuaVariadic<LuaValue>{LuaInt(anchorState->entityId), LuaInt(anchorState->positionIndex)};
            return LuaVariadic<LuaValue>();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    // slightly inconsistent for the sake of being more clear what the function is
    if (auto registration = methods.registerMethod(
          "baseMovementParameters",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<Json>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().baseParameters().toJson();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    // slightly inconsistent for the sake of being more clear what the function is
    if (auto registration = methods.registerMethod(
          "movementParameters",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<Json>, StarException> {
            if (auto mobile = as_ref<MobileEntity>(*entity))
              return mobile->get().movementController().parameters().toJson();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
          "walking",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().walking();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "running",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().running();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "movingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<int>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return numericalDirection(actor->get().movementController().movingDirection());
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "facingDirection",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<int>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return numericalDirection(actor->get().movementController().facingDirection());
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "crouching",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().crouching();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "flying",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().flying();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "falling",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().falling();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "canJump",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().canJump();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "jumping",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().jumping();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "groundMovement",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().groundMovement();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));
    if (auto registration = methods.registerMethod(
          "liquidMovement",
          [&](std::shared_ptr<Entity> entity) -> std::expected<std::optional<bool>, StarException> {
            if (auto actor = as_ref<ActorEntity>(*entity))
              return actor->get().movementController().liquidMovement();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error())); 

    // tool user entity methods
    if (auto registration = methods.registerMethod("handItem",
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

    if (auto registration = methods.registerMethod("handItemDescriptor",
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

    if (auto registration = methods.registerMethod(
          "aimPosition",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<Vec2F>, StarException> {
            if (auto toolUser = as_ref<ToolUserEntity>(*entity))
              return toolUser->get().aimPosition();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));


    // humanoid entity methods
    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod("gender",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<String>, StarException> {
        std::optional<Gender> gender;
        if (auto player = as_ref<Player>(*entity))
          gender = player->get().gender();
        else if (auto npc = as_ref<Npc>(*entity))
          gender = npc->get().gender();
        if (!gender)
            return {};
        auto name = enumName<GenderNames>(*gender);
        if (!name)
          return std::unexpected(std::move(name.error()));
        return String(*name);
  }); !registration)
      return std::unexpected(std::move(registration.error()));

    // player methods
    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod("loungingEntities",
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

    if (auto registration = methods.registerMethod("loungeableOccupied",
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

    if (auto registration = methods.registerMethod("loungeableAnchorCount",
    [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<std::size_t>, StarException> {
        if (!entity->inWorld())
            return {};
        if (auto loungeable = as_ref<LoungeableEntity>(*entity))
          return loungeable->get().anchorCount();
        return {};
    }); !registration)
      return std::unexpected(std::move(registration.error()));

    // object methods
    if (auto registration = methods.registerMethod(
          "objectSpaces",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<List<Vec2I>, StarException> {
            if (auto tileEntity = as_ref<TileEntity>(*entity))
              return tileEntity->get().spaces();
            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    // farmables
    if (auto registration = methods.registerMethod(
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
    if (auto registration = methods.registerMethod(
          "containerSize",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::optional<int>, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity))
              return container->get().containerSize();

            return {};
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration =
          methods.registerMethod("containerClose",
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
          methods.registerMethod("containerOpen",
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
          methods.registerMethod("containerItems",
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

    if (auto registration = methods.registerMethod(
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
          methods.registerMethod("containerConsume",
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

    if (auto registration = methods.registerMethod("containerConsumeAt",
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
          methods.registerMethod("containerAvailable",
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

    if (auto registration = methods.registerMethod("containerTakeAll",
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

    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod("containerTakeNumItemsAt",
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
          methods.registerMethod("containerItemsCanFit",
                                 [&](std::shared_ptr<Entity> const& entity,
                                     Json const& items) -> std::expected<std::optional<std::size_t>, StarException> {
                                   if (auto container = as_ref<ContainerObject>(*entity)) {
                                     auto itemDb = entity->root().itemDatabase();
                                     if (!itemDb)
                                       return std::unexpected(std::move(itemDb.error()));
                                     auto item = (*itemDb)->fromJson(items);
                                     if (!item)
                                       return std::unexpected(std::move(item.error()));
                                     return container->get().itemBag()->itemsCanFit(optionalRef(*item));
                                   }
                                   return {};
                                 });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
          "containerItemsFitWhere",
          [&](std::shared_ptr<Entity> const& entity, Json const& items) -> std::expected<Json, StarException> {
            if (auto container = as_ref<ContainerObject>(*entity)) {
              auto itemDb = entity->root().itemDatabase();
              if (!itemDb)
                return std::unexpected(std::move(itemDb.error()));
              auto item = (*itemDb)->fromJson(items);
              if (!item)
                return std::unexpected(std::move(item.error()));
              auto result = container->get().itemBag()->itemsFitWhere(optionalRef(*item));
              auto slots = jsonFromList<std::size_t>(result.slots);
              if (!slots)
                return std::unexpected(std::move(slots.error()));
              return JsonObject{{"leftover", result.leftover}, {"slots", std::move(*slots)}};
            }
            return Json();
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod(
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

    if (auto registration = methods.registerMethod("containerPutItemsAt",
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
          methods.registerMethod("containerSwapItems",
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

    if (auto registration = methods.registerMethod("containerItemApply",
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

    if (auto registration = methods.registerMethod(
          "movingCollisionCount",
          [&](std::shared_ptr<Entity> const& entity) -> std::expected<std::size_t, StarException> {
            if (auto phys = as_ref<PhysicsEntity>(*entity)) {
              return phys->get().movingCollisionCount();
            }

            return 0;
          });
        !registration)
      return std::unexpected(std::move(registration.error()));

    if (auto registration = methods.registerMethod(
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
