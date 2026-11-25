import std;
import star.json_extra;
import star.json;
import star.poly;
import star.list;
import star.bi_map;
import star.casting;
import star.vector;
import star.game_types;
import star.liquid_types;
import star.material_types;
import star.set;
import star.ordered_map;
import star.lru_cache;
import star.random;
import star.logging;
import star.rect;
import star.asset_path;
import star.string;
import star.map;
import star.format;

import star.collision_block;
import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;
import star.assets;
import star.root;
import star.liquids_database;
import star.dungeon_image_part;
import star.dungeon_tmx_part;
import star.material_database;
import star.static_random;
import star.math_common;
import star.algorithm;
namespace Star {

std::size_t const DefinitionsCacheSize = 20;

namespace Dungeon {


  [[nodiscard]] std::expected<Direction, StarException> flipDirection(Direction direction) {
    if (direction == Direction::Left) return Direction::Right;
    if (direction == Direction::Right) return Direction::Left;
    if (direction == Direction::Up) return Direction::Down;
    if (direction == Direction::Down) return Direction::Up;
    if (direction == Direction::Any) return Direction::Any;
    return std::unexpected(DungeonException("Invalid direction"));
  }

  [[nodiscard]] std::expected<std::reference_wrapper<Connector const>, StarException> choose_option(ConnectorCandidates& options, RandomSource& rnd) {
    float distribution = 0;
    for (std::size_t i = 0; i < options.size(); i++)
      distribution += options[i].get().part().chance();
    float pick = rnd.uniform_float() * distribution;
    for (std::size_t i = 0; i < options.size(); i++) {
      pick -= options[i].get().part().chance();
      if (pick <= 0)
        return options.take_at(i);
    }
    // Preserve the final-option float rounding fallback.
    return options.take_at(options.size() - 1);
  }

  std::expected<std::shared_ptr<MaxSpawnCountRule>, StarException> MaxSpawnCountRule::create(Json const& rule) {
    auto settings = rule.get(1);
    if (!settings) return std::unexpected(std::move(settings.error()));
    auto count = settings->get_int(0);
    if (!count) return std::unexpected(std::move(count.error()));
    auto result = std::shared_ptr<MaxSpawnCountRule>(new MaxSpawnCountRule);
    result->m_maxCount = *count;
    return result;
  }

  std::expected<std::shared_ptr<DoNotConnectToPartRule>, StarException> DoNotConnectToPartRule::create(Json const& rule) {
    auto entries = rule.get_array(1);
    if (!entries) return std::unexpected(std::move(entries.error()));
    auto result = std::shared_ptr<DoNotConnectToPartRule>(new DoNotConnectToPartRule);
    for (auto const& entry : *entries) {
      auto name = entry.to_string();
      if (!name) return std::unexpected(std::move(name.error()));
      result->m_partNames.add(std::move(*name));
    }
    return result;
  }

  std::expected<std::shared_ptr<DoNotCombineWithRule>, StarException> DoNotCombineWithRule::create(Json const& rule) {
    auto entries = rule.get_array(1);
    if (!entries) return std::unexpected(std::move(entries.error()));
    auto result = std::shared_ptr<DoNotCombineWithRule>(new DoNotCombineWithRule);
    for (auto const& entry : *entries) {
      auto name = entry.to_string();
      if (!name) return std::unexpected(std::move(name.error()));
      result->m_parts.add(std::move(*name));
    }
    return result;
  }

  std::expected<List<std::shared_ptr<Rule const>>, StarException> Rule::readRules(Root& root, Json const& rules) {
    List<std::shared_ptr<Rule const>> result;
    auto entries = rules.iterate_array();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto rule = Rule::parse(root, entry);
      if (!rule) return std::unexpected(std::move(rule.error()));
      if (*rule) result.push_back(**rule);
    }
    return result;
  }

