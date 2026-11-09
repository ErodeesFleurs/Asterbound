import std;
import star.game_types;
import star.bi_map;
import star.json;
import star.json_extra;
import star.data_stream;
import star.data_stream_extra;
import star.string;
import star.format;
import star.random;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.root;
import star.versioning_database;
namespace Star {
bool QuestItem::operator==(QuestItem const& rhs) const { return itemName == rhs.itemName && parameters == rhs.parameters; }
std::expected<ItemDescriptor, StarException> QuestItem::descriptor() const { return ItemDescriptor::create(itemName, 1, parameters); }
bool QuestEntity::operator==(QuestEntity const& rhs) const { return uniqueId == rhs.uniqueId && species == rhs.species && gender == rhs.gender; }
bool QuestLocation::operator==(QuestLocation const& rhs) const { return uniqueId == rhs.uniqueId && region == rhs.region; }
bool QuestMonsterType::operator==(QuestMonsterType const& rhs) const { return typeName == rhs.typeName && parameters == rhs.parameters; }
bool QuestNpcType::operator==(QuestNpcType const& rhs) const { return species == rhs.species && typeName == rhs.typeName && parameters == rhs.parameters && seed == rhs.seed; }
bool QuestCoordinate::operator==(QuestCoordinate const& rhs) const { return coordinate == rhs.coordinate; }

[[nodiscard]] std::expected<QuestParamDetail, StarException> questParamDetailFromJson(Json const& json) {
  auto type = json.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  if (*type == "item") {
    auto spec = json.get("item");
    if (!spec) return std::unexpected(std::move(spec.error()));
    auto item = ItemDescriptor::create(*spec);
    if (!item) return std::unexpected(std::move(item.error()));
    return QuestParamDetail(QuestItem{item->name(), item->parameters()});
  } else if (*type == "itemTag") {
    auto tag = json.getString("tag");
    if (!tag) return std::unexpected(std::move(tag.error()));
    return QuestParamDetail(QuestItemTag(*tag));
  } else if (*type == "itemList") {
    auto specs = json.getArray("items");
    if (!specs) return std::unexpected(std::move(specs.error()));
    QuestItemList items;
    for (auto const& spec : *specs) {
      auto item = ItemDescriptor::create(spec);
      if (!item) return std::unexpected(std::move(item.error()));
      items.push_back(std::move(*item));
    }
    return QuestParamDetail(std::move(items));
  } else if (*type == "entity") {
    auto id = json.optString("uniqueId");
    if (!id) return std::unexpected(std::move(id.error()));
    auto species = json.optString("species");
    if (!species) return std::unexpected(std::move(species.error()));
    auto name = json.optString("gender");
    if (!name) return std::unexpected(std::move(name.error()));
    std::optional<Gender> gender;
    if (*name) {
      auto value = enumValue<GenderNames>(**name);
      if (!value) return std::unexpected(std::move(value.error()));
      gender = *value;
    }
    return QuestParamDetail(QuestEntity{*id, *species, gender});
  } else if (*type == "location") {
    auto id = json.optString("uniqueId");
    if (!id) return std::unexpected(std::move(id.error()));
    auto regionJson = json.get("region");
    if (!regionJson) return std::unexpected(std::move(regionJson.error()));
    auto region = jsonToRectF(*regionJson);
    if (!region) return std::unexpected(std::move(region.error()));
    return QuestParamDetail(QuestLocation{*id, *region});
  } else if (*type == "monsterType") {
    auto name = json.getString("typeName");
    if (!name) return std::unexpected(std::move(name.error()));
    auto parameters = json.getObject("parameters", {});
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    return QuestParamDetail(QuestMonsterType{*name, *parameters});
  } else if (*type == "npcType") {
    auto species = json.getString("species");
    if (!species) return std::unexpected(std::move(species.error()));
    auto name = json.getString("typeName");
    if (!name) return std::unexpected(std::move(name.error()));
    auto parameters = json.getObject("parameters", {});
    if (!parameters) return std::unexpected(std::move(parameters.error()));
    auto seed = json.optUInt("seed");
    if (!seed) return std::unexpected(std::move(seed.error()));
    return QuestParamDetail(QuestNpcType{*species, *name, *parameters, *seed});
  } else if (*type == "coordinate") {
    auto spec = json.get("coordinate");
    if (!spec) return std::unexpected(std::move(spec.error()));
    auto coordinate = CelestialCoordinate::create(*spec);
    if (!coordinate) return std::unexpected(std::move(coordinate.error()));
    return QuestParamDetail(QuestCoordinate{*coordinate});
  } else if (*type == "json") return QuestParamDetail(json);
  else if (*type == "noDetail") return QuestParamDetail();
  return std::unexpected(StarException::format("Invalid QuestParam type {}", *type));
}
std::expected<QuestParam, StarException> QuestParam::fromJson(Json const& json) {
  auto detail = questParamDetailFromJson(json);
  if (!detail) return std::unexpected(std::move(detail.error()));
  auto name = json.optString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto portrait = json.opt("portrait");
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  auto indicator = json.optString("indicator");
  if (!indicator) return std::unexpected(std::move(indicator.error()));
  return QuestParam{std::move(*detail), *name, *portrait, *indicator};
}
[[nodiscard]] std::expected<QuestParamDetail, StarException> questParamDetailDiskLoad(Root& root, Json const& json) {
  auto type = json.getString("type");
  if (!type) return std::unexpected(std::move(type.error()));
  if (*type != "item" && *type != "itemList") return questParamDetailFromJson(json);
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  if (*type == "item") {
    auto spec = json.get("item");
    if (!spec) return std::unexpected(std::move(spec.error()));
    auto item = ItemDescriptor::loadStore(**database, *spec);
    if (!item) return std::unexpected(std::move(item.error()));
    return QuestParamDetail(QuestItem{item->name(), item->parameters()});
  }
  auto specs = json.getArray("items");
  if (!specs) return std::unexpected(std::move(specs.error()));
  QuestItemList items;
  for (auto const& spec : *specs) {
    auto item = ItemDescriptor::loadStore(**database, spec);
    if (!item) return std::unexpected(std::move(item.error()));
    items.push_back(std::move(*item));
  }
  return QuestParamDetail(std::move(items));
}
std::expected<QuestParam, StarException> QuestParam::diskLoad(Root& root, Json const& json) {
  auto detail = questParamDetailDiskLoad(root, json);
  if (!detail) return std::unexpected(std::move(detail.error()));
  auto name = json.optString("name");
  if (!name) return std::unexpected(std::move(name.error()));
  auto portrait = json.opt("portrait");
  if (!portrait) return std::unexpected(std::move(portrait.error()));
  auto indicator = json.optString("indicator");
  if (!indicator) return std::unexpected(std::move(indicator.error()));
  return QuestParam{std::move(*detail), *name, *portrait, *indicator};
}
[[nodiscard]] std::expected<Json, StarException> questParamDetailToJson(QuestParamDetail const& detail) {
  if (std::holds_alternative<QuestItem>(detail)) {
    auto descriptor = std::get<QuestItem>(detail).descriptor();
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    return JsonObject{{"type", "item"}, {"item", descriptor->toJson()}};
  } else if (std::holds_alternative<QuestItemTag>(detail)) return JsonObject{{"type", "itemTag"}, {"tag", std::get<QuestItemTag>(detail)}};
  else if (std::holds_alternative<QuestItemList>(detail)) {
    JsonArray array;
    for (auto const& item : std::get<QuestItemList>(detail))
      array.push_back(item.toJson());
    return JsonObject{{"type", "itemList"}, {"items", std::move(array)}};
  } else if (std::holds_alternative<QuestEntity>(detail)) {
    auto const& entity = std::get<QuestEntity>(detail);
    Json gender;
    if (entity.gender) {
      auto name = enumName<GenderNames>(*entity.gender);
      if (!name) return std::unexpected(std::move(name.error()));
      gender = String(*name);
    }
    return JsonObject{{"type", "entity"}, {"uniqueId", jsonFromOptional(entity.uniqueId)}, {"species", jsonFromOptional(entity.species)}, {"gender", gender}};
  } else if (std::holds_alternative<QuestLocation>(detail)) {
    auto const& location = std::get<QuestLocation>(detail);
    return JsonObject{{"type", "location"}, {"uniqueId", jsonFromOptional(location.uniqueId)}, {"region", jsonFromRectF(location.region)}};
  } else if (std::holds_alternative<QuestMonsterType>(detail)) {
    auto const& monster = std::get<QuestMonsterType>(detail);
    return JsonObject{{"type", "monsterType"}, {"typeName", monster.typeName}, {"parameters", monster.parameters}};
  } else if (std::holds_alternative<QuestNpcType>(detail)) {
    auto const& npc = std::get<QuestNpcType>(detail);
    return JsonObject{{"type", "npcType"}, {"species", npc.species}, {"typeName", npc.typeName}, {"parameters", npc.parameters}, {"seed", jsonFromOptional(npc.seed)}};
  } else if (std::holds_alternative<QuestCoordinate>(detail)) return JsonObject{{"type", "coordinate"}, {"coordinate", std::get<QuestCoordinate>(detail).coordinate.toJson()}};
  else if (std::holds_alternative<QuestJson>(detail)) return std::get<QuestJson>(detail).set("type", "json");
  return JsonObject{{"type", "noDetail"}};
}
[[nodiscard]] std::expected<Json, StarException> questParamDetailDiskStore(Root& root, QuestParamDetail const& detail) {
  if (!std::holds_alternative<QuestItem>(detail) && !std::holds_alternative<QuestItemList>(detail)) return questParamDetailToJson(detail);
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  if (std::holds_alternative<QuestItem>(detail)) {
    auto descriptor = std::get<QuestItem>(detail).descriptor();
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto store = descriptor->diskStore(**database);
    if (!store) return std::unexpected(std::move(store.error()));
    return JsonObject{{"type", "item"}, {"item", *store}};
  }
  JsonArray array;
  for (auto const& item : std::get<QuestItemList>(detail)) {
    auto store = item.diskStore(**database);
    if (!store) return std::unexpected(std::move(store.error()));
    array.push_back(std::move(*store));
  }
  return JsonObject{{"type", "itemList"}, {"items", std::move(array)}};
}
std::expected<Json, StarException> QuestParam::toJson() const {
  auto json = questParamDetailToJson(detail);
  if (!json) return std::unexpected(std::move(json.error()));
  auto object = json->toObject();
  if (!object) return std::unexpected(std::move(object.error()));
  (*object)["name"] = jsonFromOptional(name);
  (*object)["portrait"] = jsonFromOptional(portrait);
  (*object)["indicator"] = jsonFromOptional(indicator);
  return Json(std::move(*object));
}
std::expected<Json, StarException> QuestParam::diskStore(Root& root) const {
  auto json = questParamDetailDiskStore(root, detail);
  if (!json) return std::unexpected(std::move(json.error()));
  auto object = json->toObject();
  if (!object) return std::unexpected(std::move(object.error()));
  (*object)["name"] = jsonFromOptional(name);
  (*object)["portrait"] = jsonFromOptional(portrait);
  (*object)["indicator"] = jsonFromOptional(indicator);
  return Json(std::move(*object));
}
bool QuestParam::operator==(QuestParam const& rhs) const { return detail == rhs.detail && name == rhs.name && portrait == rhs.portrait && indicator == rhs.indicator; }
std::expected<QuestDescriptor, StarException> QuestDescriptor::fromJson(Root& root, Json const& json) {
  if (json.isType(Json::Type::String)) {
    auto name = json.toString();
    if (!name) return std::unexpected(std::move(name.error()));
    return QuestDescriptor{*name, *name, {}, root.random().randu64()};
  }
  auto id = json.getString("questId");
  if (!id) return std::unexpected(std::move(id.error()));
  auto templateId = json.getString("templateId");
  if (!templateId) return std::unexpected(std::move(templateId.error()));
  auto params = json.get("parameters");
  if (!params) return std::unexpected(std::move(params.error()));
  auto parameters = questParamsFromJson(*params);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  auto seed = json.getUInt("seed", root.random().randu64());
  if (!seed) return std::unexpected(std::move(seed.error()));
  return QuestDescriptor{*id, *templateId, std::move(*parameters), *seed};
}
std::expected<QuestDescriptor, StarException> QuestDescriptor::diskLoad(Root& root, Json const& spec) {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto versioned = VersionedJson::fromJson(spec);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  auto json = (*database)->loadVersionedJson(*versioned, "QuestDescriptor");
  if (!json) return std::unexpected(std::move(json.error()));
  auto id = json->getString("questId");
  if (!id) return std::unexpected(std::move(id.error()));
  auto templateId = json->getString("templateId");
  if (!templateId) return std::unexpected(std::move(templateId.error()));
  auto params = json->get("parameters");
  if (!params) return std::unexpected(std::move(params.error()));
  auto parameters = questParamsDiskLoad(root, *params);
  if (!parameters) return std::unexpected(std::move(parameters.error()));
  auto seed = json->getUInt("seed", root.random().randu64());
  if (!seed) return std::unexpected(std::move(seed.error()));
  return QuestDescriptor{*id, *templateId, std::move(*parameters), *seed};
}
std::expected<Json, StarException> QuestDescriptor::toJson() const {
  auto params = questParamsToJson(parameters);
  if (!params) return std::unexpected(std::move(params.error()));
  return JsonObject{{"questId", questId}, {"templateId", templateId}, {"parameters", *params}, {"seed", seed}};
}
std::expected<Json, StarException> QuestDescriptor::diskStore(Root& root) const {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto params = questParamsDiskStore(root, parameters);
  if (!params) return std::unexpected(std::move(params.error()));
  auto result = (*database)->makeCurrentVersionedJson("QuestDescriptor", JsonObject{{"questId", questId}, {"templateId", templateId}, {"parameters", *params}, {"seed", seed}});
  if (!result) return std::unexpected(std::move(result.error()));
  return result->toJson();
}
bool QuestDescriptor::operator==(QuestDescriptor const& rhs) const { return questId == rhs.questId && templateId == rhs.templateId && parameters == rhs.parameters && seed == rhs.seed; }
std::expected<QuestArcDescriptor, StarException> QuestArcDescriptor::fromJson(Root& root, Json const& json) {
  if (json.isType(Json::Type::Object)) {
    auto contains = json.contains("quests");
    if (!contains) return std::unexpected(std::move(contains.error()));
    if (*contains) {
      auto array = json.getArray("quests");
      if (!array) return std::unexpected(std::move(array.error()));
      QuestArcDescriptor arc;
      for (auto const& entry : *array) {
        auto quest = QuestDescriptor::fromJson(root, entry);
        if (!quest) return std::unexpected(std::move(quest.error()));
        arc.quests.push_back(std::move(*quest));
      }
      auto id = json.optString("stagehandUniqueId");
      if (!id) return std::unexpected(std::move(id.error()));
      arc.stagehandUniqueId = std::move(*id);
      return arc;
    }
  }
  auto quest = QuestDescriptor::fromJson(root, json);
  if (!quest) return std::unexpected(std::move(quest.error()));
  return QuestArcDescriptor{{std::move(*quest)}, {}};
}
std::expected<QuestArcDescriptor, StarException> QuestArcDescriptor::diskLoad(Root& root, Json const& spec) {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  auto versioned = VersionedJson::fromJson(spec);
  if (!versioned) return std::unexpected(std::move(versioned.error()));
  auto json = (*database)->loadVersionedJson(*versioned, "QuestArcDescriptor");
  if (!json) return std::unexpected(std::move(json.error()));
  auto array = json->getArray("quests");
  if (!array) return std::unexpected(std::move(array.error()));
  QuestArcDescriptor arc;
  for (auto const& entry : *array) {
    auto quest = QuestDescriptor::diskLoad(root, entry);
    if (!quest) return std::unexpected(std::move(quest.error()));
    arc.quests.push_back(std::move(*quest));
  }
  auto id = json->optString("stagehandUniqueId");
  if (!id) return std::unexpected(std::move(id.error()));
  arc.stagehandUniqueId = std::move(*id);
  return arc;
}
std::expected<Json, StarException> QuestArcDescriptor::toJson() const {
  JsonArray array;
  for (auto const& quest : quests) {
    auto json = quest.toJson();
    if (!json) return std::unexpected(std::move(json.error()));
    array.push_back(std::move(*json));
  }
  return JsonObject{{"quests", std::move(array)}, {"stagehandUniqueId", jsonFromOptional(stagehandUniqueId)}};
}
std::expected<Json, StarException> QuestArcDescriptor::diskStore(Root& root) const {
  auto database = root.versioningDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  JsonArray array;
  for (auto const& quest : quests) {
    auto json = quest.diskStore(root);
    if (!json) return std::unexpected(std::move(json.error()));
    array.push_back(std::move(*json));
  }
  auto result = (*database)->makeCurrentVersionedJson("QuestArcDescriptor", JsonObject{{"quests", std::move(array)}, {"stagehandUniqueId", jsonFromOptional(stagehandUniqueId)}});
  if (!result) return std::unexpected(std::move(result.error()));
  return result->toJson();
}
bool QuestArcDescriptor::operator==(QuestArcDescriptor const& rhs) const { return quests == rhs.quests && stagehandUniqueId == rhs.stagehandUniqueId; }
std::expected<String, StarException> questParamText(Root& root, QuestParam const& parameter) {
  if (parameter.name) return *parameter.name;
  auto database = root.itemDatabase();
  if (!database) return std::unexpected(std::move(database.error()));
  if (std::holds_alternative<QuestItem>(parameter.detail)) {
    auto descriptor = std::get<QuestItem>(parameter.detail).descriptor();
    if (!descriptor) return std::unexpected(std::move(descriptor.error()));
    auto value = (*database)->itemShared(*descriptor);
    if (!value) return std::unexpected(std::move(value.error()));
    return (*value)->friendlyName();
  } else if (std::holds_alternative<QuestItemList>(parameter.detail)) {
    StringList strings;
    for (auto const& descriptor : std::get<QuestItemList>(parameter.detail)) {
      auto item = (*database)->itemShared(descriptor);
      if (!item) return std::unexpected(std::move(item.error()));
      strings.push_back(Star::format("{} {}", descriptor.count(), (*item)->friendlyName()));
    }
    return strings.join(", ");
  }
  return String();
}
std::expected<StringMap<String>, StarException> questParamTags(Root& root, StringMap<QuestParam> const& parameters) {
  StringMap<String> result;
  for (auto const& p : parameters) {
    auto text = questParamText(root, p.second);
    if (!text) return std::unexpected(std::move(text.error()));
    result.set(p.first, std::move(*text));
  }
  return result;
}
std::expected<StringMap<QuestParam>, StarException> questParamsFromJson(Json const& json) { return jsonToMapV<StringMap<QuestParam>>(json, [](Json const& param) { return QuestParam::fromJson(param); }); }
std::expected<StringMap<QuestParam>, StarException> questParamsDiskLoad(Root& root, Json const& json) { return jsonToMapV<StringMap<QuestParam>>(json, [&root](Json const& param) { return QuestParam::diskLoad(root, param); }); }
std::expected<Json, StarException> questParamsToJson(StringMap<QuestParam> const& parameters) { return jsonFromMapV(parameters, [](QuestParam const& param) { return param.toJson(); }); }
std::expected<Json, StarException> questParamsDiskStore(Root& root, StringMap<QuestParam> const& parameters) { return jsonFromMapV(parameters, [&root](QuestParam const& param) { return param.diskStore(root); }); }

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestItem& item) {
  auto result = ds >> item.itemName >> item.parameters;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestItem const& item) {
  auto result = ds << item.itemName << item.parameters;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestEntity& entity) {
  auto result = ds >> entity.uniqueId >> entity.species >> entity.gender;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestEntity const& entity) {
  auto result = ds << entity.uniqueId << entity.species << entity.gender;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestLocation& location) {
  auto result = ds >> location.uniqueId >> location.region;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestLocation const& location) {
  auto result = ds << location.uniqueId << location.region;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestMonsterType& monster) {
  auto result = ds >> monster.typeName >> monster.parameters;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestMonsterType const& monster) {
  auto result = ds << monster.typeName << monster.parameters;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestNpcType& npc) {
  auto result = ds >> npc.species >> npc.typeName >> npc.parameters >> npc.seed;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestNpcType const& npc) {
  auto result = ds << npc.species << npc.typeName << npc.parameters << npc.seed;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestCoordinate& coordinate) {
  auto result = ds >> coordinate.coordinate;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestCoordinate const& coordinate) {
  auto result = ds << coordinate.coordinate;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestParam& param) {
  auto result = ds >> param.detail >> param.name >> param.portrait >> param.indicator;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestParam const& param) {
  auto result = ds << param.detail << param.name << param.portrait << param.indicator;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestDescriptor& quest) {
  auto result = ds >> quest.questId >> quest.templateId >> quest.parameters >> quest.seed;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestDescriptor const& quest) {
  auto result = ds << quest.questId << quest.templateId << quest.parameters << quest.seed;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, QuestArcDescriptor& arc) {
  auto result = ds >> arc.quests >> arc.stagehandUniqueId;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, QuestArcDescriptor const& arc) {
  auto result = ds << arc.quests << arc.stagehandUniqueId;
  if (!result) return std::unexpected(std::move(result.error()));
  return std::ref(ds);
}
}
