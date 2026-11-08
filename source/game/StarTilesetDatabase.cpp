import star.json;
import star.ordered_map;
import star.bi_map;
import star.thread;
import star.io_device;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.ref_ptr;
import star.casting;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.output_proxy;
import star.version;
import star.image;
import star.game_types;
import star.material_types;
import star.random;
import star.lexical_cast;
import star.lru_cache;
import star.set;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;
import star.tileset_database;

namespace Star {
namespace Tiled {
  using namespace Dungeon;
  extern std::expected<EnumMap<TileLayer>, StarException> const LayerNames = EnumMap<TileLayer>::create({{TileLayer::Foreground, "front"}, {TileLayer::Background, "back"}});

  Properties::Properties() : m_properties(JsonObject{}) {}
  Properties::Properties(Json const& json) : m_properties(json) {}
  Json Properties::toJson() const { return m_properties; }
  std::expected<Properties, StarException> Properties::inherit(Json const& properties) const {
    return Properties(jsonMerge(properties, m_properties));
  }
  std::expected<Properties, StarException> Properties::inherit(Properties const& properties) const {
    return Properties(jsonMerge(properties.m_properties, m_properties));
  }
  std::expected<bool, StarException> Properties::contains(String const& name) const {
    return m_properties.contains(name);
  }

  // Unlike opt(), explicit presence must still reject a null string value.
  template <typename T>
  [[nodiscard]] std::expected<std::optional<T>, StarException> presentProperty(Properties const& properties, String const& name) {
    auto present = properties.contains(name);
    if (!present) return std::unexpected(std::move(present.error()));
    if (!*present) return std::optional<T>{};
    auto value = properties.get<T>(name);
    if (!value) return std::unexpected(std::move(value.error()));
    return std::optional<T>(std::move(*value));
  }

  [[nodiscard]] std::expected<TileLayer, StarException> propertyLayer(Properties const& properties) {
    auto value = properties.get<String>("layer");
    if (!value) return std::unexpected(std::move(value.error()));
    if (!LayerNames) return std::unexpected(LayerNames.error());
    auto layer = LayerNames->get_left(*value);
    if (!layer) return std::unexpected(std::move(layer.error()));
    return layer->get();
  }

  [[nodiscard]] std::expected<std::optional<std::shared_ptr<Brush const>>, StarException> getClearBrush(bool value, Properties&) {
    if (value) return std::optional<std::shared_ptr<Brush const>>(std::make_shared<ClearBrush>());
    return std::optional<std::shared_ptr<Brush const>>{};
  }