  std::expected<List<std::shared_ptr<Brush const>>, StarException> Brush::readBrushes(Root& root, Json const& brushes) {
    List<std::shared_ptr<Brush const>> result;
    auto entries = brushes.iterate_array();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto brush = Brush::parse(root, entry);
      if (!brush) return std::unexpected(std::move(brush.error()));
      result.push_back(std::move(*brush));
    }
    return result;
  }

  std::expected<std::optional<std::shared_ptr<Rule const>>, StarException> Rule::parse(Root& root, Json const& rule) {
    auto key = rule.get_string(0);
    if (!key) return std::unexpected(std::move(key.error()));
    if (*key == "worldGenMustContainLiquid") return std::make_shared<const WorldGenMustContainLiquidRule>();
    if (*key == "worldGenMustNotContainLiquid") return std::make_shared<const WorldGenMustNotContainLiquidRule>();
    if (*key == "worldGenMustContainSolidForeground") return std::make_shared<const WorldGenMustContainSolidRule>(TileLayer::Foreground);
    if (*key == "worldGenMustContainAirForeground") return std::make_shared<const WorldGenMustContainAirRule>(TileLayer::Foreground);
    if (*key == "worldGenMustContainSolidBackground") return std::make_shared<const WorldGenMustContainSolidRule>(TileLayer::Background);
    if (*key == "worldGenMustContainAirBackground") return std::make_shared<const WorldGenMustContainAirRule>(TileLayer::Background);
    if (*key == "allowOverdrawing") return std::make_shared<const AllowOverdrawingRule>();
    if (*key == "ignorePartMaximumRule") return std::make_shared<const IgnorePartMaximumRule>();
    if (*key == "maxSpawnCount") {
      auto result = MaxSpawnCountRule::create(rule);
      if (!result) return std::unexpected(std::move(result.error()));
      return std::optional<std::shared_ptr<Rule const>>(std::move(*result));
    }
    if (*key == "doNotConnectToPart") {
      auto result = DoNotConnectToPartRule::create(rule);
      if (!result) return std::unexpected(std::move(result.error()));
      return std::optional<std::shared_ptr<Rule const>>(std::move(*result));
    }
    if (*key == "doNotCombineWith") {
      auto result = DoNotCombineWithRule::create(rule);
      if (!result) return std::unexpected(std::move(result.error()));
      return std::optional<std::shared_ptr<Rule const>>(std::move(*result));
    }
    auto logged = root.logger().error("Unknown dungeon rule: {}", *key);
    if (!logged) return std::unexpected(std::move(logged.error()));
    return std::optional<std::shared_ptr<Rule const>>();
  }

  std::expected<bool, StarException> Rule::checkTileCanPlace(Vec2I, DungeonGeneratorWriter&) const {
    return true;
  }

  bool Rule::overdrawable() const {
    return false;
  }

  bool Rule::ignorePartMaximum() const {
    return false;
  }

  bool Rule::allowSpawnCount(int) const {
    return true;
  }

  bool Rule::doesNotConnectToPart(String const&) const {
    return false;
  }

  bool Rule::checkPartCombinationsAllowed(StringMap<int> const&) const {
    return true;
  }

  bool Rule::requiresOpen() const {
    return false;
  }

  bool Rule::requiresSolid() const {
    return false;
  }

  bool Rule::requiresLiquid() const {
    return false;
  }

  template <typename MaterialBrush>
  [[nodiscard]] static std::expected<std::shared_ptr<Brush const>, StarException> parse_material_brush(Json const& brush) {
    auto material = brush.get_string(1);
    if (!material) return std::unexpected(std::move(material.error()));
    std::optional<String> mod;
    auto size = brush.size();
    if (!size) return std::unexpected(std::move(size.error()));
    if (*size > 2) {
      auto name = brush.get_string(2);
      if (!name) return std::unexpected(std::move(name.error()));
      mod = std::move(*name);
    }
    return std::make_shared<const MaterialBrush>(std::move(*material), std::move(mod), 0, 0, default_material_color_variant);
  }

  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> parseObjectBrush(Json const& brush) {
    auto object = brush.get_string(1);
    if (!object) return std::unexpected(std::move(object.error()));
    auto size = brush.size();
    if (!size) return std::unexpected(std::move(size.error()));
    JsonObject settings;
    if (*size > 2) {
      auto s = brush.get_object(2);
      if (!s) return std::unexpected(std::move(s.error()));
      settings = std::move(*s);
    }
    Star::Direction direction = Star::Direction::Left;
    if (auto d = settings.maybe("direction")) {
      auto name = d->to_string();
      if (!name) return std::unexpected(std::move(name.error()));
      auto parsed = enum_value<direction_names>(*name);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      direction = *parsed;
    }
    Json parameters = settings.value("parameters");
    return std::make_shared<const ObjectBrush>(*object, direction, parameters);
  }

  template <typename BiomeBrush>
  [[nodiscard]] static std::expected<std::shared_ptr<Brush const>, StarException> parse_surface_brush(Json const& brush) {
    auto settings = brush.get(1, JsonObject{});
    if (!settings) return std::unexpected(std::move(settings.error()));
    auto variant = settings->opt_double("variant");
    if (!variant) return std::unexpected(std::move(variant.error()));
    auto configured_variant = variant->value_or(0);
    if (!is_finite(configured_variant) || configured_variant < 0 || configured_variant > 5 || std::trunc(configured_variant) != configured_variant)
      return std::unexpected(DungeonException::format("Biome surface variant must be an integer in 0..5, got {}", configured_variant));
    auto material = biome_material_for_variant(static_cast<std::int64_t>(configured_variant));
    if (!material) return std::unexpected(std::move(material.error()));
    auto mod = settings->opt_string("mod");
    if (!mod) return std::unexpected(std::move(mod.error()));
    return std::make_shared<const BiomeBrush>(*material, std::move(*mod));
  }

  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> parseWireBrush(Json const& brush) {
    auto settings = brush.get(1);
    if (!settings) return std::unexpected(std::move(settings.error()));
    auto group = settings->get_string("group");
    if (!group) return std::unexpected(std::move(group.error()));
    auto local = settings->get_bool("local", true);
    if (!local) return std::unexpected(std::move(local.error()));
    return std::make_shared<const WireBrush>(*group, *local);
  }

  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> parseItemBrush(Json const& brush) {
    auto name = brush.get_string(1);
    if (!name) return std::unexpected(std::move(name.error()));
    auto item = ItemDescriptor::create(*name, 1, Json());
    if (!item) return std::unexpected(std::move(item.error()));
    return std::make_shared<const ItemBrush>(*item);
  }

  std::expected<std::shared_ptr<Brush const>, StarException> Brush::parse(Root& root, Json const& brush) {
    auto key = brush.get_string(0);
    if (!key) return std::unexpected(std::move(key.error()));
    if (*key == "clear") return std::make_shared<ClearBrush>();
    if (*key == "front") return parse_material_brush<FrontBrush>(brush);
    if (*key == "back") return parse_material_brush<BackBrush>(brush);
    if (*key == "object") return parseObjectBrush(brush);
    if (*key == "biomeitems") return std::make_shared<BiomeItemsBrush>();
    if (*key == "biometree") return std::make_shared<BiomeTreeBrush>();
    if (*key == "item") return parseItemBrush(brush);
    if (*key == "npc") {
      auto definition = brush.get(1);
      if (!definition) return std::unexpected(std::move(definition.error()));
      auto result = NpcBrush::create(root, *definition);
      if (!result) return std::unexpected(std::move(result.error()));
      return std::shared_ptr<Brush const>(std::move(*result));
    }
    if (*key == "stagehand") {
      auto definition = brush.get(1);
      if (!definition) return std::unexpected(std::move(definition.error()));
      return std::make_shared<StagehandBrush>(*definition);
    }
    if (*key == "random") {
      auto result = RandomBrush::create(root, brush);
      if (!result) return std::unexpected(std::move(result.error()));
      return std::shared_ptr<Brush const>(std::move(*result));
    }
    if (*key == "surface") return parse_surface_brush<SurfaceBrush>(brush);
    if (*key == "surfacebackground") return parse_surface_brush<SurfaceBackgroundBrush>(brush);
    if (*key == "liquid") {
      auto name = brush.get_string(1);
      if (!name) return std::unexpected(std::move(name.error()));
      auto source = brush.get_bool(2, false);
      if (!source) return std::unexpected(std::move(source.error()));
      return std::make_shared<LiquidBrush>(*name, 1.0f, *source);
    }
    if (*key == "wire") return parseWireBrush(brush);
    if (*key == "playerstart") return std::make_shared<PlayerStartBrush>();
    return std::unexpected(DungeonException::format("Unknown dungeon brush: {}", *key));
  }

  std::expected<std::shared_ptr<RandomBrush>, StarException> RandomBrush::create(Root& root, Json const& brush) {
    auto options = brush.get_array(1);
    if (!options) return std::unexpected(std::move(options.error()));
    auto result = std::shared_ptr<RandomBrush>(new RandomBrush);
    for (auto const& option : *options) {
      auto parsed = Brush::parse(root, option);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      result->m_brushes.push_back(std::move(*parsed));
    }
    result->m_seed = root.random().next_i64();
    return result;
  }

  std::expected<void, StarException> RandomBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (m_brushes.empty()) return std::unexpected(DungeonException("Random brush has no options"));
    std::size_t rnd = static_cast<std::size_t>(static_random_i32(m_seed, position[0], position[1]));
    return m_brushes[rnd % m_brushes.size()]->paint(position, phase, writer);
  }

  std::expected<void, StarException> ClearBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::ClearPhase)
      return {};

    // TODO: delete objects too?
    writer.setLiquid(position, {});
    writer.setForegroundMaterial(position, empty_material_id, 0, default_material_color_variant);
    writer.setBackgroundMaterial(position, empty_material_id, 0, default_material_color_variant);
    writer.setForegroundMod(position, no_mod_id, 0);
    writer.setBackgroundMod(position, no_mod_id, 0);
    return {};
  }

  FrontBrush::FrontBrush(String material, std::optional<String> mod, MaterialHue material_hue, MaterialHue mod_hue, MaterialColorVariant color_variant)
    : m_material(std::move(material)), m_material_hue(material_hue), m_color_variant(color_variant),
      m_mod(std::move(mod)), m_mod_hue(mod_hue) {}

  std::expected<void, StarException> FrontBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::WallPhase) return {};
    auto database = writer.root().material_database();
    if (!database) return std::unexpected(std::move(database.error()));
    auto material = (*database)->material_id(m_material);
    if (!material) return std::unexpected(std::move(material.error()));
    ModId mod = no_mod_id;
    if (m_mod) {
      auto id = (*database)->mod_id(*m_mod);
      if (!id) return std::unexpected(std::move(id.error()));
      mod = *id;
    }
    auto collision = (*database)->material_collision_kind(*material);
    if (!collision) return std::unexpected(std::move(collision.error()));
    if (isSolidColliding(*collision)) writer.setLiquid(position, {});
    writer.setForegroundMaterial(position, *material, m_material_hue, m_color_variant);
    if (is_real_mod(mod)) writer.setForegroundMod(position, mod, m_mod_hue);
    return {};
  }

  BackBrush::BackBrush(String material, std::optional<String> mod, MaterialHue material_hue, MaterialHue mod_hue, MaterialColorVariant color_variant)
    : m_material(std::move(material)), m_material_hue(material_hue), m_color_variant(color_variant),
      m_mod(std::move(mod)), m_mod_hue(mod_hue) {}

  std::expected<void, StarException> BackBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::WallPhase) return {};
    auto database = writer.root().material_database();
    if (!database) return std::unexpected(std::move(database.error()));
    auto material = (*database)->material_id(m_material);
    if (!material) return std::unexpected(std::move(material.error()));
    ModId mod = no_mod_id;
    if (m_mod) {
      auto id = (*database)->mod_id(*m_mod);
      if (!id) return std::unexpected(std::move(id.error()));
      mod = *id;
    }
    writer.setBackgroundMaterial(position, *material, m_material_hue, m_color_variant);
    if (is_real_mod(mod)) writer.setBackgroundMod(position, mod, m_mod_hue);
    return {};
  }

  ObjectBrush::ObjectBrush(String const& object, Star::Direction direction, Json const& parameters) {
    m_object = object;
    m_direction = direction;
    m_parameters = parameters;
  }

  std::expected<void, StarException> ObjectBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::ObjectPhase)
      return {};
    writer.placeObject(position, m_object, m_direction, m_parameters);
    return {};
  }

  VehicleBrush::VehicleBrush(String const& vehicle, Json const& parameters) {
    m_vehicle = vehicle;
    m_parameters = parameters;
  }

  std::expected<void, StarException> VehicleBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::ObjectPhase)
      return {};
    writer.placeVehicle(Vec2F(position), m_vehicle, m_parameters);
    return {};
  }

  std::expected<void, StarException> BiomeItemsBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::BiomeItemsPhase)
      return {};
    writer.placeSurfaceBiomeItems(position);
    return {};
  }

  std::expected<void, StarException> BiomeTreeBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::BiomeTreesPhase)
      return {};
    writer.placeBiomeTree(position);
    return {};
  }

  ItemBrush::ItemBrush(ItemDescriptor const& item) : m_item(item) {}

  std::expected<void, StarException> ItemBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::ItemPhase)
      return {};
    writer.addDrop(Vec2F(position), m_item);
    return {};
  }

  std::expected<std::shared_ptr<NpcBrush>, StarException> NpcBrush::create(Root& root, Json const& brush) {
    auto map = brush.to_object();
    if (!map) return std::unexpected(std::move(map.error()));
    if (map->value("seed") == Json("stable")) (*map)["seed"] = root.random().next_u64();
    auto result = std::shared_ptr<NpcBrush>(new NpcBrush);
    result->m_npc = std::move(*map);
    return result;
  }

  std::expected<void, StarException> NpcBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::NpcPhase) return {};
    auto hasSpecies = m_npc.contains("species");
    if (!hasSpecies) return std::unexpected(std::move(hasSpecies.error()));
    if (*hasSpecies) {
      // Preserve comma-separated unquoted species and the original draw.
      auto species = m_npc.get_string("species");
      if (!species) return std::unexpected(std::move(species.error()));
      auto compact = species->replace(" ", "");
      if (!compact) return std::unexpected(std::move(compact.error()));
      auto options = compact->split(",");
      if (!options) return std::unexpected(std::move(options.error()));
      auto chosen = writer.root().random().sample_ref(*options);
      if (!chosen) return std::unexpected(std::move(chosen.error()));
      auto npc = m_npc.set("species", chosen->get());
      if (!npc) return std::unexpected(std::move(npc.error()));
      writer.spawnNpc(Vec2F(position), *npc);
    } else {
      writer.spawnNpc(Vec2F(position), m_npc);
    }
    return {};
  }

  StagehandBrush::StagehandBrush(Json const& definition) {
    m_definition = definition;
  }

  std::expected<void, StarException> StagehandBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::NpcPhase)
      return {};
    writer.spawnStagehand(Vec2F(position), m_definition);
    return {};
  }

  DungeonIdBrush::DungeonIdBrush(DungeonId dungeonId) {
    m_dungeonId = dungeonId;
  }

  std::expected<void, StarException> DungeonIdBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::DungeonIdPhase)
      return {};
    writer.setDungeonId(position, m_dungeonId);
    return {};
  }

  SurfaceBrush::SurfaceBrush(MaterialId material, std::optional<String> mod)
    : m_material(material), m_mod(std::move(mod)) {}

  std::expected<void, StarException> SurfaceBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase == Phase::WallPhase) {
      writer.setForegroundMaterial(position, m_material, 0, default_material_color_variant);
      writer.setBackgroundMaterial(position, m_material, 0, default_material_color_variant);
    }
    if (phase == Phase::ModsPhase) {
      if (m_mod) {
        auto database = writer.root().material_database();
        if (!database) return std::unexpected(std::move(database.error()));
        auto mod = (*database)->mod_id(*m_mod);
        if (!mod) return std::unexpected(std::move(mod.error()));
        writer.setForegroundMod(position, *mod, 0);
      } else if (writer.needsForegroundBiomeMod(position)) {
        writer.setForegroundMod(position, biome_mod_id, 0);
      }
    }
    return {};
  }

  SurfaceBackgroundBrush::SurfaceBackgroundBrush(MaterialId material, std::optional<String> mod)
    : m_material(material), m_mod(std::move(mod)) {}

  std::expected<void, StarException> SurfaceBackgroundBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase == Phase::WallPhase) {
      writer.setBackgroundMaterial(position, m_material, 0, default_material_color_variant);
    }
    if (phase == Phase::ModsPhase) {
      if (m_mod) {
        auto database = writer.root().material_database();
        if (!database) return std::unexpected(std::move(database.error()));
        auto mod = (*database)->mod_id(*m_mod);
        if (!mod) return std::unexpected(std::move(mod.error()));
        writer.setBackgroundMod(position, *mod, 0);
      } else if (writer.needsBackgroundBiomeMod(position)) {
        writer.setBackgroundMod(position, biome_mod_id, 0);
      }
    }
    return {};
  }

  LiquidBrush::LiquidBrush(String const& liquid_name, float quantity, bool source)
    : m_liquid(liquid_name), m_quantity(quantity), m_source(source) {}

  std::expected<void, StarException> LiquidBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    auto database = writer.root().liquids_database();
    if (!database) return std::unexpected(std::move(database.error()));
    auto id = (*database)->liquid_id(m_liquid);
    if (!id) return std::unexpected(std::move(id.error()));
    LiquidStore liquid(*id, m_quantity, 1.0f, m_source);
    if (phase == Phase::WallPhase) writer.requestLiquid(position, liquid);
    return {};
  }

  std::expected<void, StarException> WireBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase != Phase::WirePhase)
      return {};
    writer.requestWire(position, m_wireGroup, m_partLocal);
    return {};
  }

  std::expected<void, StarException> PlayerStartBrush::paint(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    if (phase == Phase::NpcPhase) return writer.setPlayerStart(Vec2F(position));
    return {};
  }

  InvalidBrush::InvalidBrush(std::optional<String> nameHint) : m_nameHint(std::move(nameHint)) {}

  std::expected<void, StarException> InvalidBrush::paint(Vec2I, Phase, DungeonGeneratorWriter& writer) const {
    if (m_nameHint) return writer.root().logger().error("Invalid tile '{}'", *m_nameHint);
    return writer.root().logger().error("Invalid tile");
  }

  std::expected<bool, StarException> Tile::canPlace(Vec2I position, DungeonGeneratorWriter& writer) const {
    auto other = writer.otherDungeonPresent(position);
    if (!other) return std::unexpected(std::move(other.error()));
    if (*other || position[1] < 0) return false;
    for (auto const& rule : rules) {
      auto allowed = rule->checkTileCanPlace(position, writer);
      if (!allowed) return std::unexpected(std::move(allowed.error()));
      if (!*allowed) return false;
    }
    return true;
  }

  std::expected<void, StarException> Tile::place(Vec2I position, Phase phase, DungeonGeneratorWriter& writer) const {
    for (auto const& brush : brushes) {
      auto result = brush->paint(position, phase, writer);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return {};
  }

  bool Tile::usesPlaces() const {
    if (brushes.empty())
      return false;
    for (std::size_t i = 0; i < rules.size(); i++)
      if (rules[i]->overdrawable())
        return false;
    return true;
  }

  bool Tile::modifiesPlaces() const { return !brushes.empty(); }

  bool Tile::collidesWithPlaces() const {
    return usesPlaces();
  }

  bool Tile::requiresOpen() const {
    for (std::size_t i = 0; i < rules.size(); i++)
      if (rules[i]->requiresOpen())
        return true;
    return false;
  }

  bool Tile::requiresSolid() const {
    for (std::size_t i = 0; i < rules.size(); i++)
      if (rules[i]->requiresSolid())
        return true;
    return false;
  }

  bool Tile::requiresLiquid() const {
    for (std::size_t i = 0; i < rules.size(); i++)
      if (rules[i]->requiresLiquid())
        return true;
    return false;
  }

  [[nodiscard]] std::expected<std::shared_ptr<Part const>, StarException> parsePart(Root& root, DungeonDefinition const& dungeon, Json const& definition, std::optional<std::shared_ptr<ImageTileset const>> tileset) {
    auto def = definition.get("def");
    if (!def) return std::unexpected(std::move(def.error()));
    auto kind = def->get_string(0);
    if (!kind) return std::unexpected(std::move(kind.error()));
    std::shared_ptr<PartReader> reader;
    if (*kind == "image") {
      if (!tileset) return std::unexpected(DungeonException("Dungeon parts designed in images require the 'tiles' key in the .dungeon file"));
      reader = std::make_shared<ImagePartReader>(root, *tileset);
    } else if (*kind == "tmx") {
      reader = std::make_shared<TMXPartReader>(root);
    } else {
      return std::unexpected(DungeonException::format("Unknown dungeon part kind: {}", *kind));
    }
    auto part = Part::create(root, dungeon, definition, std::move(reader));
    if (!part) return std::unexpected(std::move(part.error()));
    return std::shared_ptr<Part const>(std::move(*part));
  }

  std::expected<std::shared_ptr<Part>, StarException> Part::create(Root& root, DungeonDefinition const& dungeon, Json const& part, std::shared_ptr<PartReader> reader) {
    auto result = std::shared_ptr<Part>(new Part(root));

    auto name = part.get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    result->m_name = std::move(*name);
    auto ruleJson = part.get("rules");
    if (!ruleJson) return std::unexpected(std::move(ruleJson.error()));
    auto rules = Rule::readRules(root, *ruleJson);
    if (!rules) return std::unexpected(std::move(rules.error()));
    result->m_rules = std::move(*rules);
    auto chance = part.get_float("chance", 1);
    if (!chance) return std::unexpected(std::move(chance.error()));
    result->m_chance = *chance <= 0 ? 0.0001f : *chance;
    auto mark = part.get_bool("markDungeonId", true);
    if (!mark) return std::unexpected(std::move(mark.error()));
    result->m_markDungeonId = *mark;
    auto always = part.get_bool("overrideAllowAlways", false);
    if (!always) return std::unexpected(std::move(always.error()));
    result->m_overrideAllowAlways = *always;
    auto minimum = part.opt_float("minimumThreatLevel");
    if (!minimum) return std::unexpected(std::move(minimum.error()));
    result->m_minimumThreatLevel = *minimum;
    auto maximum = part.opt_float("maximumThreatLevel");
    if (!maximum) return std::unexpected(std::move(maximum.error()));
    result->m_maximumThreatLevel = *maximum;
    auto clear = part.get_bool("clearAnchoredObjects", true);
    if (!clear) return std::unexpected(std::move(clear.error()));
    result->m_clearAnchoredObjects = *clear;
    result->m_reader = reader;
    auto def = part.get("def");
    if (!def) return std::unexpected(std::move(def.error()));
    auto assetDefinitions = def->get(1);
    if (!assetDefinitions) return std::unexpected(std::move(assetDefinitions.error()));
    if (assetDefinitions->type() == Json::Type::String) {
      auto asset = assetDefinitions->to_string();
      if (!asset) return std::unexpected(std::move(asset.error()));
      auto loaded = reader->readAsset(AssetPath::relative_to(dungeon.directory(), *asset));
      if (!loaded) return std::unexpected(std::move(loaded.error()));
    } else {
      auto assets = assetDefinitions->iterate_array();
      if (!assets) return std::unexpected(std::move(assets.error()));
      for (auto const& definition : *assets) {
        auto asset = definition.to_string();
        if (!asset) return std::unexpected(std::move(asset.error()));
        auto loaded = reader->readAsset(AssetPath::relative_to(dungeon.directory(), *asset));
        if (!loaded) return std::unexpected(std::move(loaded.error()));
      }
    }
    result->m_size = result->m_reader->size();
    auto connectors = result->scanConnectors();
    if (!connectors) return std::unexpected(std::move(connectors.error()));
    auto anchor = result->scanAnchor();
    if (!anchor) return std::unexpected(std::move(anchor.error()));
    return result;
  }

  String const& Part::name() const {
    return m_name;
  }

  Vec2U Part::size() const {
    return m_size;
  }

  Vec2I Part::anchorPoint() const {
    return m_anchorPoint;
  }

  float Part::chance() const {
    return m_chance;
  }

  bool Part::markDungeonId() const {
    return m_markDungeonId;
  }

  std::optional<float> Part::minimumThreatLevel() const {
    return m_minimumThreatLevel;
  }

  std::optional<float> Part::maximumThreatLevel() const {
    return m_maximumThreatLevel;
  }

  bool Part::clearAnchoredObjects() const {
    return m_clearAnchoredObjects;
  }

  std::expected<int, StarException> Part::placementLevelConstraint() const {
    Vec2I air = {0, size().y()};
    Vec2I ground = {0, 0};
    Vec2I liquid = {0, 0};
    auto visited = m_reader->forEachTile([&ground, &air, &liquid](Vec2I tilePos, Tile const& tile) -> std::expected<bool, StarException> {
      for (auto const& rule : tile.rules) {
        if (is<WorldGenMustContainSolidRule>(rule) && tilePos.y() > ground.y()) {
          ground = tilePos;
        }
        if (is<WorldGenMustContainAirRule>(rule) && tilePos.y() < air.y()) {
          air = tilePos;
        }
        if ((is<WorldGenMustContainLiquidRule>(rule) || is<WorldGenMustNotContainLiquidRule>(rule)) && tilePos.y() > liquid.y()) {
          liquid = tilePos;
        }
      }
      return false;
    });
    if (!visited) return std::unexpected(std::move(visited.error()));
    ground[1] = std::max(ground[1], liquid[1]);
    if (air.y() < ground.y())
      return std::unexpected(DungeonException::format(
        "Invalid ground vs air contraint! Ground {} can't be above air {}"
        " (try moving your 'require there be air here' anchors above any other 'require there be (something) here' anchors.)",
        ground, air));
    return air.y();
  }

  bool Part::ignoresPartMaximum() const {
    for (std::size_t i = 0; i < m_rules.size(); i++)
      if (m_rules[i]->ignorePartMaximum())
        return true;
    return false;
  }

  bool Part::allowsPlacement(int currentPlacementCount) const {
    for (std::size_t i = 0; i < m_rules.size(); i++)
      if (!m_rules[i]->allowSpawnCount(currentPlacementCount))
        return false;
    return true;
  }

  List<std::shared_ptr<Connector const>> const& Part::connections() const {
    return m_connections;
  }

  bool Part::doesNotConnectTo(Part const& part) const {
    for (std::size_t i = 0; i < m_rules.size(); i++)
      if (m_rules[i]->doesNotConnectToPart(part.name()))
        return true;
    for (std::size_t i = 0; i < part.m_rules.size(); i++)
      if (part.m_rules[i]->doesNotConnectToPart(m_name))
        return true;
    return false;
  }

  bool Part::checkPartCombinationsAllowed(StringMap<int> const& placementCounter) const {
    for (std::size_t i = 0; i < m_rules.size(); i++)
      if (!m_rules[i]->checkPartCombinationsAllowed(placementCounter))
        return false;
    return true;
  }

  std::expected<bool, StarException> Part::collidesWithPlaces(Vec2I pos, Set<Vec2I>& places) const {
    if (m_overrideAllowAlways) return true;
    for (auto offset : m_occupied_offsets) {
      auto position = pos + offset;
      if (places.contains(position)) {
        auto logged = root().logger().debug("Tile collided with place at {}", position);
        if (!logged) return std::unexpected(std::move(logged.error()));
        return true;
      }
    }
    return false;
  }

  std::expected<bool, StarException> Part::canPlace(Vec2I pos, DungeonGeneratorWriter& writer) const {
    if (m_overrideAllowAlways) return true;
    bool result = true;
    auto visited = m_reader->forEachTile([&result, pos, &writer](Vec2I tilePos, Tile const& tile) -> std::expected<bool, StarException> {
      auto allowed = tile.canPlace(pos + tilePos, writer);
      if (!allowed) return std::unexpected(std::move(allowed.error()));
      if (!*allowed) {
        result = false;
        return true;
      }
      return false;
    });
    if (!visited) return std::unexpected(std::move(visited.error()));
    return result;
  }

  std::expected<void, StarException> Part::place(Vec2I pos, Set<Vec2I> const& places, DungeonGeneratorWriter& writer) const {
    for (auto phase : {Phase::ClearPhase, Phase::WallPhase, Phase::ModsPhase, Phase::ObjectPhase,
        Phase::BiomeTreesPhase, Phase::BiomeItemsPhase, Phase::WirePhase, Phase::ItemPhase, Phase::NpcPhase, Phase::DungeonIdPhase}) {
      auto painted = placePhase(pos, phase, places, writer);
      if (!painted) return std::unexpected(std::move(painted.error()));
    }
    return {};
  }

  std::expected<void, StarException> Part::forEachTile(TileCallback const& callback) const {
    return m_reader->forEachTile(callback);
  }

  std::expected<void, StarException> Part::placePhase(Vec2I pos, Phase phase, Set<Vec2I> const& places, DungeonGeneratorWriter& writer) const {
    return m_reader->forEachTile([this, &places, pos, phase, &writer](Vec2I tilePos, Tile const& tile) -> std::expected<bool, StarException> {
      Vec2I position = pos + tilePos;
      if (tile.collidesWithPlaces() || !places.contains(position)) {
        auto painted = tile.place(position, phase, writer);
        if (!painted) {
          auto logged = this->root().logger().error("Error at map position {}:", tilePos);
          if (!logged) return std::unexpected(std::move(logged.error()));
          return std::unexpected(std::move(painted.error()));
        }
      }
      return false;
    });
  }

  std::expected<bool, StarException> Part::tileUsesPlaces(Vec2I pos) const {
    bool result = false;
    auto visited = m_reader->forEachTileAt(pos, [&result](Vec2I, Tile const& tile) -> std::expected<bool, StarException> {
      if (tile.usesPlaces()) {
        result = true;
        return true;
      }
      return false;
    });
    if (!visited) return std::unexpected(std::move(visited.error()));
    return result;
  }

  std::expected<Direction, StarException> Part::pickByEdge(Vec2I position, Vec2U size) const {
    int dxa = position[0];
    int dxb = size[0] - position[0];
    int dya = position[1];
    int dyb = size[1] - position[1];

    int m = std::min({dxa, dxb, dya, dyb});
    if (dxa == m)
      return Direction::Left;
    if (dxb == m)
      return Direction::Right;
    if (dya == m)
      return Direction::Down;
    if (dyb == m)
      return Direction::Up;
    return std::unexpected(DungeonException("Ambiguous direction"));
  }

  std::expected<Direction, StarException> Part::pickByNeighbours(Vec2I pos) const {
    int x = pos.x(), y = pos.y();
    if (x == 0) return Direction::Left;
    if (x == static_cast<int>(size().x()) - 1)
      return Direction::Right;
    if (y == 0) return Direction::Down;
    if (y == static_cast<int>(size().y()) - 1)
      return Direction::Up;
    // Preserve the directional scan order and short-circuit tile inspection.
    for (auto const& check : {std::pair{Vec2I{1, 0}, Direction::Left}, std::pair{Vec2I{-1, 0}, Direction::Right},
        std::pair{Vec2I{0, 1}, Direction::Down}, std::pair{Vec2I{0, -1}, Direction::Up}}) {
      auto near = tileUsesPlaces(pos + check.first);
      if (!near) return std::unexpected(std::move(near.error()));
      if (*near) {
        auto opposite = tileUsesPlaces(pos - check.first);
        if (!opposite) return std::unexpected(std::move(opposite.error()));
        if (!*opposite) return check.second;
      }
    }
    return Direction::Unknown;
  }

  std::expected<void, StarException> Part::scanConnectors() {
    auto visited = m_reader->forEachTile([this](Vec2I position, Tile const& tile) -> std::expected<bool, StarException> {
      if (tile.connector) {
        auto direction = tile.connector->direction;
        if (direction == Direction::Unknown) {
          auto picked = pickByNeighbours(position);
          if (!picked) return std::unexpected(std::move(picked.error()));
          direction = *picked;
        }
        if (direction == Direction::Unknown) {
          auto picked = pickByEdge(position, m_size);
          if (!picked) return std::unexpected(std::move(picked.error()));
          direction = *picked;
        }
        auto logged = this->root().logger().debug("Found connector on {} at {} group {} direction {}", m_name, position,
                                                  tile.connector->value, static_cast<int>(direction));
        if (!logged) return std::unexpected(std::move(logged.error()));
        m_connections.push_back(
          std::make_shared<Connector>(*this, tile.connector->value, tile.connector->forwardOnly, direction, position));
      }
      return false;
    });
    if (!visited) return std::unexpected(DungeonException(Star::format("Exception in connector {}", m_name), ExceptionDiagnostic(visited.error())));
    return {};
  }

  std::expected<void, StarException> Part::scanAnchor() {
    int cx, cy, cc;
    cx = cy = cc = 0;
    int lowestAir = m_size[1];
    int highestGound = -1;
    int highestLiquid = -1;
    auto visited = m_reader->forEachTile([&](Vec2I pos, Tile const& tile) -> std::expected<bool, StarException> {
        int x = pos.x(), y = pos.y();
        if (tile.modifiesPlaces())
          m_modified_offsets.push_back(pos);
        if (tile.usesPlaces()) {
          m_occupied_offsets.push_back(pos);
          cx += x;
          cy += y;
          cc++;
        }
        if (tile.requiresOpen()) {
          if (y < lowestAir)
            lowestAir = y;
        }
        if (tile.requiresSolid()) {
          if (y > highestGound)
            highestGound = y;
        }
        if (tile.requiresLiquid()) {
          if (y > highestLiquid)
            highestLiquid = y;
        }
        return false;
      });
    if (!visited) return std::unexpected(DungeonException(Star::format("Exception in part {}", m_name), ExceptionDiagnostic(visited.error())));

    highestGound = std::max(highestGound, highestLiquid);
    if (highestGound == -1)
      highestGound = lowestAir - 1;

    if (lowestAir == static_cast<int>(m_size[1]))
      lowestAir = highestGound + 1;

    if (cc == 0) {
      cx = m_size[0] / 2;
      cy = m_size[1] / 2;
    } else {
      cx /= cc;
      cy /= cc;
    }

    if (highestGound != -1)
      cy = highestGound + 1;

    m_anchorPoint = {cx, cy};
    return {};
  }

  std::expected<bool, StarException> WorldGenMustContainSolidRule::checkTileCanPlace(Vec2I position, DungeonGeneratorWriter& writer) const {
    return writer.checkSolid(position, layer);
  }

  std::expected<bool, StarException> WorldGenMustContainAirRule::checkTileCanPlace(Vec2I position, DungeonGeneratorWriter& writer) const {
    return writer.checkOpen(position, layer);
  }
  
  std::expected<bool, StarException> WorldGenMustContainLiquidRule::checkTileCanPlace(Vec2I position, DungeonGeneratorWriter& writer) const {
    return writer.checkLiquid(position);
  }

  std::expected<bool, StarException> WorldGenMustNotContainLiquidRule::checkTileCanPlace(Vec2I position, DungeonGeneratorWriter& writer) const {
    auto liquid = writer.checkLiquid(position);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    return !*liquid;
  }

  Connector::Connector(Part const& part, String value, bool forwardOnly, Direction direction, Vec2I offset)
      : m_value(std::move(value)), m_forwardOnly(forwardOnly), m_direction(direction), m_offset(offset), m_part(part) {}

  std::expected<bool, StarException> Connector::accepts_direction_from(Connector const& connector) const {
    if (m_forwardOnly)
      return false;
    if (m_direction == Direction::Any || connector.m_direction == Direction::Any)
      return true;
    auto flipped = flipDirection(connector.m_direction);
    if (!flipped) return std::unexpected(std::move(flipped.error()));
    return m_direction == *flipped;
  }

  String const& Connector::value() const {
    return m_value;
  }

  Vec2I Connector::positionAdjustment() const {
    if (m_direction == Direction::Any)
      return Vec2I(0, 0);
    if (m_direction == Direction::Left)
      return Vec2I(-1, 0);
    if (m_direction == Direction::Right)
      return Vec2I(1, 0);
    if (m_direction == Direction::Up)
      return Vec2I(0, 1);
    if constexpr (Star::debug_enabled) {
      Star::star_assert_failed((m_direction == Direction::Down));
    }
    return Vec2I(0, -1);
  }

  Part const& Connector::part() const {
    return m_part.get();
  }

  Vec2I Connector::offset() const {
    return m_offset;
  }

  DungeonGeneratorWriter::DungeonGeneratorWriter(Root& root, DungeonGeneratorWorldFacade& facade,
                                                 std::optional<int> terrain_marking_surface_level,
                                                 std::optional<int> terrain_surface_space_extends)
      : m_root(root), m_facade(facade), m_terrainMarkingSurfaceLevel(terrain_marking_surface_level),
        m_terrainSurfaceSpaceExtends(terrain_surface_space_extends) {
    m_currentBounds.set_min(Vec2I{std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()});
    m_currentBounds.set_max(Vec2I{std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min()});
  }

  Vec2I DungeonGeneratorWriter::wrapPosition(Vec2I const& pos) const {
    return m_facade.getWorldGeometry().xwrap(pos);
  }

  void DungeonGeneratorWriter::setMarkDungeonId(std::optional<DungeonId> dungeonId) {
    m_markDungeonId = dungeonId;
  }

  void DungeonGeneratorWriter::requestLiquid(Vec2I const& pos, LiquidStore const& liquid) {
    m_pendingLiquids[pos] = liquid;
  }

  void DungeonGeneratorWriter::setLiquid(Vec2I const& pos, LiquidStore const& liquid) {
    m_liquids[pos] = liquid;
    markPosition(pos);
  }

  void DungeonGeneratorWriter::setForegroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
    m_foregroundMaterial[position] = {material, hueshift, colorVariant};
    markPosition(position);
  }

  void DungeonGeneratorWriter::setBackgroundMaterial(Vec2I const& position, MaterialId material, MaterialHue hueshift, MaterialColorVariant colorVariant) {
    m_backgroundMaterial[position] = {material, hueshift, colorVariant};
    markPosition(position);
  }

  void DungeonGeneratorWriter::setForegroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
    m_foregroundMod[position] = {mod, hueshift};
    markPosition(position);
  }

  void DungeonGeneratorWriter::setBackgroundMod(Vec2I const& position, ModId mod, MaterialHue hueshift) {
    m_backgroundMod[position] = {mod, hueshift};
    markPosition(position);
  }

  bool DungeonGeneratorWriter::needsForegroundBiomeMod(Vec2I const& position) {
    if (!m_foregroundMaterial.contains(position))
      return false;
    if (!is_biome_material(m_foregroundMaterial[position].material))
      return false;
    Vec2I abovePosition(position.x(), position.y() + 1);
    if (m_foregroundMaterial.contains(abovePosition))
      if (m_foregroundMaterial[abovePosition].material != empty_material_id)
        return false;
    return true;
  }

  bool DungeonGeneratorWriter::needsBackgroundBiomeMod(Vec2I const& position) {
    if (!m_backgroundMaterial.contains(position))
      return false;
    if (!is_biome_material(m_backgroundMaterial[position].material))
      return false;
    Vec2I abovePosition(position.x(), position.y() + 1);
    if (m_backgroundMaterial.contains(abovePosition))
      if (m_backgroundMaterial[abovePosition].material != empty_material_id)
        return false;
    if (m_foregroundMaterial.contains(abovePosition))
      if (m_foregroundMaterial[abovePosition].material != empty_material_id)
        return false;
    return true;
  }

  void DungeonGeneratorWriter::placeObject(Vec2I const& pos, String const& objectType, Star::Direction direction, Json const& parameters) {
    m_objects[pos] = {objectType, direction, parameters};
    markPosition(pos);
  }

  void DungeonGeneratorWriter::placeVehicle(Vec2F const& pos, String const& vehicleName, Json const& parameters) {
    m_vehicles[pos] = std::make_pair(vehicleName, parameters);
    markPosition(pos);
  }

  void DungeonGeneratorWriter::placeSurfaceBiomeItems(Vec2I const& pos) {
    m_biomeItems.insert(pos);
    markPosition(pos);
  }

  void DungeonGeneratorWriter::placeBiomeTree(Vec2I const& pos) {
    m_biomeTrees.insert(pos);
    markPosition(pos);
  }

  void DungeonGeneratorWriter::addDrop(Vec2F const& position, ItemDescriptor const& item) {
    m_drops[position] = item;
    markPosition(position);
  }

  void DungeonGeneratorWriter::requestWire(Vec2I const& position, String const& wireGroup, bool partLocal) {
    if (partLocal)
      m_openLocalWires[wireGroup].add(position);
    else
      m_globalWires[wireGroup].add(position);
  }

  void DungeonGeneratorWriter::spawnNpc(Vec2F const& position, Json const& definition) {
    m_npcs[position] = definition;
    markPosition(position);
  }

  void DungeonGeneratorWriter::spawnStagehand(Vec2F const& position, Json const& definition) {
    m_stagehands[position] = definition;
    markPosition(position);
  }

  std::expected<void, StarException> DungeonGeneratorWriter::setPlayerStart(Vec2F const& startPosition) {
    return m_facade.setPlayerStart(startPosition);
  }

  std::expected<bool, StarException> DungeonGeneratorWriter::checkSolid(Vec2I position, TileLayer layer) {
    if (m_terrainMarkingSurfaceLevel)
      return position.y() < *m_terrainMarkingSurfaceLevel;
    return m_facade.checkSolid(position, layer);
  }

  std::expected<bool, StarException> DungeonGeneratorWriter::checkOpen(Vec2I position, TileLayer layer) {
    if (m_terrainMarkingSurfaceLevel)
      return position.y() >= *m_terrainMarkingSurfaceLevel;
    return m_facade.checkOpen(position, layer);
  }

  std::expected<bool, StarException> DungeonGeneratorWriter::checkLiquid(Vec2I const& position) {
    return m_facade.checkOceanLiquid(position);
  }

  std::expected<bool, StarException> DungeonGeneratorWriter::otherDungeonPresent(Vec2I position) {
    auto id = m_facade.getDungeonIdAt(position);
    if (!id) return std::unexpected(std::move(id.error()));
    return *id != no_dungeon_id;
  }

  void DungeonGeneratorWriter::setDungeonId(Vec2I const& pos, DungeonId dungeonId) {
    m_dungeonIds[pos] = dungeonId;
  }

  void DungeonGeneratorWriter::markPosition(Vec2F const& pos) {
    markPosition(Vec2I(pos.floor()));
  }

  void DungeonGeneratorWriter::markPosition(Vec2I const& pos) {
    m_currentBounds.combine(pos);
    if (m_markDungeonId)
      m_dungeonIds[pos] = *m_markDungeonId;
  }

  std::expected<void, StarException> DungeonGeneratorWriter::clearTileEntities(RectI const& bounds, Set<Vec2I> const& positions, bool clearAnchoredObjects) {
    return m_facade.clearTileEntities(bounds, positions, clearAnchoredObjects);
  }

  void DungeonGeneratorWriter::finishPart() {
    for (auto&& entries : m_openLocalWires)
      m_localWires.push_back(entries.second);
    m_openLocalWires.clear();

    if (m_currentBounds.x_min() > m_currentBounds.x_max())
      return;
    m_boundingBoxes.push_back(m_currentBounds);
    m_currentBounds.set_min(Vec2I{std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()});
    m_currentBounds.set_max(Vec2I{std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min()});
  }

  std::expected<void, StarException> pressurize_liquids(Map<Vec2I, LiquidStore>& liquids) {
    Map<LiquidId, Set<Vec2I>> unpressurized_liquids;
    for (auto const& [position, liquid] : liquids)
      unpressurized_liquids[liquid.liquid].add(position);

    List<Vec2I> region;
    for (auto& [liquid_id, unpressurized] : unpressurized_liquids) {
      while (!unpressurized.empty()) {
        auto first = unpressurized.take_first();
        if (!first) return std::unexpected(std::move(first.error()));
        region.clear();
        region.push_back(*first);
        int highest_y = (*first)[1];
        for (std::size_t i = 0; i < region.size(); ++i) {
          auto const current = region[i]; // Appending neighbors can invalidate references.
          for (auto dir : {Vec2I(1, 0), Vec2I(-1, 0), Vec2I(0, 1), Vec2I(0, -1)}) {
            auto pos = current + dir;
            if (unpressurized.remove(pos)) {
              region.push_back(pos);
              highest_y = std::max(highest_y, pos[1]);
            }
          }
        }
        for (auto const& pos : region)
          liquids[pos].pressure = 1.0f + highest_y - pos[1];
      }
    }
    return {};
  }

  std::expected<void, StarException> DungeonGeneratorWriter::flush_liquids() {
    if (auto result = pressurize_liquids(m_pendingLiquids); !result) return result;

    for (auto& p : m_pendingLiquids)
      setLiquid(p.first, p.second);

    m_pendingLiquids.clear();
    return {};
  }

  std::expected<List<RectI>, StarException> DungeonGeneratorWriter::flush() {
    auto geometry = m_facade.getWorldGeometry();
    auto displace = [&](Vec2I pos) -> Vec2I { return geometry.xwrap(pos); };
    auto displaceF = [&](Vec2F pos) -> Vec2F { return geometry.xwrap(pos); };

    PolyF::VertexList terrainBlendingVertexes;
    PolyF::VertexList spaceBlendingVertexes;
    for (auto const& bb : m_boundingBoxes) {
      if (auto applied = m_facade.markRegion(bb); !applied) return std::unexpected(std::move(applied.error()));

      if (m_terrainMarkingSurfaceLevel) {
        // Mark the regions of the dungeon above the dungeon surface as needing
        // space, and the regions below the surface as needing terrain
        if (bb.y_min() < *m_terrainMarkingSurfaceLevel) {
          RectI lower = bb;
          lower.set_y_max(std::min(lower.y_max(), *m_terrainMarkingSurfaceLevel));
          terrainBlendingVertexes.push_back(Vec2F(lower.x_min(), lower.y_min()));
          terrainBlendingVertexes.push_back(Vec2F(lower.x_max(), lower.y_min()));
          terrainBlendingVertexes.push_back(Vec2F(lower.x_min(), lower.y_max()));
          terrainBlendingVertexes.push_back(Vec2F(lower.x_max(), lower.y_max()));
        }

        if (bb.y_max() > *m_terrainMarkingSurfaceLevel) {
          RectI upper = bb;
          upper.set_y_min(std::max(upper.y_min(), *m_terrainMarkingSurfaceLevel));
          spaceBlendingVertexes.push_back(Vec2F(upper.x_min(), upper.y_min()));
          spaceBlendingVertexes.push_back(Vec2F(upper.x_max(), upper.y_min()));
          spaceBlendingVertexes.push_back(Vec2F(upper.x_min(), upper.y_max() + m_terrainSurfaceSpaceExtends.value_or(0)));
          spaceBlendingVertexes.push_back(Vec2F(upper.x_max(), upper.y_max() + m_terrainSurfaceSpaceExtends.value_or(0)));
        }
      }
    }

    if (!terrainBlendingVertexes.empty())
      if (auto applied = m_facade.markTerrain(PolyF::convex_hull(std::move(terrainBlendingVertexes))); !applied) return std::unexpected(std::move(applied.error()));
    if (!spaceBlendingVertexes.empty())
      if (auto applied = m_facade.markSpace(PolyF::convex_hull(std::move(spaceBlendingVertexes))); !applied) return std::unexpected(std::move(applied.error()));

    for (auto iter = m_backgroundMaterial.begin(); iter != m_backgroundMaterial.end(); iter++)
      if (auto applied = m_facade.setBackgroundMaterial(displace(iter->first), iter->second.material, iter->second.hueshift, iter->second.colorVariant); !applied) return std::unexpected(std::move(applied.error()));
    for (auto iter = m_foregroundMaterial.begin(); iter != m_foregroundMaterial.end(); iter++)
      if (auto applied = m_facade.setForegroundMaterial(displace(iter->first), iter->second.material, iter->second.hueshift, iter->second.colorVariant); !applied) return std::unexpected(std::move(applied.error()));
    for (auto iter = m_foregroundMod.begin(); iter != m_foregroundMod.end(); iter++)
      if (auto applied = m_facade.setForegroundMod(displace(iter->first), iter->second.mod, iter->second.hueshift); !applied) return std::unexpected(std::move(applied.error()));
    for (auto iter = m_backgroundMod.begin(); iter != m_backgroundMod.end(); iter++)
      if (auto applied = m_facade.setBackgroundMod(displace(iter->first), iter->second.mod, iter->second.hueshift); !applied) return std::unexpected(std::move(applied.error()));

    List<Vec2I> sortedPositions = m_objects.keys();
    sort_by_key(sortedPositions, [](Vec2I pos) { return pos[1] + pos[0] / 1000.0f; });
    for (auto pos : sortedPositions) {
      auto& object = m_objects[pos];
      if (auto applied = m_facade.placeObject(displace(pos), object.objectName, object.direction, object.parameters); !applied) return std::unexpected(std::move(applied.error()));
    }

    for (auto const& entry : m_vehicles) {
      String vehicleName;
      Json parameters;
      std::tie(vehicleName, parameters) = entry.second;
      if (auto applied = m_facade.placeVehicle(displaceF(entry.first), vehicleName, parameters); !applied) return std::unexpected(std::move(applied.error()));
    }

    sortedPositions = List<Vec2I>::from(m_biomeTrees);
    sort_by_key(sortedPositions, [](Vec2I pos) { return pos[1] + pos[0] / 1000.0f; });
    for (auto pos : sortedPositions) {
      if (auto applied = m_facade.placeBiomeTree(pos); !applied) return std::unexpected(std::move(applied.error()));
    }

    sortedPositions = List<Vec2I>::from(m_biomeItems);
    sort_by_key(sortedPositions, [](Vec2I pos) { return pos[1] + pos[0] / 1000.0f; });
    for (auto pos : sortedPositions) {
      if (auto applied = m_facade.placeSurfaceBiomeItems(pos); !applied) return std::unexpected(std::move(applied.error()));
    }

    for (auto& npc : m_npcs) {
      if (auto applied = m_facade.spawnNpc(displaceF(npc.first), npc.second); !applied) return std::unexpected(std::move(applied.error()));
    }

    for (auto& stagehand : m_stagehands) {
      if (auto applied = m_facade.spawnStagehand(displaceF(stagehand.first), stagehand.second); !applied) return std::unexpected(std::move(applied.error()));
    }

    for (auto&& wires : m_globalWires) {
      List<Vec2I> wireGroup;
      for (auto& pos : wires.second)
        wireGroup.push_back(displace(pos));
      if (auto applied = m_facade.connectWireGroup(wireGroup); !applied) return std::unexpected(std::move(applied.error()));
    }
    for (auto& wires : m_localWires) {
      List<Vec2I> wireGroup;
      for (auto& pos : wires)
        wireGroup.push_back(displace(pos));
      if (auto applied = m_facade.connectWireGroup(wireGroup); !applied) return std::unexpected(std::move(applied.error()));
    }

    for (auto iter = m_drops.begin(); iter != m_drops.end(); iter++)
      if (auto applied = m_facade.addDrop(displaceF(iter->first), iter->second); !applied) return std::unexpected(std::move(applied.error()));

    for (auto iter = m_liquids.begin(); iter != m_liquids.end(); iter++)
      if (auto applied = m_facade.setLiquid(displace(iter->first), iter->second); !applied) return std::unexpected(std::move(applied.error()));

    for (auto const& [position, dungeon_id] : m_dungeonIds)
      if (auto applied = m_facade.setDungeonIdAt(position, dungeon_id); !applied) return std::unexpected(std::move(applied.error()));
    return std::move(m_boundingBoxes);
  }

  void DungeonGeneratorWriter::reset() {
    m_currentBounds.set_min(Vec2I{std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()});
    m_currentBounds.set_max(Vec2I{std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min()});

    m_pendingLiquids.clear();
    m_foregroundMaterial.clear();
    m_backgroundMaterial.clear();
    m_foregroundMod.clear();
    m_backgroundMod.clear();
    m_objects.clear();
    m_biomeTrees.clear();
    m_biomeItems.clear();
    m_drops.clear();
    m_npcs.clear();
    m_stagehands.clear();
    m_liquids.clear();
    m_globalWires.clear();
    m_localWires.clear();
    m_openLocalWires.clear();
    m_boundingBoxes.clear();
  }
}

