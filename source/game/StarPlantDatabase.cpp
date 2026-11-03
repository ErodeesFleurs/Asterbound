import star.json;
import star.json_extra;
import star.bi_map;
import star.id_map;
import star.set;
import star.casting;
import star.data_stream;
import star.game_types;
import star.strong_typedef;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.string;
import star.format;
import star.version;

import star.tile_damage;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;

import star.plant_database;

namespace Star {

TreeVariant::TreeVariant()
  : stemHueShift(), foliageHueShift(), ceiling(), ephemeral() {}

std::expected<TreeVariant, StarException> TreeVariant::create(Assets const& assets, Json const& variant) {
  TreeVariant value;
  auto result147 = variant.getString("stemName");
if (!result147) return std::unexpected(std::move(result147.error()));
value.stemName = std::move(*result147);
  auto result148 = variant.getString("foliageName");
if (!result148) return std::unexpected(std::move(result148.error()));
value.foliageName = std::move(*result148);
  auto result149 = variant.getString("stemDirectory");
if (!result149) return std::unexpected(std::move(result149.error()));
value.stemDirectory = std::move(*result149);
  auto result150 = variant.get("stemSettings");
if (!result150) return std::unexpected(std::move(result150.error()));
value.stemSettings = std::move(*result150);
  auto result151 = variant.getFloat("stemHueShift");
if (!result151) return std::unexpected(std::move(result151.error()));
value.stemHueShift = std::move(*result151);
  auto result152 = variant.getString("foliageDirectory");
if (!result152) return std::unexpected(std::move(result152.error()));
value.foliageDirectory = std::move(*result152);
  auto result153 = variant.get("foliageSettings");
if (!result153) return std::unexpected(std::move(result153.error()));
value.foliageSettings = std::move(*result153);
  auto result154 = variant.getFloat("foliageHueShift");
if (!result154) return std::unexpected(std::move(result154.error()));
value.foliageHueShift = std::move(*result154);
  auto result155 = variant.get("descriptions");
if (!result155) return std::unexpected(std::move(result155.error()));
value.descriptions = std::move(*result155);
  auto result156 = variant.getBool("ceiling");
if (!result156) return std::unexpected(std::move(result156.error()));
value.ceiling = std::move(*result156);
  auto result157 = variant.getBool("ephemeral");
if (!result157) return std::unexpected(std::move(result157.error()));
value.ephemeral = std::move(*result157);
  auto result158 = variant.get("stemDropConfig");
if (!result158) return std::unexpected(std::move(result158.error()));
value.stemDropConfig = std::move(*result158);
  auto result159 = variant.get("foliageDropConfig");
if (!result159) return std::unexpected(std::move(result159.error()));
value.foliageDropConfig = std::move(*result159);
  auto result160 = variant.get("tileDamageParameters");
if (!result160) return std::unexpected(std::move(result160.error()));
auto result161 = TileDamageParameters::create(assets, std::move(*result160));
if (!result161) return std::unexpected(std::move(result161.error()));
value.tileDamageParameters = std::move(*result161);
  return value;
}

std::expected<Json, StarException> TreeVariant::toJson() const {
  auto result186 = tileDamageParameters.toJson();
if (!result186) return std::unexpected(std::move(result186.error()));
return JsonObject{
      {"stemName", stemName},
      {"foliageName", foliageName},
      {"stemDirectory", stemDirectory},
      {"stemSettings", stemSettings},
      {"stemHueShift", stemHueShift},
      {"foliageDirectory", foliageDirectory},
      {"foliageSettings", foliageSettings},
      {"foliageHueShift", foliageHueShift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"stemDropConfig", stemDropConfig},
      {"foliageDropConfig", foliageDropConfig},
      {"tileDamageParameters", std::move(*result186)},
  };
}

GrassVariant::GrassVariant() : hueShift(), ceiling(), ephemeral() {}

std::expected<GrassVariant, StarException> GrassVariant::create(Assets const& assets, Json const& variant) {
  GrassVariant value;
  auto result162 = variant.getString("name");
if (!result162) return std::unexpected(std::move(result162.error()));
value.name = std::move(*result162);
  auto result163 = variant.getString("directory");
if (!result163) return std::unexpected(std::move(result163.error()));
value.directory = std::move(*result163);
  auto result164 = variant.get("images");
if (!result164) return std::unexpected(std::move(result164.error()));
auto result165 = jsonToStringList(std::move(*result164));
if (!result165) return std::unexpected(std::move(result165.error()));
value.images = std::move(*result165);
  auto result166 = variant.getFloat("hueShift");
if (!result166) return std::unexpected(std::move(result166.error()));
value.hueShift = std::move(*result166);
  auto result167 = variant.get("descriptions");
if (!result167) return std::unexpected(std::move(result167.error()));
value.descriptions = std::move(*result167);
  auto result168 = variant.getBool("ceiling");
if (!result168) return std::unexpected(std::move(result168.error()));
value.ceiling = std::move(*result168);
  auto result169 = variant.getBool("ephemeral");
if (!result169) return std::unexpected(std::move(result169.error()));
value.ephemeral = std::move(*result169);
  auto result170 = variant.get("tileDamageParameters");
if (!result170) return std::unexpected(std::move(result170.error()));
auto result171 = TileDamageParameters::create(assets, std::move(*result170));
if (!result171) return std::unexpected(std::move(result171.error()));
value.tileDamageParameters = std::move(*result171);
  return value;
}

std::expected<Json, StarException> GrassVariant::toJson() const {
  auto result187 = tileDamageParameters.toJson();
if (!result187) return std::unexpected(std::move(result187.error()));
return JsonObject{{"name", name},
      {"directory", directory},
      {"images", jsonFromStringList(images)},
      {"hueShift", hueShift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"tileDamageParameters", std::move(*result187)}};
}

BushVariant::BushVariant() : baseHueShift(), modHueShift(), ceiling(), ephemeral() {}

std::expected<BushVariant, StarException> BushVariant::create(Assets const& assets, Json const& variant) {
  BushVariant value;
  auto result172 = variant.getString("bushName");
if (!result172) return std::unexpected(std::move(result172.error()));
value.bushName = std::move(*result172);
  auto result173 = variant.getString("modName");
if (!result173) return std::unexpected(std::move(result173.error()));
value.modName = std::move(*result173);
  auto result174 = variant.getString("directory");
if (!result174) return std::unexpected(std::move(result174.error()));
value.directory = std::move(*result174);
  auto result175 = variant.getArray("shapes");
if (!result175) return std::unexpected(std::move(result175.error()));
for (auto const& v : std::move(*result175)) {
auto result176 = v.getString(0);
if (!result176) return std::unexpected(std::move(result176.error()));
auto result177 = v.get(1);
if (!result177) return std::unexpected(std::move(result177.error()));
auto result178 = jsonToStringList(std::move(*result177));
if (!result178) return std::unexpected(std::move(result178.error()));
value.shapes.append(BushShape{std::move(*result176), std::move(*result178)});
}
  auto result179 = variant.getFloat("baseHueShift");
if (!result179) return std::unexpected(std::move(result179.error()));
value.baseHueShift = std::move(*result179);
  auto result180 = variant.getFloat("modHueShift");
if (!result180) return std::unexpected(std::move(result180.error()));
value.modHueShift = std::move(*result180);
  auto result181 = variant.get("descriptions");
if (!result181) return std::unexpected(std::move(result181.error()));
value.descriptions = std::move(*result181);
  auto result182 = variant.getBool("ceiling");
if (!result182) return std::unexpected(std::move(result182.error()));
value.ceiling = std::move(*result182);
  auto result183 = variant.getBool("ephemeral");
if (!result183) return std::unexpected(std::move(result183.error()));
value.ephemeral = std::move(*result183);
  auto result184 = variant.get("tileDamageParameters");
if (!result184) return std::unexpected(std::move(result184.error()));
auto result185 = TileDamageParameters::create(assets, std::move(*result184));
if (!result185) return std::unexpected(std::move(result185.error()));
value.tileDamageParameters = std::move(*result185);
  return value;
}

std::expected<Json, StarException> BushVariant::toJson() const {
  auto result188 = tileDamageParameters.toJson();
if (!result188) return std::unexpected(std::move(result188.error()));
return JsonObject{{"bushName", bushName},
      {"modName", modName},
      {"directory", directory},
      {"shapes", shapes.transformed([](BushShape const& shape) -> Json {
          return JsonArray{shape.image, jsonFromStringList(shape.mods)};
        })},
      {"baseHueShift", baseHueShift},
      {"modHueShift", modHueShift},
      {"descriptions", descriptions},
      {"ceiling", ceiling},
      {"ephemeral", ephemeral},
      {"tileDamageParameters", std::move(*result188)}};
}

std::expected<std::shared_ptr<PlantDatabase>, StarException> PlantDatabase::create(Root& root) {
  struct MakeShared final : PlantDatabase {
    explicit MakeShared(Root& root) : PlantDatabase(root) {}
  };
  auto database = std::make_shared<MakeShared>(root);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto load = [&]() -> std::expected<void, StarException> {
    auto stems = (*assets)->scanExtension("modularstem");
    if (!stems) return std::unexpected(std::move(stems.error()));
    auto foliages = (*assets)->scanExtension("modularfoliage");
    if (!foliages) return std::unexpected(std::move(foliages.error()));
    auto grasses = (*assets)->scanExtension("grass");
    if (!grasses) return std::unexpected(std::move(grasses.error()));
    auto bushes = (*assets)->scanExtension("bush");
    if (!bushes) return std::unexpected(std::move(bushes.error()));
    for (auto files : {*stems,*foliages,*grasses,*bushes})
      if (auto queued = (*assets)->queueJsons(files.get()); !queued) return std::unexpected(std::move(queued.error()));
    auto read = [&](auto const& files, auto& configs) -> std::expected<void, StarException> {
      for (auto const& scannedFile : files) {
        String file = scannedFile.string();
        auto loaded = [&]() -> std::expected<void, StarException> {
          auto config = (*assets)->json(file);
          if (!config) return std::unexpected(std::move(config.error()));
          auto name = config->getString("name");
          if (!name) return std::unexpected(std::move(name.error()));
          auto settings = config->toObject();
          if (!settings) return std::unexpected(std::move(settings.error()));
          configs.insert(std::move(*name), Config{AssetPath::directory(file), std::move(*settings)});
          return {};
        }();
        if (!loaded)
          return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), file, "Loading plant configuration"));
      }
      return {};
    };
    if (auto result = read(stems->get(),database->m_treeStemConfigs); !result) return result;
    if (auto result = read(foliages->get(),database->m_treeFoliageConfigs); !result) return result;
    if (auto result = read(grasses->get(),database->m_grassConfigs); !result) return result;
    return read(bushes->get(),database->m_bushConfigs);
  }();
  if (!load) return std::unexpected(PlantDatabaseException("Error loading plant database",load.error()));
  return database;
}

