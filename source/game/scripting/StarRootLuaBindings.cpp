module;
import star.system_world;
import star.json_extra;
import star.json;
import star.xx_hash;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.perlin;
import star.weighted_pool;
import star.strong_typedef;
import star.lua;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.set;
import star.string;
import star.color;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.periodic_function;
import star.audio;
import star.map;
import star.image;


import star.lua_root;

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
import star.collision_block;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.spawn_type_database;
import star.biome;
import star.lua_game_converters;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.portrait_entity;
import star.damage_bar_entity;
import star.nametag_entity;
import star.scripted_entity;
import star.chat_action;
import star.chatty_entity;
import star.emote_entity;
import star.entity_rendering_types;
import star.lounging_entities;
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.lua_components;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.effect_emitter;
import star.npc;
import star.entity_rendering;
import star.aggressive_entity;
import star.monster_database;
import star.monster;
import star.dungeon_generator;
import star.image_lua_bindings;

module star.root_lua_bindings;
import star.stored_functions;
import star.treasure;
import star.name_generator;
import star.projectile_database;
import star.liquids_database;
import star.behavior_database;
import star.species_database;
import star.tech_database;
import star.collection_database;
import star.damage_database;
import star.status_effect_database;
import star.quest_template_database;
import star.tenant_database;
import star.versioning_database;
import star.npc_database;
import star.image_metadata_database;
import star.biome_database;
import star.material_database;


namespace Star::LuaBindings {
  namespace RootCallbacks {
    [[nodiscard]] std::expected<String, StarException> assetData(Root& root, String const& path);
    [[nodiscard]] std::expected<Image, StarException> assetImage(Root& root, String const& path);
    [[nodiscard]] std::expected<Json, StarException> assetFrames(Root& root, String const& path);
    [[nodiscard]] std::expected<Json, StarException> assetJson(Root& root, String const& path);
    [[nodiscard]] std::expected<Json, StarException> makeCurrentVersionedJson(Root& root, String const& identifier, Json const& content);
    [[nodiscard]] std::expected<Json, StarException> loadVersionedJson(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog, Json const& versionedJson, String const& expectedIdentifier);
    [[nodiscard]] std::expected<double, StarException> evalFunction(Root& root, String const& arg1, double arg2);
    [[nodiscard]] std::expected<double, StarException> evalFunction2(Root& root, String const& arg1, double arg2, double arg3);
    [[nodiscard]] std::expected<Vec2U, StarException> imageSize(Root& root, String const& arg1);
    [[nodiscard]] std::expected<List<Vec2I>, StarException> imageSpaces(Root& root, String const& arg1, Vec2F const& arg2, float arg3, bool arg4);
    [[nodiscard]] std::expected<RectU, StarException> nonEmptyRegion(Root& root, String const& arg1);
    [[nodiscard]] std::expected<Json, StarException> npcConfig(Root& root, String const& arg1);
    [[nodiscard]] std::expected<float, StarException> projectileGravityMultiplier(Root& root, String const& arg1);
    [[nodiscard]] std::expected<Json, StarException> projectileConfig(Root& root, String const& arg1);
    [[nodiscard]] std::expected<JsonArray, StarException> recipesForItem(ItemDatabase const& database, String const& arg1);
    [[nodiscard]] std::expected<JsonArray, StarException> allRecipes(ItemDatabase const& database, std::optional<StringSet> filter);
    [[nodiscard]] std::expected<String, StarException> itemType(ItemDatabase const& database, String const& itemName);
    [[nodiscard]] std::expected<Json, StarException> itemTags(ItemDatabase const& database, String const& itemName);
    [[nodiscard]] std::expected<bool, StarException> itemHasTag(ItemDatabase const& database, String const& itemName, String const& itemTag);
    [[nodiscard]] std::expected<Json, StarException> itemConfig(ItemDatabase const& database, Json const& descriptor, std::optional<float> const& level, std::optional<std::uint64_t> const& seed);
    [[nodiscard]] std::expected<Json, StarException> createItem(ItemDatabase const& database, Json const& descriptor, std::optional<float> const& level, std::optional<std::uint64_t> const& seed);
    [[nodiscard]] std::expected<Json, StarException> tenantConfig(Root& root, String const& tenantName);
    [[nodiscard]] std::expected<JsonArray, StarException> getMatchingTenants(Root& root, StringMap<unsigned> const& colonyTags);
    [[nodiscard]] std::expected<Json, StarException> liquidStatusEffects(Root& root, LiquidId arg1);
    [[nodiscard]] std::expected<String, StarException> generateName(Root& root, String const& rulesAsset, std::optional<std::uint64_t> seed);
    [[nodiscard]] std::expected<Json, StarException> questConfig(Root& root, String const& templateId);
    [[nodiscard]] std::expected<JsonArray, StarException> npcPortrait(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog,
        String const& portraitMode,
        String const& species,
        String const& typeName,
        float level,
        std::optional<std::uint64_t> seed,
        std::optional<JsonObject> const& parameters);
    [[nodiscard]] std::expected<Json, StarException> npcVariant(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog,
        String const& species,
        String const& typeName,
        float level,
        std::optional<std::uint64_t> seed,
        std::optional<JsonObject> const& parameters);
    [[nodiscard]] std::expected<JsonArray, StarException> monsterPortrait(Root& root, String const& typeName, std::optional<JsonObject> const& parameters);
    [[nodiscard]] std::expected<bool, StarException> isTreasurePool(Root& root, String const& pool);
    [[nodiscard]] std::expected<JsonArray, StarException> createTreasure(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog, String const& pool, float level, std::optional<std::uint64_t> seed);
    [[nodiscard]] std::expected<std::optional<String>, StarException> materialMiningSound(Root& root, String const& materialName, std::optional<String> const& modName);
    [[nodiscard]] std::expected<std::optional<String>, StarException> materialFootstepSound(Root& root, String const& materialName, std::optional<String> const& modName);
  }
}