DungeonDefinitions::DungeonDefinitions(Root& root) : m_root(root), m_definitionCache(DefinitionsCacheSize) {}

std::expected<std::shared_ptr<DungeonDefinitions>, StarException> DungeonDefinitions::create(Root& root) {
  auto result = std::shared_ptr<DungeonDefinitions>(new DungeonDefinitions(root));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scan(".dungeon");
  if (!files) return std::unexpected(std::move(files.error()));
  for (auto const& file : *files) {
    auto dungeon = (*assets)->json(file);
    if (!dungeon) return std::unexpected(std::move(dungeon.error()));
    auto metadata = dungeon->get("metadata");
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    auto name = metadata->get_string("name");
    if (!name) return std::unexpected(std::move(name.error()));
    result->m_paths.insert(*name, file);
  }
  return result;
}

std::expected<std::shared_ptr<DungeonDefinition const>, StarException> DungeonDefinitions::get(String const& name) const {
  std::lock_guard locker(m_cacheMutex);
  auto definition = m_definitionCache.get(name, [this](String const& name) -> std::expected<std::shared_ptr<DungeonDefinition>, StarException> {
    if (auto path = m_paths.maybe(name)) return readDefinition(*path);
    return std::unexpected(DungeonException::format("Unknown dungeon: '{}'", name));
  });
  if (!definition) return std::unexpected(std::move(definition.error()));
  return std::shared_ptr<DungeonDefinition const>(definition->get());
}