std::expected<StringList, StarException> PlantDatabase::treeStemNames(bool ceiling) const {
  StringList names;
  for (auto const& pair : m_treeStemConfigs) {
    auto result255 = pair.second.settings.getBool("ceiling", false);
if (!result255) return std::unexpected(std::move(result255.error()));
if (std::move(*result255) == ceiling)
      names.append(pair.first);
  }
  return names;
}

StringList PlantDatabase::treeFoliageNames() const {
  return m_treeFoliageConfigs.keys();
}

std::expected<String, StarException> PlantDatabase::treeStemShape(String const& stemName) const {
  auto result252 = m_treeStemConfigs.get(stemName);
if (!result252) return std::unexpected(std::move(result252.error()));
auto result253 = result252->get().settings.get("shape");
if (!result253) return std::unexpected(std::move(result253.error()));
auto result254 = std::move(*result253).toString();
if (!result254) return std::unexpected(std::move(result254.error()));
return std::move(*result254);
}

std::expected<String, StarException> PlantDatabase::treeFoliageShape(String const& foliageName) const {
  auto result249 = m_treeFoliageConfigs.get(foliageName);
if (!result249) return std::unexpected(std::move(result249.error()));
auto result250 = result249->get().settings.get("shape");
if (!result250) return std::unexpected(std::move(result250.error()));
auto result251 = std::move(*result250).toString();
if (!result251) return std::unexpected(std::move(result251.error()));
return std::move(*result251);
}