  template <typename MaterialBrush>
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getLayerBrush(String const& name, Properties& properties) {
    auto hue = properties.opt<float>("hueshift");
    if (!hue) return std::unexpected(std::move(hue.error()));
    auto color = properties.opt<std::size_t>("colorVariant");
    if (!color) return std::unexpected(std::move(color.error()));
    auto mod = properties.opt<String>("mod");
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto modHue = properties.opt<float>("modhueshift");
    if (!modHue) return std::unexpected(std::move(modHue.error()));
    std::optional<MaterialColorVariant> colorVariant;
    if (*color) colorVariant = static_cast<MaterialColorVariant>(**color);
    return std::make_shared<MaterialBrush>(name, *mod, *hue, *modHue, colorVariant);
  }

  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getMaterialBrush(String const& name, Properties& properties) {
    auto layer = propertyLayer(properties);
    if (!layer) return std::unexpected(std::move(layer.error()));
    if (*layer == TileLayer::Background) return getLayerBrush<BackBrush>(name, properties);
    return getLayerBrush<FrontBrush>(name, properties);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getPlayerStartBrush(String const&, Properties&) {
    return std::make_shared<PlayerStartBrush>();
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getObjectBrush(String const& name, Properties& properties) {
    Star::Direction direction = Star::Direction::Right;
    auto configured = presentProperty<String>(properties, "tilesetDirection");
    if (!configured) return std::unexpected(std::move(configured.error()));
    if (*configured) {
      auto parsed = enumValue<DirectionNames>(**configured);
      if (!parsed) return std::unexpected(std::move(parsed.error()));
      direction = *parsed;
    }
    auto flip = properties.contains("flipX");
    if (!flip) return std::unexpected(std::move(flip.error()));
    if (*flip) direction = -direction;
    auto parameters = presentProperty<Json>(properties, "parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto configuredParameters = parameters->value_or(JsonObject{});
    if (configuredParameters.isNull()) configuredParameters = JsonObject{};
    return std::make_shared<ObjectBrush>(name, direction, configuredParameters);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getVehicleBrush(String const& name, Properties& properties) {
    auto parameters = presentProperty<Json>(properties, "parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    return std::make_shared<VehicleBrush>(name, parameters->value_or(JsonObject{}));
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getWireBrush(String const& group, Properties& properties) {
    auto local = properties.opt<bool>("local");
    if (!local) return std::unexpected(std::move(local.error()));
    return std::make_shared<WireBrush>(group, local->value_or(true));
  }
  [[nodiscard]] std::expected<Json, StarException> getSeed(Properties& properties) {
    auto seed = properties.get<String>("seed");
    if (!seed) return std::unexpected(std::move(seed.error()));
    if (*seed == "stable") return Json(*seed);
    auto value = lexicalCast<std::uint64_t>(*seed);
    if (!value) return std::unexpected(std::move(value.error()));
    return Json(*value);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getNpcBrush(Root& root, String const& species, Properties& properties) {
    JsonObject brush{{"kind", "npc"}, {"species", species}};
    auto hasSeed = properties.contains("seed");
    if (!hasSeed) return std::unexpected(std::move(hasSeed.error()));
    if (*hasSeed) {
      auto seed = getSeed(properties);
      if (!seed) return std::unexpected(std::move(seed.error()));
      brush["seed"] = std::move(*seed);
    }
    auto type = presentProperty<String>(properties, "typeName");
    if (!type) return std::unexpected(std::move(type.error()));
    if (*type) brush["typeName"] = **type;
    auto parameters = properties.opt<Json>("parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    brush["parameters"] = parameters->value_or(JsonObject{});
    auto result = NpcBrush::create(root, brush);
    if (!result) return std::unexpected(std::move(result.error()));
    return std::move(*result);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getMonsterBrush(Root& root, String const& type, Properties& properties) {
    JsonObject brush{{"kind", "monster"}, {"typeName", type}};
    auto hasSeed = properties.contains("seed");
    if (!hasSeed) return std::unexpected(std::move(hasSeed.error()));
    if (*hasSeed) {
      auto seed = getSeed(properties);
      if (!seed) return std::unexpected(std::move(seed.error()));
      brush["seed"] = std::move(*seed);
    }
    auto parameters = properties.opt<Json>("parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    brush["parameters"] = parameters->value_or(JsonObject{});
    auto result = NpcBrush::create(root, brush);
    if (!result) return std::unexpected(std::move(result.error()));
    return std::move(*result);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getStagehandBrush(String const& type, Properties& properties) {
    auto parameters = properties.opt<Json>("parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    Json configured = parameters->value_or(JsonObject{});
    auto broadcast = presentProperty<Json>(properties, "broadcastArea");
    if (!broadcast) return std::unexpected(std::move(broadcast.error()));
    if (*broadcast) {
      auto updated = configured.set("broadcastArea", **broadcast);
      if (!updated) return std::unexpected(std::move(updated.error()));
      configured = std::move(*updated);
    }
    if (type == "radiomessage") {
      auto message = presentProperty<Json>(properties, "radioMessage");
      if (!message) return std::unexpected(std::move(message.error()));
      if (*message) {
        auto updated = configured.set("radioMessage", **message);
        if (!updated) return std::unexpected(std::move(updated.error()));
        configured = std::move(*updated);
      }
    }
    return std::make_shared<StagehandBrush>(JsonObject{{"type", type}, {"parameters", configured}});
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getDungeonIdBrush(String const& value, Properties&) {
    return std::make_shared<DungeonIdBrush>(maybeLexicalCast<DungeonId>(value).value_or(NoDungeonId));
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getBiomeItemsBrush(String const&, Properties&) {
    return std::make_shared<BiomeItemsBrush>();
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getBiomeTreeBrush(String const&, Properties&) {
    return std::make_shared<BiomeTreeBrush>();
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getItemBrush(String const& name, Properties& properties) {
    auto count = properties.opt<std::size_t>("count");
    if (!count) return std::unexpected(std::move(count.error()));
    auto parameters = properties.opt<Json>("parameters");
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto item = ItemDescriptor::create(name, count->value_or(1), parameters->value_or(JsonObject{}));
    if (!item) return std::unexpected(std::move(item.error()));
    return std::make_shared<ItemBrush>(*item);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getSurfaceBrush(String const& variantText, Properties& properties) {
    auto layer = propertyLayer(properties);
    if (!layer) return std::unexpected(std::move(layer.error()));
    auto mod = properties.opt<String>("mod");
    if (!mod) return std::unexpected(std::move(mod.error()));
    auto variant = maybeLexicalCast<int>(variantText);
    if (*layer == TileLayer::Background) return std::make_shared<SurfaceBackgroundBrush>(variant, *mod);
    return std::make_shared<SurfaceBrush>(variant, *mod);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Brush const>, StarException> getLiquidBrush(String const& name, Properties& properties) {
    auto quantity = properties.opt<float>("quantity");
    if (!quantity) return std::unexpected(std::move(quantity.error()));
    auto source = properties.opt<bool>("source");
    if (!source) return std::unexpected(std::move(source.error()));
    return std::make_shared<LiquidBrush>(name, quantity->value_or(1.0f), source->value_or(false));
  }
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<Brush const>>, StarException> getInvalidBrush(bool invalid, Properties& properties) {
    if (!invalid) return std::optional<std::shared_ptr<Brush const>>{};
    auto name = properties.opt<String>("//name");
    if (!name) return std::unexpected(std::move(name.error()));
    return std::optional<std::shared_ptr<Brush const>>(std::make_shared<InvalidBrush>(*name));
  }
  [[nodiscard]] std::expected<std::shared_ptr<Rule const>, StarException> getAirRule(String const&, Properties& properties) {
    auto layer = propertyLayer(properties);
    if (!layer) return std::unexpected(std::move(layer.error()));
    return std::make_shared<WorldGenMustContainAirRule>(*layer);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Rule const>, StarException> getSolidRule(String const&, Properties& properties) {
    auto layer = propertyLayer(properties);
    if (!layer) return std::unexpected(std::move(layer.error()));
    return std::make_shared<WorldGenMustContainSolidRule>(*layer);
  }
  [[nodiscard]] std::expected<std::shared_ptr<Rule const>, StarException> getLiquidRule(String const&, Properties&) {
    return std::make_shared<WorldGenMustContainLiquidRule>();
  }
  [[nodiscard]] std::expected<std::shared_ptr<Rule const>, StarException> getNotLiquidRule(String const&, Properties&) {
    return std::make_shared<WorldGenMustNotContainLiquidRule>();
  }
  [[nodiscard]] std::expected<std::shared_ptr<Rule const>, StarException> getAllowOverdrawingRule(String const&, Properties&) {
    return std::make_shared<AllowOverdrawingRule>();
  }

  template <typename T>
  class PropertyReader {
  public:
    template <typename PropertyType, typename Getter>
    [[nodiscard]] std::expected<void, StarException> optRead(List<T>& list, String const& name, Getter getter, Properties& properties) {
      auto value = properties.opt<PropertyType>(name);
      if (!value) return std::unexpected(std::move(value.error()));
      if (!*value) return {};
      auto converted = getter(**value, properties);
      if (!converted) return std::unexpected(std::move(converted.error()));
      if constexpr (std::is_same_v<typename decltype(converted)::value_type, std::optional<T>>) {
        if (*converted)
          list.push_back(std::move(**converted));
      } else {
        list.push_back(std::move(*converted));
      }
      return {};
    }
  };

  std::expected<std::shared_ptr<Tile>, StarException> Tile::create(Root& root, Properties const& properties, TileLayer layer, bool flipX) {
    auto result = std::shared_ptr<Tile>(new Tile(properties));
    auto initialized = result->init(root, layer, flipX);
    if (!initialized) return std::unexpected(std::move(initialized.error()));
    return result;
  }
  std::expected<void, StarException> Tile::init(Root& root, TileLayer layer, bool flipX) {
    JsonObject computed;
    auto hasLayer = properties.contains("layer");
    if (!hasLayer) return std::unexpected(std::move(hasLayer.error()));
    if (!*hasLayer) {
      if (!LayerNames) return std::unexpected(LayerNames.error());
      auto name = LayerNames->get_right(layer);
      if (!name) return std::unexpected(std::move(name.error()));
      computed["layer"] = name->get();
    } else {
      auto configured = propertyLayer(properties);
      if (!configured) return std::unexpected(std::move(configured.error()));
      layer = *configured;
    }
    if (flipX) computed["flipX"] = "true";
    if (layer == TileLayer::Background) {
      auto clear = properties.contains("clear");
      if (!clear) return std::unexpected(std::move(clear.error()));
      if (!*clear) computed["clear"] = "true";
    }
    auto inherited = properties.inherit(computed);
    if (!inherited) return std::unexpected(std::move(inherited.error()));
    properties = std::move(*inherited);
    PropertyReader<std::shared_ptr<Brush const>> br;
    auto clear = br.optRead<bool>(brushes, "clear", getClearBrush, properties);
    if (!clear) return std::unexpected(std::move(clear.error()));
    auto material = br.optRead<String>(brushes, "material", getMaterialBrush, properties);
    if (!material) return std::unexpected(std::move(material.error()));
    auto front = br.optRead<String>(brushes, "front", getLayerBrush<FrontBrush>, properties);
    if (!front) return std::unexpected(std::move(front.error()));
    auto back = br.optRead<String>(brushes, "back", getLayerBrush<BackBrush>, properties);
    if (!back) return std::unexpected(std::move(back.error()));
    auto player = br.optRead<String>(brushes, "playerstart", getPlayerStartBrush, properties);
    if (!player) return std::unexpected(std::move(player.error()));
    auto object = br.optRead<String>(brushes, "object", getObjectBrush, properties);
    if (!object) return std::unexpected(std::move(object.error()));
    auto vehicle = br.optRead<String>(brushes, "vehicle", getVehicleBrush, properties);
    if (!vehicle) return std::unexpected(std::move(vehicle.error()));
    auto wire = br.optRead<String>(brushes, "wire", getWireBrush, properties);
    if (!wire) return std::unexpected(std::move(wire.error()));
    auto npc = br.optRead<String>(brushes, "npc", std::bind_front(getNpcBrush, std::ref(root)), properties);
    if (!npc) return std::unexpected(std::move(npc.error()));
    auto monster = br.optRead<String>(brushes, "monster", std::bind_front(getMonsterBrush, std::ref(root)), properties);
    if (!monster) return std::unexpected(std::move(monster.error()));
    auto stagehand = br.optRead<String>(brushes, "stagehand", getStagehandBrush, properties);
    if (!stagehand) return std::unexpected(std::move(stagehand.error()));
    auto dungeonId = br.optRead<String>(brushes, "dungeonid", getDungeonIdBrush, properties);
    if (!dungeonId) return std::unexpected(std::move(dungeonId.error()));
    auto biomeItems = br.optRead<String>(brushes, "biomeitems", getBiomeItemsBrush, properties);
    if (!biomeItems) return std::unexpected(std::move(biomeItems.error()));
    auto biomeTree = br.optRead<String>(brushes, "biometree", getBiomeTreeBrush, properties);
    if (!biomeTree) return std::unexpected(std::move(biomeTree.error()));
    auto item = br.optRead<String>(brushes, "item", getItemBrush, properties);
    if (!item) return std::unexpected(std::move(item.error()));
    auto surface = br.optRead<String>(brushes, "surface", getSurfaceBrush, properties);
    if (!surface) return std::unexpected(std::move(surface.error()));
    auto liquid = br.optRead<String>(brushes, "liquid", getLiquidBrush, properties);
    if (!liquid) return std::unexpected(std::move(liquid.error()));
    auto invalid = br.optRead<bool>(brushes, "invalid", getInvalidBrush, properties);
    if (!invalid) return std::unexpected(std::move(invalid.error()));
    PropertyReader<std::shared_ptr<Rule const>> rr;
    auto air = rr.optRead<String>(rules, "worldGenMustContainAir", getAirRule, properties);
    if (!air) return std::unexpected(std::move(air.error()));
    auto solid = rr.optRead<String>(rules, "worldGenMustContainSolid", getSolidRule, properties);
    if (!solid) return std::unexpected(std::move(solid.error()));
    auto containsLiquid = rr.optRead<String>(rules, "worldGenMustContainLiquid", getLiquidRule, properties);
    if (!containsLiquid) return std::unexpected(std::move(containsLiquid.error()));
    auto notLiquid = rr.optRead<String>(rules, "worldGenMustNotContainLiquid", getNotLiquidRule, properties);
    if (!notLiquid) return std::unexpected(std::move(notLiquid.error()));
    auto overdraw = rr.optRead<String>(rules, "allowOverdrawing", getAllowOverdrawingRule, properties);
    if (!overdraw) return std::unexpected(std::move(overdraw.error()));
    auto name = properties.opt<String>("connector");
    if (!name) return std::unexpected(std::move(name.error()));
    if (*name) {
      TileConnector configured;
      configured.value = **name;
      auto forward = properties.opt<bool>("connectForwardOnly");
      if (!forward) return std::unexpected(std::move(forward.error()));
      configured.forwardOnly = forward->value_or(false);
      auto direction = properties.opt<String>("connectDirection");
      if (!direction) return std::unexpected(std::move(direction.error()));
      if (*direction) {
        auto parsed = enumValue<DungeonDirectionNames>(**direction);
        if (!parsed) return std::unexpected(std::move(parsed.error()));
        configured.direction = *parsed;
      }
      connector = std::move(configured);
    }
    return {};
  }

  std::expected<std::shared_ptr<Tileset>, StarException> Tileset::create(Root& root, Json const& json) {
    auto result = std::shared_ptr<Tileset>(new Tileset);
    auto properties = json.opt("properties");
    if (!properties) return std::unexpected(std::move(properties.error()));
    Properties defaults(properties->value_or(JsonObject{}));
    auto tileProperties = json.opt("tileproperties");
    if (!tileProperties) return std::unexpected(std::move(tileProperties.error()));
    auto count = json.getInt("tilecount");
    if (!count) return std::unexpected(std::move(count.error()));
    if (*count < 0) return std::unexpected(StarException("Negative tile count in tileset"));
    result->m_tilesBack.resize(*count);
    result->m_tilesFront.resize(*count);
    auto entries = tileProperties->value_or(JsonObject{}).iterateObject();
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entry : *entries) {
      auto index = lexicalCast<std::size_t>(entry.first);
      if (!index) return std::unexpected(std::move(index.error()));
      if (*index >= result->m_tilesBack.size())
        return std::unexpected(StarException::format("Tile index {} exceeds tileset size {}", *index, result->m_tilesBack.size()));
      auto inherited = Properties(entry.second).inherit(defaults);
      if (!inherited) return std::unexpected(std::move(inherited.error()));
      auto back = Tile::create(root, *inherited, TileLayer::Background, false);
      if (!back) return std::unexpected(std::move(back.error()));
      result->m_tilesBack[*index] = std::move(*back);
      auto front = Tile::create(root, *inherited, TileLayer::Foreground, false);
      if (!front) return std::unexpected(std::move(front.error()));
      result->m_tilesFront[*index] = std::move(*front);
    }
    return result;
  }
  std::expected<std::shared_ptr<Tile const>, StarException> Tileset::getTile(std::size_t id, TileLayer layer) const {
    auto const& values = tiles(layer);
    if (id >= values.size()) return std::unexpected(StarException::format("Tile index {} exceeds tileset size {}", id, values.size()));
    return values[id];
  }
  std::size_t Tileset::size() const {
    if constexpr (Star::DebugEnabled) Star::starAssertFailed((m_tilesBack.size() == m_tilesFront.size()));
    return m_tilesBack.size();
  }
  List<std::shared_ptr<Tile const>> const& Tileset::tiles(TileLayer layer) const {
    if (layer == TileLayer::Background) return m_tilesBack;
    if constexpr (Star::DebugEnabled) Star::starAssertFailed((layer == TileLayer::Foreground));
    return m_tilesFront;
  }

  std::expected<Json, StarException> PropertyConverter<Json>::to(String const& value) {
    auto parsed = Json::parseJson(value);
    if (!parsed) {
      if (parsed.error().is<JsonParsingException>())
        return std::unexpected(parsed.error().withContext("Error parsing Tiled property as Json"));
      return std::unexpected(std::move(parsed.error()));
    }
    return std::move(*parsed);
  }
  std::expected<String, StarException> PropertyConverter<Json>::from(Json const& value) { return value.repr(); }
  std::expected<String, StarException> PropertyConverter<String>::to(String const& value) { return value; }
  std::expected<String, StarException> PropertyConverter<String>::from(String const& value) { return value; }
}

std::expected<std::shared_ptr<TilesetDatabase>, StarException> TilesetDatabase::create(Root& root) {
  return std::shared_ptr<TilesetDatabase>(new TilesetDatabase(root));
}
std::expected<std::shared_ptr<Tiled::Tileset const>, StarException> TilesetDatabase::get(String const& path) const {
  std::lock_guard locker(m_cacheMutex);
  auto result = m_tilesetCache.get(path, [&database = *this](String const& path) { return database.readTileset(path); });
  if (!result) return std::unexpected(std::move(result.error()));
  return result->get();
}
std::expected<std::shared_ptr<Tiled::Tileset const>, StarException> TilesetDatabase::readTileset(String const& path) const {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto json = (*assets)->json(path);
  if (!json) return std::unexpected((*assets)->withAssetContext(std::move(json.error()), path, "Loading tileset"));
  auto result = Tiled::Tileset::create(m_root, *json);
  if (!result) return std::unexpected((*assets)->withAssetContext(std::move(result.error()), path, "Loading tileset"));
  return std::move(*result);
}
}