std::expected<JsonObject, StarException> DungeonDefinitions::getMetadata(String const& name) const {
  auto definition = get(name);
  if (!definition) return std::unexpected(std::move(definition.error()));
  return (*definition)->metadata();
}

std::expected<std::shared_ptr<DungeonDefinition>, StarException> DungeonDefinitions::readDefinition(String const& path) const {
  auto load = [&]() -> std::expected<std::shared_ptr<DungeonDefinition>, StarException> {
    auto assets = this->root().assets();
    if (!assets) return std::unexpected(std::move(assets.error()));
    auto json = (*assets)->json(path);
    if (!json) return std::unexpected(std::move(json.error()));
    auto definition = json->to_object();
    if (!definition) return std::unexpected(std::move(definition.error()));
    return DungeonDefinition::create(this->root(), *definition, AssetPath::directory(path));
  };
  auto result = load();
  if (!result) return std::unexpected(DungeonException(Star::format("Error loading dungeon '{}'", path), ExceptionDiagnostic(result.error())));
  return result;
}

std::expected<std::shared_ptr<DungeonDefinition>, StarException> DungeonDefinition::create(Root& root, JsonObject const& definition, String const& directory) {
  auto result = std::shared_ptr<DungeonDefinition>(new DungeonDefinition(root));
  result->m_directory = directory;
  auto metadataValue = definition.get("metadata");
  if (!metadataValue) return std::unexpected(std::move(metadataValue.error()));
  Json const& metadataJson = metadataValue->get();
  auto metadata = metadataJson.to_object();
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  result->m_metadata = std::move(*metadata);
  auto name = metadataJson.get_string("name");
  if (!name) return std::unexpected(std::move(name.error()));
  result->m_name = std::move(*name);
  auto displayName = result->m_metadata.value("displayName", "").to_string();
  if (!displayName) return std::unexpected(std::move(displayName.error()));
  result->m_displayName = std::move(*displayName);
  auto species = metadataJson.get_string("species");
  if (!species) return std::unexpected(std::move(species.error()));
  result->m_species = std::move(*species);
  auto protectedValue = result->m_metadata.value("protected", false).to_bool();
  if (!protectedValue) return std::unexpected(std::move(protectedValue.error()));
  result->m_isProtected = *protectedValue;
  if (auto ruleJson = result->m_metadata.maybe("rules")) {
    auto rules = Dungeon::Rule::readRules(root, *ruleJson);
    if (!rules) return std::unexpected(std::move(rules.error()));
    result->m_rules = std::move(*rules);
  }
  auto radius = result->m_metadata.value("maxRadius", 100).to_int();
  if (!radius) return std::unexpected(std::move(radius.error()));
  result->m_maxRadius = *radius;
  auto maxParts = result->m_metadata.value("maxParts", 100).to_int();
  if (!maxParts) return std::unexpected(std::move(maxParts.error()));
  result->m_maxParts = *maxParts;
  auto freeSpace = result->m_metadata.value("extendSurfaceFreeSpace", 0).to_int();
  if (!freeSpace) return std::unexpected(std::move(freeSpace.error()));
  result->m_extendSurfaceFreeSpace = *freeSpace;
  auto anchorJson = metadataJson.get("anchor");
  if (!anchorJson) return std::unexpected(std::move(anchorJson.error()));
  auto anchors = json_to_string_list(*anchorJson);
  if (!anchors) return std::unexpected(std::move(anchors.error()));
  result->m_anchors = std::move(*anchors);
  std::optional<std::shared_ptr<Dungeon::ImageTileset const>> tileset;
  if (auto tiles = definition.maybe("tiles")) {
    auto loaded = Dungeon::ImageTileset::create(root, *tiles);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    tileset = std::move(*loaded);
  }
  auto partsValue = definition.get("parts");
  if (!partsValue) return std::unexpected(std::move(partsValue.error()));
  auto parts = partsValue->get().iterate_array();
  if (!parts) return std::unexpected(std::move(parts.error()));
  for (auto const& partJson : *parts) {
    auto part = parsePart(root, *result, partJson, tileset);
    if (!part) return std::unexpected(std::move(part.error()));
    if (result->m_parts.contains((*part)->name()))
      return std::unexpected(DungeonException::format("Duplicate dungeon part name: {}", (*part)->name()));
    auto const& partName = (*part)->name();
    result->m_parts.insert(partName, std::move(*part));
  }
  // Iterate the completed part map, not the JSON list: bucket order is draw order.
  for (auto const& entry : result->m_parts)
    for (auto const& connection : entry.second->connections())
      result->m_connectors_by_value[connection->value()].push_back(std::cref(*connection));
  if (auto gravityJson = result->m_metadata.maybe("gravity")) {
    auto gravity = gravityJson->to_float();
    if (!gravity) return std::unexpected(std::move(gravity.error()));
    result->m_gravity = *gravity;
  }
  if (auto breathableJson = result->m_metadata.maybe("breathable")) {
    auto breathable = breathableJson->to_bool();
    if (!breathable) return std::unexpected(std::move(breathable.error()));
    result->m_breathable = *breathable;
  }
  return result;
}