std::optional<String> PlantDatabase::treeStemDirectory(String const& stemName) const {
  if (auto stem = m_treeStemConfigs.maybe(stemName))
    return stem->directory;
  return {};
}

std::optional<String> PlantDatabase::treeFoliageDirectory(String const& foliageName) const {
  if (auto foliage = m_treeFoliageConfigs.maybe(foliageName))
    return foliage->directory;
  return {};
}

std::expected<TreeVariant, StarException> PlantDatabase::buildTreeVariant(
    String const& stemName, float stemHueShift, String const& foliageName, float foliageHueShift) const {
  if (!m_treeStemConfigs.contains(stemName) || !m_treeFoliageConfigs.contains(foliageName))
    return std::unexpected(PlantDatabaseException::format("stemName '{}' or foliageName '{}' not found in plant database", stemName, foliageName));

  TreeVariant treeVariant;

  treeVariant.stemName = stemName;
  treeVariant.foliageName = foliageName;

  auto result236 = m_treeStemConfigs.get(stemName);
if (!result236) return std::unexpected(std::move(result236.error()));
auto const& stemConfig = result236->get();
  treeVariant.stemDirectory = stemConfig.directory;
  treeVariant.stemSettings = stemConfig.settings;
  treeVariant.stemHueShift = stemHueShift;

  auto result237 = m_treeFoliageConfigs.get(foliageName);
if (!result237) return std::unexpected(std::move(result237.error()));
auto const& foliageConfig = result237->get();
  treeVariant.foliageDirectory = foliageConfig.directory;
  treeVariant.foliageSettings = foliageConfig.settings;
  treeVariant.foliageHueShift = foliageHueShift;

  auto result238 = stemConfig.settings.getBool("ceiling", false);
if (!result238) return std::unexpected(std::move(result238.error()));
treeVariant.ceiling = std::move(*result238);

  auto result239 = stemConfig.settings.get("dropConfig", JsonObject());
if (!result239) return std::unexpected(std::move(result239.error()));
treeVariant.stemDropConfig = std::move(*result239);
  auto result240 = foliageConfig.settings.get("dropConfig", JsonObject());
if (!result240) return std::unexpected(std::move(result240.error()));
treeVariant.foliageDropConfig = std::move(*result240);

  JsonObject descriptions;
  auto result241 = stemConfig.settings.iterateObject();
if (!result241) return std::unexpected(std::move(result241.error()));
for (auto const& entry : std::move(*result241)) {
    auto result242 = entry.first.endsWith("Description");
if (!result242) return std::unexpected(std::move(result242.error()));
if (std::move(*result242))
      descriptions[entry.first] = entry.second;
  }
  auto result243 = stemConfig.settings.getString("description", stemName + " with " + foliageName);
if (!result243) return std::unexpected(std::move(result243.error()));
descriptions["description"] = std::move(*result243);
  treeVariant.descriptions = descriptions;

  auto result244 = stemConfig.settings.getBool("allowsBlockPlacement", false);
if (!result244) return std::unexpected(std::move(result244.error()));
treeVariant.ephemeral = std::move(*result244);

  auto result245 = m_root.assets();
if (!result245) return std::unexpected(std::move(result245.error()));
auto result246 = stemConfig.settings.get("damageTable", "/plants/treeDamage.config");
if (!result246) return std::unexpected(std::move(result246.error()));
auto result247 = stemConfig.settings.getFloat("health", 1.0f);
if (!result247) return std::unexpected(std::move(result247.error()));
auto result248 = TileDamageParameters::create(*std::move(*result245),
      std::move(*result246),
      std::move(*result247));
if (!result248) return std::unexpected(std::move(result248.error()));
treeVariant.tileDamageParameters = std::move(*result248);

  return treeVariant;
}