namespace Star {

enum VariantTerrainLayer { Background, Platform, Foreground };
auto const VariantTerrainLayerNames = EnumMap<VariantTerrainLayer>::create({
  {VariantTerrainLayer::Background, "Background"},
  {VariantTerrainLayer::Platform, "Platform"},
  {VariantTerrainLayer::Foreground, "Foreground"},
});

std::expected<LuaCallbacks, StarException> LuaBindings::makeRootCallbacks(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) {
  LuaCallbacks callbacks;

  // Builders borrow their ready catalog. Regular calls retain the current
  // shared generation catalog until invocation completes, without freezing it.
  auto itemCallback = [&root, itemCatalog]<typename Result, typename... Args>(Result (*callback)(ItemDatabase const&, Args...)) {
    return [&root, itemCatalog, callback](Args... args) -> Result {
      if (itemCatalog)
        return callback(itemCatalog->get(), std::forward<Args>(args)...);
      auto databaseOwner = root.itemDatabase();
      if (!databaseOwner)
        return std::unexpected(std::move(databaseOwner.error()));
      return callback(**databaseOwner, std::forward<Args>(args)...);
    };
  };

  if (auto r = callbacks.registerCallbackWithSignature<std::expected<String, StarException>, String>(
        "assetData", [&root](String const& path) { return RootCallbacks::assetData(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Image, StarException>, String>(
        "assetImage", [&root](String const& path) { return RootCallbacks::assetImage(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "assetFrames", [&root](String const& path) { return RootCallbacks::assetFrames(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "assetJson", [&root](String const& path) { return RootCallbacks::assetJson(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String, Json>(
        "makeCurrentVersionedJson",
        [&root](String const& identifier, Json const& content) {
          return RootCallbacks::makeCurrentVersionedJson(root, identifier, content);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, Json, String>(
        "loadVersionedJson",
        [&root, itemCatalog](Json const& versionedJson, String const& identifier) {
          return RootCallbacks::loadVersionedJson(root, itemCatalog, versionedJson, identifier);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<double, StarException>, String, double>(
        "evalFunction",
        [&root](String const& name, double arg) { return RootCallbacks::evalFunction(root, name, arg); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<double, StarException>, String, double, double>(
        "evalFunction2",
        [&root](String const& name, double arg1, double arg2) {
          return RootCallbacks::evalFunction2(root, name, arg1, arg2);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Vec2U, StarException>, String>(
        "imageSize", [&root](String const& path) { return RootCallbacks::imageSize(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r =
        callbacks.registerCallbackWithSignature<std::expected<List<Vec2I>, StarException>, String, Vec2F, float, bool>(
          "imageSpaces",
          [&root](String const& path, Vec2F const& position, float fillLimit, bool flip) {
            return RootCallbacks::imageSpaces(root, path, position, fillLimit, flip);
          });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<RectU, StarException>, String>(
        "nonEmptyRegion", [&root](String const& path) { return RootCallbacks::nonEmptyRegion(root, path); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "npcConfig", [&root](String const& name) { return RootCallbacks::npcConfig(root, name); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<float, StarException>, String>(
        "projectileGravityMultiplier",
        [&root](String const& name) { return RootCallbacks::projectileGravityMultiplier(root, name); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "projectileConfig", [&root](String const& name) { return RootCallbacks::projectileConfig(root, name); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<JsonArray, StarException>, String>("recipesForItem", itemCallback(RootCallbacks::recipesForItem)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<JsonArray, StarException>, std::optional<StringSet>>("allRecipes", itemCallback(RootCallbacks::allRecipes)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<String, StarException>, String>("itemType", itemCallback(RootCallbacks::itemType)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>("itemTags", itemCallback(RootCallbacks::itemTags)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, String, String>("itemHasTag", itemCallback(RootCallbacks::itemHasTag)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, Json, std::optional<float>, std::optional<std::uint64_t>>("itemConfig", itemCallback(RootCallbacks::itemConfig)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, Json, std::optional<float>, std::optional<std::uint64_t>>("createItem", itemCallback(RootCallbacks::createItem)); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "tenantConfig", [&root](String const& name) { return RootCallbacks::tenantConfig(root, name); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<JsonArray, StarException>, StringMap<unsigned>>(
        "getMatchingTenants",
        [&root](StringMap<unsigned> const& tags) { return RootCallbacks::getMatchingTenants(root, tags); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, LiquidId>(
        "liquidStatusEffects", [&root](LiquidId liquid) { return RootCallbacks::liquidStatusEffects(root, liquid); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r =
        callbacks
          .registerCallbackWithSignature<std::expected<String, StarException>, String, std::optional<std::uint64_t>>(
            "generateName",
            [&root](String const& rules, std::optional<std::uint64_t> seed) {
              return RootCallbacks::generateName(root, rules, seed);
            });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String>(
        "questConfig", [&root](String const& name) { return RootCallbacks::questConfig(root, name); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<JsonArray, StarException>, String, String, String,
                                                       float, std::optional<std::uint64_t>, std::optional<JsonObject>>(
        "npcPortrait",
        [&root, itemCatalog](String const& mode, String const& species, String const& typeName, float level,
                             std::optional<std::uint64_t> seed, std::optional<JsonObject> const& parameters) {
          return RootCallbacks::npcPortrait(root, itemCatalog, mode, species, typeName, level, seed, parameters);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<Json, StarException>, String, String, float,
                                                       std::optional<std::uint64_t>, std::optional<JsonObject>>(
        "npcVariant",
        [&root, itemCatalog](String const& species, String const& typeName, float level,
                             std::optional<std::uint64_t> seed, std::optional<JsonObject> const& parameters) {
          return RootCallbacks::npcVariant(root, itemCatalog, species, typeName, level, seed, parameters);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r =
        callbacks
          .registerCallbackWithSignature<std::expected<JsonArray, StarException>, String, std::optional<JsonObject>>(
            "monsterPortrait",
            [&root](String const& typeName, std::optional<JsonObject> const& parameters) {
              return RootCallbacks::monsterPortrait(root, typeName, parameters);
            });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<bool, StarException>, String>(
        "isTreasurePool", [&root](String const& pool) { return RootCallbacks::isTreasurePool(root, pool); });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<JsonArray, StarException>, String, float,
                                                       std::optional<std::uint64_t>>(
        "createTreasure",
        [&root, itemCatalog](String const& pool, float level, std::optional<std::uint64_t> seed) {
          return RootCallbacks::createTreasure(root, itemCatalog, pool, level, seed);
        });
      !r)
    return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallbackWithSignature<std::expected<std::optional<String>, StarException>, String,
                                                       std::optional<String>>(
        "materialMiningSound",
        [&root](String const& material, std::optional<String> const& mod) {
          return RootCallbacks::materialMiningSound(root, material, mod);
        });
      !r)
    return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallbackWithSignature<std::expected<std::optional<String>, StarException>, String,
                                                       std::optional<String>>(
        "materialFootstepSound",
        [&root](String const& material, std::optional<String> const& mod) {
          return RootCallbacks::materialFootstepSound(root, material, mod);
        });
      !r)
    return std::unexpected(std::move(r.error()));

  auto getVariant = [](unsigned x, unsigned y, bool isMatMod, VariantTerrainLayer layer, unsigned variants) -> unsigned {
    XXHash32 state(2938728349);
    state.push(std::span<char const>(reinterpret_cast<char const*>(&x), sizeof(x)));
    state.push(std::span<char const>(reinterpret_cast<char const*>(&y), sizeof(y)));
    state.push(std::span<char const>(reinterpret_cast<char const*>(&layer), sizeof(layer)));
    if (isMatMod)
      state.push(std::span<char const>("mod", 3));
    else
      state.push(std::span<char const>("main", 4));
    return state.digest() % variants;
  };

  if (auto r = callbacks.registerCallback("materialVariant", [getVariant](Vec2U const& tilePosition, String const& layer, unsigned variants) -> std::expected<unsigned, StarException> {
    if (!VariantTerrainLayerNames) return std::unexpected(VariantTerrainLayerNames.error());
    auto terrainLayer = VariantTerrainLayerNames->get_left(layer);
    if (!terrainLayer) return std::unexpected(std::move(terrainLayer.error()));
    return getVariant(tilePosition.x(), tilePosition.y(), false, terrainLayer->get(), variants);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("modVariant", [getVariant](Vec2U const& tilePosition, String const& layer, unsigned variants) -> std::expected<unsigned, StarException> {
    if (!VariantTerrainLayerNames) return std::unexpected(VariantTerrainLayerNames.error());
    auto terrainLayer = VariantTerrainLayerNames->get_left(layer);
    if (!terrainLayer) return std::unexpected(std::move(terrainLayer.error()));
    return getVariant(tilePosition.x(), tilePosition.y(), true, terrainLayer->get(), variants);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetsByExtension", [&root](String const& extension) -> std::expected<StringList, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto paths = (*assets)->scanExtension(extension);
    if (!paths) return std::unexpected(std::move(paths.error()));
    StringList result;
    result.reserve(paths->get().size());
    for (auto const& path : paths->get())
      result.push_back(path.string());
    return result;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetsScan", [&root](std::optional<String> const& a, std::optional<String> const& b) -> std::expected<StringList, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    return b ? (*assets)->scan(a.value_or(String{}), *b) : (*assets)->scan(a.value_or(String{}));
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetOrigin", [&root](String const& path) -> std::expected<std::optional<String>, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto descriptor = (*assets)->assetDescriptor(path);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    if (*descriptor)
      return (*assets)->assetSourcePath((*descriptor)->source);
    return std::nullopt;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetPatches", [&root](LuaEngine& engine, String const& path) -> std::expected<std::optional<LuaTable>, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto descriptor = (*assets)->assetDescriptor(path);
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    if (*descriptor) {
      auto& patches = (*descriptor)->patchSources;
      auto table = engine.createTable(patches.size(), 0);
      if (!table) return std::unexpected(std::move(table.error()));
      for (std::size_t i = 0; i != patches.size(); ++i) {
        auto& patch = patches[i];
        auto patchTable = engine.createTable(2, 0);
        if (!patchTable) return std::unexpected(std::move(patchTable.error()));
        if (auto sourcePath = (*assets)->assetSourcePath(patch.second)) {
          auto set = patchTable->set(1, *sourcePath);
          if (!set) return std::unexpected(std::move(set.error()));
        }
        auto setPath = patchTable->set(2, patch.first);
        if (!setPath) return std::unexpected(std::move(setPath.error()));
        auto setPatch = table->set(i + 1, *patchTable);
        if (!setPatch) return std::unexpected(std::move(setPatch.error()));
      }
      return std::move(*table);
    }
    return std::nullopt;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetSourcePaths", [&root](LuaEngine& engine, std::optional<bool> withMetadata) -> std::expected<LuaTable, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto assetSources = (*assets)->assetSources();
    auto table = engine.createTable(assetSources.size(), 0);
    if (!table) return std::unexpected(std::move(table.error()));
    if (withMetadata.value_or(false)) {
      for (auto& assetSource : assetSources) {
        auto metadata = (*assets)->assetSourceMetadata(assetSource);
        if (!metadata) return std::unexpected(std::move(metadata.error()));
        auto set = table->set(assetSource, *metadata);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    } else {
      std::size_t i = 0;
      for (auto& assetSource : assetSources) {
        auto set = table->set(++i, assetSource);
        if (!set) return std::unexpected(std::move(set.error()));
      }
    }
    return table;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("assetSourceMetadata", [&root](String const& assetSourcePath) -> std::expected<JsonObject, StarException> {
    auto assets = root.assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    return (*assets)->assetSourceMetadata(assetSourcePath);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("itemFile", [&root, itemCatalog](String const& itemName) -> std::expected<std::optional<String>, StarException> {
    if (itemCatalog) return itemCatalog->get().itemFile(itemName);
    auto database = root.itemDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->itemFile(itemName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("materialConfig", [&root](std::variant<MaterialId, String> const& materialNameOrId) -> std::expected<Json, StarException> {
    auto database = root.materialDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    MaterialId materialId;
    if (std::holds_alternative<MaterialId>(materialNameOrId)) {
      materialId = std::get<MaterialId>(materialNameOrId);
    } else {
      auto resolvedId = (*database)->materialId(std::get<String>(materialNameOrId));
      if (!resolvedId) return std::unexpected(std::move(resolvedId.error()));
      materialId = *resolvedId;
    }
    auto path = (*database)->materialPath(materialId);
    if (!path) return std::unexpected(std::move(path.error()));
    if (!*path) return Json();
    auto config = (*database)->materialConfig(materialId);
    if (!config) return std::unexpected(std::move(config.error()));
    if (!*config) return std::unexpected(StarException("Material path has no configuration"));
    return JsonObject{{"path", **path}, {"config", **config}};
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("modConfig", [&root](std::variant<ModId, String> const& modNameOrId) -> std::expected<Json, StarException> {
    auto database = root.materialDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    ModId modId;
    if (std::holds_alternative<ModId>(modNameOrId)) {
      modId = std::get<ModId>(modNameOrId);
    } else {
      auto resolvedId = (*database)->modId(std::get<String>(modNameOrId));
      if (!resolvedId) return std::unexpected(std::move(resolvedId.error()));
      modId = *resolvedId;
    }
    auto path = (*database)->modPath(modId);
    if (!path) return std::unexpected(std::move(path.error()));
    if (!*path) return Json();
    auto config = (*database)->modConfig(modId);
    if (!config) return std::unexpected(std::move(config.error()));
    if (!*config) return std::unexpected(StarException("Mod path has no configuration"));
    return JsonObject{{"path", **path}, {"config", **config}};
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("liquidConfig", [&root](LuaEngine& engine, LuaValue nameOrId) -> std::expected<Json, StarException> {
    auto database = root.liquidsDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    LiquidId liquidId;
    auto id = engine.luaMaybeTo<std::uint8_t>(nameOrId);
    if (!id) return std::unexpected(std::move(id.error()));
    if (*id) {
      liquidId = **id;
    } else {
      auto name = engine.luaMaybeTo<String>(nameOrId);
      if (!name) return std::unexpected(std::move(name.error()));
      if (!*name) return Json();
      auto namedId = (*database)->liquidId(**name);
      if (!namedId) return std::unexpected(std::move(namedId.error()));
      liquidId = *namedId;
    }
    auto path = (*database)->liquidPath(liquidId);
    if (!path) return std::unexpected(std::move(path.error()));
    if (!*path) return Json();
    auto config = (*database)->liquidConfig(liquidId);
    if (!config) return std::unexpected(std::move(config.error()));
    if (!*config) return std::unexpected(StarException("Liquid path has no configuration"));
    return JsonObject{{"path", **path}, {"config", **config}};
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("liquidName", [&root](LiquidId liquidId) -> std::expected<String, StarException> {
    auto database = root.liquidsDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->liquidName(liquidId);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("liquidId", [&root](String liquidName) -> std::expected<LiquidId, StarException> {
    auto database = root.liquidsDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->liquidId(liquidName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("monsterSkillParameter", [&root](String const& skillName, String const& configParameterName) -> std::expected<Json, StarException> {
    auto database = root.monsterDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->skillConfigParameter(skillName, configParameterName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("monsterParameters", [&root](String const& monsterType, std::optional<std::uint64_t> seed) -> std::expected<Json, StarException> {
    auto database = root.monsterDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto variant = (*database)->monsterVariant(monsterType, seed.value_or(0));
    if (!variant) return std::unexpected(std::move(variant.error()));
    return std::move(variant->parameters);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("monsterMovementSettings", [&root](String const& monsterType, std::optional<std::uint64_t> seed) -> std::expected<ActorMovementParameters, StarException> {
    auto database = root.monsterDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto variant = (*database)->monsterVariant(monsterType, seed.value_or(0));
    if (!variant) return std::unexpected(std::move(variant.error()));
    return std::move(variant->movementSettings);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("createBiome", [&root](String const& biomeName, std::uint64_t seed, float verticalMidPoint, float threatLevel) -> std::expected<Json, StarException> {
    auto database = root.biomeDatabase();
    if (!database) {
      if (database.error().is<BiomeException>()) return Json();
      return std::unexpected(std::move(database.error()));
    }
    auto biome = (*database)->createBiome(biomeName, seed, verticalMidPoint, threatLevel);
    if (!biome) {
      if (biome.error().is<BiomeException>()) return Json();
      return std::unexpected(std::move(biome.error()));
    }
    auto json = (*biome)->toJson();
    if (!json && json.error().is<BiomeException>()) return Json();
    return json;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("materialHealth", [&root](String const& materialName) -> std::expected<float, StarException> {
    auto database = root.materialDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto id = (*database)->materialId(materialName);
    if (!id) return std::unexpected(std::move(id.error()));
    auto parameters = (*database)->materialDamageParameters(*id);
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    return parameters->totalHealth();
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("techType", [&root](String const& techName) -> std::expected<String, StarException> {
    auto database = root.techDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto tech = (*database)->tech(techName);
    if (!tech) return std::unexpected(std::move(tech.error()));
    if (!TechTypeNames) return std::unexpected(TechTypeNames.error());
    auto name = TechTypeNames->get_right(tech->type);
    if (!name) return std::unexpected(std::move(name.error()));
    return name->get();
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("hasTech", [&root](String const& tech) -> std::expected<bool, StarException> {
    auto database = root.techDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->contains(tech);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("techConfig", [&root](String const& techName) -> std::expected<Json, StarException> {
    auto database = root.techDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto tech = (*database)->tech(techName);
    if (!tech) return std::unexpected(std::move(tech.error()));
    return std::move(tech->parameters);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("treeStemDirectory", [&root](String const& stemName) -> std::expected<std::optional<String>, StarException> {
    auto database = root.plantDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->treeStemDirectory(stemName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("treeFoliageDirectory", [&root](String const& foliageName) -> std::expected<std::optional<String>, StarException> {
    auto database = root.plantDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->treeFoliageDirectory(foliageName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("collection", [&root](String const& collectionName) -> std::expected<Collection, StarException> {
    auto database = root.collectionDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->collection(collectionName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("collectables", [&root, itemCatalog](String const& collectionName) -> std::expected<List<Collectable>, StarException> {
    auto database = root.collectionDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->collectables(collectionName, itemCatalog);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("elementalResistance", [&root](String const& damageKindName) -> std::expected<String, StarException> {
    auto database = root.damageDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto kind = (*database)->damageKind(damageKindName);
    if (!kind) return std::unexpected(std::move(kind.error()));
    auto type = (*database)->elementalType(kind->get().elementalType);
    if (!type) return std::unexpected(std::move(type.error()));
    return type->get().resistanceStat;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("elementalType", [&root](String const& damageKindName) -> std::expected<String, StarException> {
    auto database = root.damageDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto kind = (*database)->damageKind(damageKindName);
    if (!kind) return std::unexpected(std::move(kind.error()));
    return kind->get().elementalType;
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("dungeonMetadata", [&root](String const& name) -> std::expected<JsonObject, StarException> {
    auto database = root.dungeonDefinitions();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->getMetadata(name);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("systemObjectTypeConfig", [&root](String const& name) -> std::expected<Json, StarException> {
    return SystemWorld::systemObjectTypeConfig(root, name);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("itemDescriptorsMatch", [](Json const& descriptor1, Json const& descriptor2, std::optional<bool> exactMatch) -> std::expected<bool, StarException> {
    auto first = ItemDescriptor::create(descriptor1);
    if (!first) return std::unexpected(std::move(first.error()));
    auto second = ItemDescriptor::create(descriptor2);
    if (!second) return std::unexpected(std::move(second.error()));
    return first->matches(*second, exactMatch.value_or(false));
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("getConfiguration", [&root](String const& key) -> std::expected<Json, StarException> {
    if (key == "title") return std::unexpected(StarException(strf("Cannot get {}", key)));
    auto configuration = root.configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->get(key);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("setConfiguration", [&root](String const& key, Json const& value) -> std::expected<void, StarException> {
    if (key == "safeScripts" || key == "safe") return std::unexpected(StarException(strf("Cannot set {}", key)));
    auto configuration = root.configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->set(key, value);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("getConfigurationPath", [&root](String const& path) -> std::expected<Json, StarException> {
    if (path.empty()) return std::unexpected(ConfigurationException(strf("cannot get {}", path)));
    auto title = path.beginsWith("title");
    if (!title) return std::unexpected(std::move(title.error()));
    if (*title) return std::unexpected(ConfigurationException(strf("cannot get {}", path)));
    auto configuration = root.configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->getPath(path);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("setConfigurationPath", [&root](String const& path, Json const& value) -> std::expected<void, StarException> {
    if (path.empty()) return std::unexpected(ConfigurationException(strf("cannot set {}", path)));
    auto safeScripts = path.beginsWith("safeScripts");
    if (!safeScripts) return std::unexpected(std::move(safeScripts.error()));
    if (*safeScripts) return std::unexpected(ConfigurationException(strf("cannot set {}", path)));
    auto parts = path.splitAny("[].");
    if (!parts) return std::unexpected(std::move(parts.error()));
    if (!parts->empty() && (*parts)[0] == "safe")
      return std::unexpected(ConfigurationException(strf("cannot set {}", path)));
    auto configuration = root.configuration();
    if (!configuration) return std::unexpected(std::move(configuration.error()));
    return (*configuration)->setPath(path, value);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("speciesConfig", [&root](String const& species) -> std::expected<Json, StarException> {
    auto database = root.speciesDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto definition = (*database)->species(species);
    if (!definition) return std::unexpected(std::move(definition.error()));
    return (*definition)->config();
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("generateHumanoidIdentity", [&root, itemCatalog](String const& species, std::optional<std::uint64_t> seed, std::optional<String> gender) -> std::expected<LuaTupleReturn<Json, JsonObject, JsonObject>, StarException> {
    auto database = root.speciesDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    std::optional<Gender> parsedGender;
    if (gender) {
      auto parsed = enumValue<GenderNames>(*gender);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      parsedGender = *parsed;
    }
    auto result = (*database)->generateHumanoid(species, seed.value_or(root.random().randu64()), parsedGender, itemCatalog);
    if (!result) return std::unexpected(std::move(result.error()));
    auto identity = result->identity.toJson();
    if (!identity) return std::unexpected(std::move(identity.error()));
    return LuaTupleReturn<Json, JsonObject, JsonObject>{std::move(*identity), std::move(result->humanoidParameters), std::move(result->armor)};
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.copyCallback("generateHumanoidIdentity", "generateHumanoid"); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("createHumanoid", [&root, itemCatalog](
    String name,
    String speciesChoice,
    std::size_t genderChoice,
    std::size_t bodyColorChoice,
    std::size_t alty,
    std::size_t hairChoice,
    std::size_t heady,
    std::size_t shirtChoice,
    std::size_t shirtColor,
    std::size_t pantsChoice,
    std::size_t pantsColor,
    std::size_t personality,
    LuaVariadic<LuaValue> ext
  ) -> std::expected<LuaTupleReturn<Json, JsonObject, JsonObject>, StarException> {
    auto database = root.speciesDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto result = (*database)->createHumanoid(std::move(name), std::move(speciesChoice), genderChoice, bodyColorChoice,
                                              alty, hairChoice, heady, shirtChoice, shirtColor, pantsChoice, pantsColor,
                                              personality, std::move(ext), itemCatalog);
    if (!result) return std::unexpected(std::move(result.error()));
    auto identity = result->identity.toJson();
    if (!identity) return std::unexpected(std::move(identity.error()));
    return LuaTupleReturn<Json, JsonObject, JsonObject>{std::move(*identity), std::move(result->humanoidParameters), std::move(result->armor)};
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("effectConfig", [&root](String const& effect) -> std::expected<Json, StarException> {
    auto database = root.statusEffectDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    auto config = (*database)->uniqueEffectConfig(effect);
    if (!config) return std::unexpected(std::move(config.error()));
    return config->toJson();
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("monsterConfig", [&root](String const& typeName) -> std::expected<Json, StarException> {
    auto database = root.monsterDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->monsterConfig(typeName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("biomeConfig", [&root](String const& typeName) -> std::expected<Json, StarException> {
    auto database = root.biomeDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->biomeConfig(typeName);
  }); !r) return std::unexpected(std::move(r.error()));

  if (auto r = callbacks.registerCallback("treeFoliageConfig", [&root](String const& typeName) -> std::expected<Json, StarException> {
    auto database = root.plantDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->treeFoliageConfig(typeName);
  }); !r) return std::unexpected(std::move(r.error()));
  if (auto r = callbacks.registerCallback("treeStemConfig", [&root](String const& typeName) -> std::expected<Json, StarException> {
    auto database = root.plantDatabase();
    if (!database) return std::unexpected(std::move(database.error()));
    return (*database)->treeStemConfig(typeName);
  }); !r) return std::unexpected(std::move(r.error()));

  return callbacks;
}

std::expected<String, StarException> LuaBindings::RootCallbacks::assetData(Root& root, String const& path) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto bytes = (*assets)->bytes(path);
  if (!bytes) return std::unexpected(std::move(bytes.error()));
  return String(std::string_view((*bytes)->span().data(), (*bytes)->span().size()));
}

std::expected<Image, StarException> LuaBindings::RootCallbacks::assetImage(Root& root, String const& path) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto image = (*assets)->image(path);
  if (!image) return std::unexpected(std::move(image.error()));
  return **image;
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::assetFrames(Root& root, String const& path) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto frames = (*assets)->imageFrames(path);
  if (!frames) return std::unexpected(std::move(frames.error()));
  if (*frames) return (*frames)->toJson();
  return Json();
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::assetJson(Root& root, String const& path) {
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  return (*assets)->json(path);
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::makeCurrentVersionedJson(Root& root, String const& identifier, Json const& content) {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto versioned = (*database)->makeCurrentVersionedJson(identifier, content);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  return versioned->toJson();
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::loadVersionedJson(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog, Json const& versionedJson, String const& identifier) {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto versioned = VersionedJson::fromJson(versionedJson);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  return (*database)->loadVersionedJson(*versioned, identifier, itemCatalog);
}

std::expected<double, StarException> LuaBindings::RootCallbacks::evalFunction(Root& root, String const& arg1, double arg2) {
  auto database = root.functionDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto function = (*database)->function(arg1);
  if (!function) return std::unexpected(std::move(function.error()));
  return (*function)->evaluate(arg2);
}

std::expected<double, StarException> LuaBindings::RootCallbacks::evalFunction2(Root& root, String const& arg1, double arg2, double arg3) {
  auto database = root.functionDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto function = (*database)->function2(arg1);
  if (!function) return std::unexpected(std::move(function.error()));
  return (*function)->evaluate(arg2, arg3);
}

std::expected<Vec2U, StarException> LuaBindings::RootCallbacks::imageSize(Root& root, String const& arg1) {
  auto database = root.imageMetadataDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->imageSize(arg1);
}

std::expected<List<Vec2I>, StarException> LuaBindings::RootCallbacks::imageSpaces(
    Root& root, String const& arg1, Vec2F const& arg2, float arg3, bool arg4) {
  auto database = root.imageMetadataDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->imageSpaces(arg1, arg2, arg3, arg4);
}

std::expected<RectU, StarException> LuaBindings::RootCallbacks::nonEmptyRegion(Root& root, String const& arg1) {
  auto database = root.imageMetadataDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->nonEmptyRegion(arg1);
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::npcConfig(Root& root, String const& arg1) {
  auto database = root.npcDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->buildConfig(arg1);
}

std::expected<float, StarException> LuaBindings::RootCallbacks::projectileGravityMultiplier(Root& root, String const& arg1) {
  auto database = root.projectileDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->gravityMultiplier(arg1);
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::projectileConfig(Root& root, String const& arg1) {
  auto database = root.projectileDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->projectileConfig(arg1);
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::recipesForItem(ItemDatabase const& database, String const& arg1) {
  auto recipes = database.recipesForOutputItem(arg1);
  JsonArray result;
  result.reserve(recipes.size());
  for (auto& recipe : recipes) {
    auto json = recipe.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.push_back(std::move(*json));
  }
  return result;
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::allRecipes(ItemDatabase const& database, std::optional<StringSet> filter) {
  HashSet<ItemRecipe> filtered;
  auto recipes = std::cref(database.allRecipes());
  if (filter) {
    filtered = database.allRecipes(*filter);
    recipes = std::cref(filtered);
  }
  JsonArray result;
  result.reserve(recipes.get().size());
  for (auto& recipe : recipes.get()) {
    auto json = recipe.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.push_back(std::move(*json));
  }
  return result;
}

std::expected<String, StarException> LuaBindings::RootCallbacks::itemType(ItemDatabase const& database, String const& itemName) {
  auto type = database.itemType(itemName);
  if (!type) return std::unexpected(std::move(type.error()));
  auto name = enumName<ItemTypeNames>(*type);
  if (!name) return std::unexpected(std::move(name.error()));
  return String(*name);
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::itemTags(ItemDatabase const& database, String const& itemName) {
  auto tags = database.itemTags(itemName);
  if (!tags) return std::unexpected(std::move(tags.error()));
  return jsonFromStringSet(*tags);
}

std::expected<bool, StarException> LuaBindings::RootCallbacks::itemHasTag(ItemDatabase const& database, String const& itemName, String const& itemTag) {
  auto tags = database.itemTags(itemName);
  if (!tags) return std::unexpected(std::move(tags.error()));
  return tags->contains(itemTag);
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::itemConfig(ItemDatabase const& database, Json const& descJson, std::optional<float> const& level, std::optional<std::uint64_t> const& seed) {
  auto descriptor = ItemDescriptor::create(descJson);
  if (!descriptor) return std::unexpected(std::move(descriptor.error()));
  if (!database.hasItem(descriptor->name())) return Json();
  auto config = database.itemConfig(descriptor->name(), descriptor->parameters(), level, seed);
  if (!config) return std::unexpected(std::move(config.error()));
  return JsonObject{{"directory", config->directory}, {"config", config->config}, {"parameters", config->parameters}};
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::createItem(ItemDatabase const& database, Json const& descriptor, std::optional<float> const& level, std::optional<std::uint64_t> const& seed) {
  auto parsed = ItemDescriptor::create(descriptor);
  if (!parsed) return std::unexpected(std::move(parsed.error()));
  auto item = database.item(*parsed, level, seed);
  if (!item) return std::unexpected(std::move(item.error()));
  return (*item)->descriptor().toJson();
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::tenantConfig(Root& root, String const& tenantName) {
  auto database = root.tenantDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto tenant = (*database)->getTenant(tenantName);
  if (!tenant) return std::unexpected(std::move(tenant.error()));
  return (*tenant)->config;
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::getMatchingTenants(Root& root, StringMap<unsigned> const& colonyTags) {
  auto database = root.tenantDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto tenants = (*database)->getMatchingTenants(colonyTags);
  if (!tenants) return std::unexpected(std::move(tenants.error()));
  return tenants->transformed([](std::shared_ptr<Tenant> const& tenant) { return tenant->config; });
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::liquidStatusEffects(Root& root, LiquidId arg1) {
  auto database = root.liquidsDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  if (auto settings = (*database)->liquidSettings(arg1)) return settings->statusEffects;
  return Json();
}

std::expected<String, StarException> LuaBindings::RootCallbacks::generateName(Root& root, String const& rulesAsset, std::optional<std::uint64_t> seed) {
  auto generator = root.nameGenerator();
  if (!generator) return std::unexpected(std::move(generator.error()));
  return (*generator)->generateName(rulesAsset, seed.value_or(root.random().randu64()));
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::questConfig(Root& root, String const& templateId) {
  auto database = root.questTemplateDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto quest = (*database)->questTemplate(templateId);
  if (!quest) return std::unexpected(std::move(quest.error()));
  if (*quest) return (*quest)->config;
  return std::unexpected(StarException(strf("No quest template found for id '{}'", templateId)));
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::npcPortrait(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog,
    String const& portraitMode,
    String const& species,
    String const& typeName,
    float level,
    std::optional<std::uint64_t> seed,
    std::optional<JsonObject> const& parameters) {
  auto database = root.npcDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto variant = (*database)->generateNpcVariant(species, typeName, level, seed.value_or(root.random().randu64()), parameters.value_or(JsonObject{}), itemCatalog);
  if (!variant) return std::unexpected(std::move(variant.error()));
  auto mode = enumValue<PortraitModeNames>(portraitMode);
  if (!mode) return std::unexpected(std::move(mode.error()));
  auto drawables = (*database)->npcPortrait(*variant, *mode, itemCatalog);
  if (!drawables) return std::unexpected(std::move(drawables.error()));
  JsonArray result;
  result.reserve(drawables->size());
  for (auto const& drawable : *drawables) {
    auto json = drawable.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.push_back(std::move(*json));
  }
  return result;
}

std::expected<Json, StarException> LuaBindings::RootCallbacks::npcVariant(Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog, String const& species, String const& typeName, float level, std::optional<std::uint64_t> seed, std::optional<JsonObject> const& parameters) {
  auto database = root.npcDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto variant = (*database)->generateNpcVariant(species, typeName, level, seed.value_or(root.random().randu64()), parameters.value_or(JsonObject{}), itemCatalog);
  if (!variant) return std::unexpected(std::move(variant.error()));
  return (*database)->writeNpcVariantToJson(*variant);
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::monsterPortrait(Root& root, String const& typeName, std::optional<JsonObject> const& parameters) {
  auto database = root.monsterDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto seed = 0; // use a static seed to utilize caching
  auto variant = (*database)->monsterVariant(typeName, seed, parameters.value_or(JsonObject{}));
  if (!variant) return std::unexpected(std::move(variant.error()));
  auto drawables = (*database)->monsterPortrait(*variant);
  if (!drawables) return std::unexpected(std::move(drawables.error()));
  JsonArray result;
  result.reserve(drawables->size());
  for (auto const& drawable : *drawables) {
    auto json = drawable.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    result.push_back(std::move(*json));
  }
  return result;
}

std::expected<bool, StarException> LuaBindings::RootCallbacks::isTreasurePool(Root& root, String const& pool) {
  auto database = root.treasureDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  return (*database)->isTreasurePool(pool);
}

std::expected<JsonArray, StarException> LuaBindings::RootCallbacks::createTreasure(
    Root& root, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog, String const& pool, float level, std::optional<std::uint64_t> seed) {
  auto database = root.treasureDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto treasure = (*database)->createTreasure(pool, level, seed.value_or(root.random().randu64()), itemCatalog);
  if (!treasure) return std::unexpected(std::move(treasure.error()));
  return treasure->transformed([](std::shared_ptr<Item> const& item) { return item->descriptor().toJson(); });
}

std::expected<std::optional<String>, StarException> LuaBindings::RootCallbacks::materialMiningSound(
    Root& root, String const& materialName, std::optional<String> const& modName) {
  auto database = root.materialDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto materialId = (*database)->materialId(materialName);
  if (!materialId) return std::unexpected(std::move(materialId.error()));
  ModId modId = NoModId;
  if (modName) {
    auto id = (*database)->modId(*modName);
    if (!id) return std::unexpected(std::move(id.error()));
    modId = *id;
  }
  auto sound = (*database)->miningSound(*materialId, modId);
  if (!sound) return std::unexpected(std::move(sound.error()));
  if (sound->empty()) return std::nullopt;
  return std::move(*sound);
}

std::expected<std::optional<String>, StarException> LuaBindings::RootCallbacks::materialFootstepSound(
    Root& root, String const& materialName, std::optional<String> const& modName) {
  auto database = root.materialDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto materialId = (*database)->materialId(materialName);
  if (!materialId) return std::unexpected(std::move(materialId.error()));
  ModId modId = NoModId;
  if (modName) {
    auto id = (*database)->modId(*modName);
    if (!id) return std::unexpected(std::move(id.error()));
    modId = *id;
  }
  auto sound = (*database)->footstepSound(*materialId, modId);
  if (!sound) return std::unexpected(std::move(sound.error()));
  if (sound->empty()) return std::nullopt;
  return std::move(*sound);
}

}