JsonObject DungeonDefinition::metadata() const {
  return m_metadata;
}

String DungeonDefinition::directory() const {
  return m_directory;
}

String DungeonDefinition::name() const {
  return m_name;
}

String DungeonDefinition::displayName() const {
  return m_displayName;
}

bool DungeonDefinition::isProtected() const {
  return m_isProtected;
}

std::optional<float> DungeonDefinition::gravity() const {
  return m_gravity;
}

std::optional<bool> DungeonDefinition::breathable() const {
  return m_breathable;
}

StringMap<std::shared_ptr<Dungeon::Part const>> const& DungeonDefinition::parts() const {
  return m_parts;
}

List<String> const& DungeonDefinition::anchors() const {
  return m_anchors;
}

std::optional<Json> const& DungeonDefinition::optTileset() const {
  return m_tileset;
}

int DungeonDefinition::maxParts() const {
  return m_maxParts;
}

int DungeonDefinition::maxRadius() const {
  return m_maxRadius;
}

int DungeonDefinition::extendSurfaceFreeSpace() const {
  return m_extendSurfaceFreeSpace;
}

DungeonGenerator::DungeonGenerator(Root& root, std::shared_ptr<DungeonDefinition const> definition, std::uint64_t seed, float threat_level, std::optional<DungeonId> dungeon_id)
  : m_root(root), m_def(std::move(definition)), m_rand(seed), m_threatLevel(threat_level), m_dungeonId(dungeon_id) {}