std::expected<TreeVariant, StarException> PlantDatabase::buildTreeVariant(String const& stemName, float stemHueShift) const {
  if (!m_treeStemConfigs.contains(stemName))
    return std::unexpected(PlantDatabaseException(strf("stemName '{}' not found in plant database", stemName)));

  TreeVariant treeVariant;

  auto result225 = m_treeStemConfigs.get(stemName);
if (!result225) return std::unexpected(std::move(result225.error()));
auto const& stemConfig = result225->get();
  treeVariant.stemDirectory = stemConfig.directory;
  treeVariant.stemSettings = stemConfig.settings;
  treeVariant.stemHueShift = stemHueShift;

  auto result226 = stemConfig.settings.getBool("ceiling", false);
if (!result226) return std::unexpected(std::move(result226.error()));
treeVariant.ceiling = std::move(*result226);

  auto result227 = stemConfig.settings.get("dropConfig", JsonObject());
if (!result227) return std::unexpected(std::move(result227.error()));
treeVariant.stemDropConfig = std::move(*result227);

  treeVariant.foliageSettings = JsonObject();
  treeVariant.foliageDropConfig = JsonObject();

  JsonObject descriptions;
  auto result228 = stemConfig.settings.iterateObject();
if (!result228) return std::unexpected(std::move(result228.error()));
for (auto const& entry : std::move(*result228)) {
    auto result229 = entry.first.endsWith("Description");
if (!result229) return std::unexpected(std::move(result229.error()));
if (std::move(*result229))
      descriptions[entry.first] = entry.second;
  }
  auto result230 = stemConfig.settings.getString("description", stemName);
if (!result230) return std::unexpected(std::move(result230.error()));
descriptions["description"] = std::move(*result230);
  treeVariant.descriptions = descriptions;

  auto result231 = stemConfig.settings.getBool("ephemeral", false);
if (!result231) return std::unexpected(std::move(result231.error()));
treeVariant.ephemeral = std::move(*result231);

  auto result232 = m_root.assets();
if (!result232) return std::unexpected(std::move(result232.error()));
auto result233 = stemConfig.settings.get("damageTable", "/plants/treeDamage.config");
if (!result233) return std::unexpected(std::move(result233.error()));
auto result234 = stemConfig.settings.getFloat("health", 1.0f);
if (!result234) return std::unexpected(std::move(result234.error()));
auto result235 = TileDamageParameters::create(*std::move(*result232),
      std::move(*result233),
      std::move(*result234));
if (!result235) return std::unexpected(std::move(result235.error()));
treeVariant.tileDamageParameters = std::move(*result235);

  return treeVariant;
}