std::expected<DungeonGenerator, StarException> DungeonGenerator::create(Root& root, String const& dungeon_name, std::uint64_t seed, float threat_level, std::optional<DungeonId> dungeon_id) {
  auto definitions = root.dungeonDefinitions();
  if (!definitions) return std::unexpected(std::move(definitions.error()));
  auto definition = (*definitions)->get(dungeon_name);
  if (!definition) return std::unexpected(std::move(definition.error()));
  return DungeonGenerator(root, std::move(*definition), seed, threat_level, dungeon_id);
}

std::expected<std::optional<DungeonBuildResult>, StarException> DungeonGenerator::generate(DungeonGeneratorWorldFacade& facade, Vec2I position, bool mark_surface_and_terrain, bool force_placement) {
  String name = m_def->name();
  auto generate = [&]() -> std::expected<std::optional<DungeonBuildResult>, StarException> {
    Dungeon::DungeonGeneratorWriter writer(root(), facade, mark_surface_and_terrain ? position[1] : std::optional<int>(), m_def->extendSurfaceFreeSpace());
    auto logged = force_placement ? root().logger().debug("Forcing generation of dungeon {}", name) : root().logger().debug("Generating dungeon {}", name);
    if (!logged) return std::unexpected(std::move(logged.error()));
    auto picked = pick_anchor();
    if (!picked) return std::unexpected(std::move(picked.error()));
    if (!*picked) {
      auto missing = root().logger().error("No valid anchor piece found for dungeon {} at {}", name, position);
      if (!missing) return std::unexpected(std::move(missing.error()));
      return {};
    }
    auto const& anchor = (*picked)->get();
    auto constraint = anchor.placementLevelConstraint();
    if (!constraint) return std::unexpected(std::move(constraint.error()));
    auto const y = static_cast<std::int64_t>(position[1]) - *constraint;
    if (!std::in_range<int>(y))
      return std::unexpected(DungeonException("Dungeon anchor position is outside the coordinate range"));
    Vec2I pos{position[0], static_cast<int>(y)};
    if (!force_placement) {
      auto can_place = anchor.canPlace(pos, writer);
      if (!can_place) return std::unexpected(std::move(can_place.error()));
      if (!*can_place) {
        auto failed = root().logger().debug("Failed to place dungeon {} at {}", name, position);
        if (!failed) return std::unexpected(std::move(failed.error()));
        return {};
      }
    }
    auto placing = root().logger().info("Placing dungeon {} at {}", name, position);
    if (!placing) return std::unexpected(std::move(placing.error()));
    auto built = build_dungeon(anchor, pos, writer, force_placement);
    if (!built) return std::unexpected(std::move(built.error()));
    return std::optional<DungeonBuildResult>(std::move(*built));
  };
  auto result = generate();
  if (!result) return std::unexpected(DungeonException(Star::format("Error generating dungeon named '{}'", name), ExceptionDiagnostic(result.error())));
  return result;
}

std::expected<DungeonBuildResult, StarException> DungeonGenerator::build_dungeon(Dungeon::Part const& anchor, Vec2I base_pos, Dungeon::DungeonGeneratorWriter& writer, bool force_placement) {
  writer.reset();

  Deque<std::pair<std::reference_wrapper<Dungeon::Part const>, Vec2I>> openSet;
  StringMap<int> placementCounter;
  Set<Vec2I> modified_tiles;
  Set<Vec2I> preserveTiles;
  int piecesPlaced = 0;

  if (auto logged = root().logger().debug("Placing dungeon entrance at {}", base_pos); !logged) return std::unexpected(std::move(logged.error()));

  auto place_part = [&](Dungeon::Part const& part, Vec2I const& place_pos) -> std::expected<void, StarException> {
      auto const size = part.size();
      for (std::size_t axis = 0; axis < 2; ++axis)
        if (!std::in_range<int>(size[axis]) || !std::in_range<int>(static_cast<std::int64_t>(place_pos[axis]) + size[axis]))
          return std::unexpected(DungeonException("Dungeon part bounds are outside the coordinate range"));
      Set<Vec2I> clear_positions;
      for (auto offset : part.modified_offsets())
        clear_positions.insert(writer.wrapPosition(place_pos + offset));
      auto part_bounds = RectI::with_size(place_pos, Vec2I(size));
      if (auto result = writer.clearTileEntities(part_bounds, clear_positions, part.clearAnchoredObjects()); !result) return std::unexpected(std::move(result.error()));

      if (part.markDungeonId())
        writer.setMarkDungeonId(m_dungeonId);
      else
        writer.setMarkDungeonId({});

      if (auto result = part.place(place_pos, preserveTiles, writer); !result) return std::unexpected(std::move(result.error()));
      writer.finishPart();

      for (auto offset : part.occupied_offsets())
        preserveTiles.insert(place_pos + offset);
      for (auto offset : part.modified_offsets())
        modified_tiles.insert(place_pos + offset);
      openSet.push_back({std::cref(part), place_pos});

      placementCounter[part.name()]++;
      piecesPlaced++;

      if (auto logged = this->root().logger().debug("placed {}", part.name()); !logged) return std::unexpected(std::move(logged.error()));
      return {};
    };

  auto placed_anchor = place_part(anchor, base_pos);
  if (!placed_anchor) return std::unexpected(std::move(placed_anchor.error()));

  Vec2I origin = base_pos + Vec2I(anchor.size()) / 2;

  Set<Vec2I> closedConnectors;
  while (!openSet.empty()) {
    auto next = openSet.take_front();
    if (!next) return std::unexpected(std::move(next.error()));
    Dungeon::Part const& parentPart = next->first.get();
    Vec2I parentPos = next->second;
    if (auto logged = this->root().logger().debug("Trying to add part {} at {} connectors: {}", parentPart.name(), parentPos, parentPart.connections().size()); !logged) return std::unexpected(std::move(logged.error()));
    for (std::size_t i = 0; i < parentPart.connections().size(); i++) {
      auto const& connector = parentPart.connections()[i];
      Vec2I connectorPos = parentPos + connector->offset();
      if (closedConnectors.contains(connectorPos))
        continue;
      auto found_options = m_def->find_connectable_parts(*connector);
      if (!found_options) return std::unexpected(std::move(found_options.error()));
      auto options = std::move(*found_options);
      while (!options.empty()) {
        auto chosen_option = choose_option(options, m_rand);
        if (!chosen_option) return std::unexpected(std::move(chosen_option.error()));
        auto const& option = chosen_option->get();
        auto const& option_part = option.part();
        if (auto logged = this->root().logger().debug("Trying part {}", option_part.name()); !logged) return std::unexpected(std::move(logged.error()));
        auto adjustment = option.positionAdjustment();
        Vec2I partPos = connectorPos - option.offset() + adjustment;
        Vec2I optionPos = connectorPos + adjustment;
        if (!option_part.ignoresPartMaximum()) {
          if (piecesPlaced >= m_def->maxParts())
            continue;

          if ((partPos - origin).magnitude() > m_def->maxRadius()) {
            if (auto logged = this->root().logger().debug("out of range. {} ... {}", partPos, origin); !logged) return std::unexpected(std::move(logged.error()));
            continue;
          }
        }
        if (!option_part.allowsPlacement(placementCounter[option_part.name()])) {
          if (auto logged = this->root().logger().debug("part failed in allowsPlacement"); !logged) return std::unexpected(std::move(logged.error()));
          continue;
        }
        if (!option_part.checkPartCombinationsAllowed(placementCounter)) {
          if (auto logged = this->root().logger().debug("part failed in checkPartCombinationsAllowed"); !logged) return std::unexpected(std::move(logged.error()));
          continue;
        }
        auto collides = option_part.collidesWithPlaces(partPos, preserveTiles);
        if (!collides) return std::unexpected(std::move(collides.error()));
        if (*collides) {
          if (auto logged = this->root().logger().debug("part failed in collidesWithPlaces"); !logged) return std::unexpected(std::move(logged.error()));
          continue;
        }
        if (option_part.minimumThreatLevel() && m_threatLevel < *option_part.minimumThreatLevel()) {
          if (auto logged = this->root().logger().debug("part failed in minimumThreatLevel"); !logged) return std::unexpected(std::move(logged.error()));
          continue;
        }
        if (option_part.maximumThreatLevel() && m_threatLevel > *option_part.maximumThreatLevel()) {
          if (auto logged = this->root().logger().debug("part failed in maximumThreatLevel"); !logged) return std::unexpected(std::move(logged.error()));
          continue;
        }
        bool allowed = force_placement;
        if (!allowed) {
          auto canPlace = option_part.canPlace(partPos, writer);
          if (!canPlace) return std::unexpected(std::move(canPlace.error()));
          allowed = *canPlace;
        }
        if (allowed) {
          auto placed = place_part(option_part, partPos);
          if (!placed) return std::unexpected(std::move(placed.error()));
          closedConnectors.add(connectorPos);
          closedConnectors.add(optionPos);
          break;
        } else {
          if (auto logged = this->root().logger().debug("part failed in canPlace"); !logged) return std::unexpected(std::move(logged.error()));
        }
      }
    }
  }
  if (auto logged = this->root().logger().debug("Settling dungeon water."); !logged) return std::unexpected(std::move(logged.error()));
  if (auto result = writer.flush_liquids(); !result) return std::unexpected(std::move(result.error()));
  if (auto logged = this->root().logger().debug("Flushing dungeon into the worldgen."); !logged) return std::unexpected(std::move(logged.error()));
  auto bounds = writer.flush();
  if (!bounds) return std::unexpected(std::move(bounds.error()));
  return DungeonBuildResult{.bounds = std::move(*bounds), .modified_tiles = std::move(modified_tiles)};
}