std::expected<StringList, StarException> PlantDatabase::grassNames(bool ceiling) const {
  StringList names;
  for (auto const& pair : m_grassConfigs) {
    auto result224 = pair.second.settings.getBool("ceiling", false);
if (!result224) return std::unexpected(std::move(result224.error()));
if (std::move(*result224) == ceiling)
      names.append(pair.first);
  }
  return names;
}

std::expected<GrassVariant, StarException> PlantDatabase::buildGrassVariant(String const& name, float hueShift) const {
  if (!m_grassConfigs.contains(name))
    return std::unexpected(PlantDatabaseException(strf("grass '{}' not found in plant database", name)));

  GrassVariant grassVariant;
  auto result212 = m_grassConfigs.get(name);
if (!result212) return std::unexpected(std::move(result212.error()));
auto const& config = result212->get();

  grassVariant.name = name;
  grassVariant.directory = config.directory;
  auto result213 = config.settings.get("images");
if (!result213) return std::unexpected(std::move(result213.error()));
auto result214 = jsonToStringList(std::move(*result213));
if (!result214) return std::unexpected(std::move(result214.error()));
grassVariant.images = std::move(*result214);
  grassVariant.hueShift = hueShift;
  auto result215 = config.settings.getBool("ceiling", false);
if (!result215) return std::unexpected(std::move(result215.error()));
grassVariant.ceiling = std::move(*result215);

  JsonObject descriptions;
  auto result216 = config.settings.iterateObject();
if (!result216) return std::unexpected(std::move(result216.error()));
for (auto const& entry : std::move(*result216)) {
    auto result217 = entry.first.endsWith("Description");
if (!result217) return std::unexpected(std::move(result217.error()));
if (std::move(*result217))
      descriptions[entry.first] = entry.second;
  }
  auto result218 = config.settings.getString("description", name);
if (!result218) return std::unexpected(std::move(result218.error()));
descriptions["description"] = std::move(*result218);
  grassVariant.descriptions = descriptions;

  auto result219 = config.settings.getBool("ephemeral", true);
if (!result219) return std::unexpected(std::move(result219.error()));
grassVariant.ephemeral = std::move(*result219);
  auto result220 = m_root.assets();
if (!result220) return std::unexpected(std::move(result220.error()));
auto result221 = config.settings.get("damageTable", "/plants/grassDamage.config");
if (!result221) return std::unexpected(std::move(result221.error()));
auto result222 = config.settings.getFloat("health", 1.0f);
if (!result222) return std::unexpected(std::move(result222.error()));
auto result223 = TileDamageParameters::create(*std::move(*result220),
      std::move(*result221),
      std::move(*result222));
if (!result223) return std::unexpected(std::move(result223.error()));
grassVariant.tileDamageParameters = std::move(*result223);

  return grassVariant;
}

std::expected<StringList, StarException> PlantDatabase::bushNames(bool ceiling) const {
  StringList names;
  for (auto const& pair : m_bushConfigs) {
    auto result211 = pair.second.settings.getBool("ceiling");
if (!result211) return std::unexpected(std::move(result211.error()));
if (std::move(*result211) == ceiling)
      names.append(pair.first);
  }
  return names;
}

std::expected<StringList, StarException> PlantDatabase::bushMods(String const& bushName) const {
  auto config = m_bushConfigs.get(bushName);
  if (!config) return std::unexpected(std::move(config.error()));
  auto mods = config->get().settings.opt("mods");
  if (!mods) return std::unexpected(std::move(mods.error()));
  if (!*mods) return StringList{};
  return jsonToStringList(**mods);
}

std::expected<BushVariant, StarException> PlantDatabase::buildBushVariant(String const& bushName, float baseHueShift, String const& modName, float modHueShift) const {
  if (!m_bushConfigs.contains(bushName))
    return std::unexpected(PlantDatabaseException(strf("bush '{}' not found in plant database", bushName)));

  BushVariant bushVariant;
  auto result193 = m_bushConfigs.get(bushName);
if (!result193) return std::unexpected(std::move(result193.error()));
auto const& config = result193->get();

  bushVariant.bushName = bushName;
  bushVariant.modName = modName;
  bushVariant.directory = config.directory;
  auto result194 = config.settings.get("shapes");
if (!result194) return std::unexpected(std::move(result194.error()));
auto result195 = std::move(*result194).toArray();
if (!result195) return std::unexpected(std::move(result195.error()));
auto shapes = std::move(*result195);
  for (auto shapeVar : shapes) {
    auto result196 = shapeVar.toObject();
if (!result196) return std::unexpected(std::move(result196.error()));
auto shapeMap = std::move(*result196);
    auto result197 = shapeMap.get("base");
if (!result197) return std::unexpected(std::move(result197.error()));
auto result198 = result197->get().toString();
if (!result198) return std::unexpected(std::move(result198.error()));
auto base = std::move(*result198);
    StringList mods;
    if (!modName.empty())
      {
auto result199 = shapeMap.get("mods");
if (!result199) return std::unexpected(std::move(result199.error()));
auto result200 = result199->get().get(modName, JsonArray());
if (!result200) return std::unexpected(std::move(result200.error()));
auto result201 = jsonToStringList(std::move(*result200));
if (!result201) return std::unexpected(std::move(result201.error()));
mods = std::move(*result201);
}
    bushVariant.shapes.push_back({base, mods});
  }
  bushVariant.baseHueShift = baseHueShift;
  bushVariant.modHueShift = modHueShift;
  auto result202 = config.settings.getBool("ceiling", false);
if (!result202) return std::unexpected(std::move(result202.error()));
bushVariant.ceiling = std::move(*result202);

  JsonObject descriptions;
  auto result203 = config.settings.iterateObject();
if (!result203) return std::unexpected(std::move(result203.error()));
for (auto const& entry : std::move(*result203)) {
    auto result204 = entry.first.endsWith("Description");
if (!result204) return std::unexpected(std::move(result204.error()));
if (std::move(*result204))
      descriptions[entry.first] = entry.second;
  }
  auto result205 = config.settings.getString("description", bushName + " with " + modName);
if (!result205) return std::unexpected(std::move(result205.error()));
descriptions["description"] = std::move(*result205);
  bushVariant.descriptions = descriptions;

  auto result206 = config.settings.getBool("ephemeral", true);
if (!result206) return std::unexpected(std::move(result206.error()));
bushVariant.ephemeral = std::move(*result206);
  auto result207 = m_root.assets();
if (!result207) return std::unexpected(std::move(result207.error()));
auto result208 = config.settings.get("damageTable", "/plants/bushDamage.config");
if (!result208) return std::unexpected(std::move(result208.error()));
auto result209 = config.settings.getFloat("health", 1.0f);
if (!result209) return std::unexpected(std::move(result209.error()));
auto result210 = TileDamageParameters::create(*std::move(*result207),
      std::move(*result208),
      std::move(*result209));
if (!result210) return std::unexpected(std::move(result210.error()));
bushVariant.tileDamageParameters = std::move(*result210);
  return bushVariant;
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::createPlant(TreeVariant const& treeVariant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, treeVariant, seed);
  if (!plant) return std::unexpected(PlantDatabaseException(strf("Error constructing plant from tree variant stem: {} foliage: {}", treeVariant.stemName, treeVariant.foliageName),plant.error()));
  return plant;
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::createPlant(GrassVariant const& grassVariant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, grassVariant, seed);
  if (!plant) return std::unexpected(PlantDatabaseException(strf("Error constructing plant from grass variant name: {}", grassVariant.name),plant.error()));
  return plant;
}

std::expected<std::shared_ptr<Plant>, StarException> PlantDatabase::createPlant(BushVariant const& bushVariant, std::uint64_t seed) const {
  auto plant = Plant::create(m_root, bushVariant, seed);
  if (!plant) return std::unexpected(PlantDatabaseException(strf("Error constructing plant from bush variant name: {} mod: {}", bushVariant.bushName, bushVariant.modName),plant.error()));
  return plant;
}

Json PlantDatabase::Config::toJson() const {
  return JsonObject{
    {"config", settings},
    {"path", directory}
  };
}


std::expected<Json, StarException> PlantDatabase::treeFoliageConfig(String const & foliageName) const {
  auto config = m_treeFoliageConfigs.get(foliageName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().toJson();
}

std::expected<Json, StarException> PlantDatabase::treeStemConfig(String const & stemName) const {
  auto config = m_treeStemConfigs.get(stemName);
  if (!config) return std::unexpected(std::move(config.error()));
  return config->get().toJson();
}

}