std::expected<std::optional<std::reference_wrapper<Dungeon::Part const>>, StarException> DungeonGenerator::pick_anchor() {
  SmallList<std::reference_wrapper<Dungeon::Part const>, 1> valid_anchors;
  valid_anchors.reserve(m_def->anchors().size());
  for (auto const& name : m_def->anchors()) {
    auto part = m_def->parts().get(name);
    if (!part) return std::unexpected(std::move(part.error()));
    auto const& anchor = *part->get();
    if ((!anchor.minimumThreatLevel() || m_threatLevel >= *anchor.minimumThreatLevel())
        && (!anchor.maximumThreatLevel() || m_threatLevel <= *anchor.maximumThreatLevel()))
      valid_anchors.push_back(std::cref(anchor));
  }
  if (valid_anchors.empty()) return {};
  auto selected = m_rand.sample_ref(valid_anchors);
  if (!selected) return std::unexpected(std::move(selected.error()));
  return selected->get();
}

std::expected<Dungeon::ConnectorCandidates, StarException> DungeonDefinition::find_connectable_parts(Dungeon::Connector const& connector) const {
  Dungeon::ConnectorCandidates result;
  auto bucket = m_connectors_by_value.find(connector.value());
  if (bucket == m_connectors_by_value.end()) return result;
  auto same_part = [](auto a, auto b) { return &a.get().part() == &b.get().part(); };
  for (auto group : bucket->second | std::views::chunk_by(same_part)) {
    if (group.front().get().part().doesNotConnectTo(connector.part())) continue;
    for (auto connection : group) {
      auto connects = connection.get().accepts_direction_from(connector);
      if (!connects) return std::unexpected(std::move(connects.error()));
      if (*connects)
        result.push_back(connection);
    }
  }
  return result;
}

DungeonDefinition const& DungeonGenerator::definition() const {
  return *m_def;
}

}