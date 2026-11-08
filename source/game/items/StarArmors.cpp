import star.json;
import star.json_extra;
import star.id_map;
import star.game_types;
import star.strong_typedef;
import star.data_stream;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.bi_map;
import star.rpc_promise;
import star.a_star;
import star.set;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.random;
import star.periodic_function;
import star.audio;
import star.map;
import star.logging;
import star.listener;
import std;
import star.format;
import star.version;
import star.parametric_function;
import star.multi_array_interpolator;
import star.multi_table;
import star.ordered_set;
import star.either;

import star.armors;
import star.lua_root;
import star.drawable;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.status_types;
import star.status_effect_item;
import star.effect_source_item;
import star.previewable_item;
import star.damage_types;
import star.world_geometry;
import star.damage;
import star.light_source;
import star.entity;
import star.animation;
import star.particle;
import star.interaction_types;
import star.tile_damage;
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
import star.mobile_entity;
import star.actor_entity;
import star.tool_user_entity;
import star.tool_user_item;
import star.lua_components;
import star.fireable_item;
import star.swingable_item;
import star.asset_source;
import star.assets;
import star.animated_part_set;
import star.mixer;
import star.networked_animator;
import star.humanoid;
import star.root_base;
import star.configuration;
import star.root;
import star.uuid;
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
import star.entity_rendering_types;
import star.entity_rendering;
import star.player_types;
import star.lua_actor_movement_component;
import star.lua_animation_component;
import star.radio_message_database;
import star.player;
import star.stored_functions;

import star.math_common;
namespace Star {

extern std::expected<EnumMap<ArmorType>, StarException> const ArmorTypeNames = EnumMap<ArmorType>::create({
  {ArmorType::Head, "Head"},
  {ArmorType::Chest, "Chest"},
  {ArmorType::Legs, "Legs"},
  {ArmorType::Back, "Back"}
});

std::expected<void, StarException> ArmorItem::initializeArmor(Json const& config, String const& directory, Json const& data) {
  auto initialized = initialize(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto swingable = initializeSwingable(config);
  if (!swingable)
    return std::unexpected(std::move(swingable.error()));
  auto effects = refreshStatusEffects();
  if (!effects)
    return std::unexpected(std::move(effects.error()));
  auto sources = instanceValue("effectSources", JsonArray()).and_then(jsonToStringSet);
  if (!sources)
    return std::unexpected(std::move(sources.error()));
  m_effectSources = std::move(*sources);
  auto module = instanceValue("techModule", "").and_then([](Json const& value) { return value.toString(); });
  if (!module)
    return std::unexpected(std::move(module.error()));
  m_techModule = std::move(*module);
  if (m_techModule->empty())
    m_techModule = {};
  else
    m_techModule = AssetPath::relative_to(directory, *m_techModule);
  auto directives = instanceValue("directives", "").and_then([](Json const& value) { return value.toString(); });
  if (!directives)
    return std::unexpected(std::move(directives.error()));
  m_directives = *directives;
  auto fullbright = instanceValue("fullbright", false).and_then([](Json const& value) { return value.toBool(); });
  if (!fullbright)
    return std::unexpected(std::move(fullbright.error()));
  m_fullbright = *fullbright;
  auto options = config.getArray("colorOptions", JsonArray{""}).and_then(colorDirectivesFromConfig);
  if (!options)
    return std::unexpected(std::move(options.error()));
  m_colorOptions = std::move(*options);
  if (!m_directives) {
    auto index = instanceValue("colorIndex", 0).and_then([](Json const& value) { return value.toUInt(); });
    if (!index)
      return std::unexpected(std::move(index.error()));
    auto option = m_colorOptions.wrap(*index);
    if (!option)
      return std::unexpected(std::move(option.error()));
    m_directives = "?" + option->get();
  }
  auto icons = refreshIconDrawables();
  if (!icons)
    return std::unexpected(std::move(icons.error()));
  auto flipValue = instanceValueOfType("flipDirectives", Json::Type::String);
  if (!flipValue)
    return std::unexpected(std::move(flipValue.error()));
  if (*flipValue) {
    auto flipDirectives = flipValue->toString();
    if (!flipDirectives)
      return std::unexpected(std::move(flipDirectives.error()));
    auto directivesStr = m_directives.maybeString();
    auto append = flipDirectives->starts_with('+');
    if (!append)
      return std::unexpected(std::move(append.error()));
    if (*append && directivesStr) {
      auto suffix = flipDirectives->substr(1);
      if (!suffix)
        return std::unexpected(std::move(suffix.error()));
      m_flipDirectives = Directives(directivesStr->get() + *suffix);
    } else {
      m_flipDirectives = Directives(std::move(*flipDirectives));
    }
  }
  auto bypass = instanceValueOfType("bypassNude", Json::Type::Bool, false).and_then([](Json const& value) { return value.toBool(); });
  if (!bypass)
    return std::unexpected(std::move(bypass.error()));
  m_bypassNude = *bypass;
  auto hideSlots = instanceValueOfType("hideInVanillaSlots", Json::Type::Bool, false).and_then([](Json const& value) { return value.toBool(); });
  if (!hideSlots)
    return std::unexpected(std::move(hideSlots.error()));
  m_hideInVanillaSlots = *hideSlots;
  auto hiddenTypes = instanceValueOfType("armorTypesToHide", Json::Type::Array);
  if (!hiddenTypes)
    return std::unexpected(std::move(hiddenTypes.error()));
  if (*hiddenTypes) {
    auto types = hiddenTypes->toArray();
    if (!types)
      return std::unexpected(std::move(types.error()));
    m_armorTypesToHide.emplace();
    for (auto const& value : *types) {
      if (!value.isType(Json::Type::String))
        continue;
      auto name = value.toString();
      if (!name)
        return std::unexpected(std::move(name.error()));
      if (!ArmorTypeNames)
        return std::unexpected(ArmorTypeNames.error());
      auto type = ArmorTypeNames->find_left_ref(*name);
      if (!type)
        return std::unexpected(std::move(type.error()));
      if (*type)
        m_armorTypesToHide->add((*type)->get());
    }
  }
  auto hideBody = config.getBool("hideBody", false);
  if (!hideBody)
    return std::unexpected(std::move(hideBody.error()));
  m_hideBody = *hideBody;
  auto cosmeticEffects = config.getBool("statusEffectsInCosmeticSlots", false);
  if (!cosmeticEffects)
    return std::unexpected(std::move(cosmeticEffects.error()));
  m_statusEffectsInCosmeticSlot = *cosmeticEffects;
  return {};
}

std::expected<List<PersistentStatusEffect>, StarException> ArmorItem::statusEffects() const {
  return m_statusEffects;
}

bool ArmorItem::statusEffectsInCosmeticSlot() const {
  return m_statusEffectsInCosmeticSlot;
}

List<PersistentStatusEffect> ArmorItem::cosmeticStatusEffects() const {
  return m_cosmeticStatusEffects;
}

std::expected<StringSet, StarException> ArmorItem::effectSources() const {
  return m_effectSources;
}

std::expected<List<Drawable>, StarException> ArmorItem::drawables() const {
  auto drawables = iconDrawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  Drawable::scaleAll(*drawables, 1.0f / TilePixels);
  Drawable::translateAll(*drawables, -handPosition() / TilePixels);
  return drawables;
}

std::expected<float, StarException> ArmorItem::getAngle(float) {
  return -25.0f * Constants::deg2rad;
}

std::expected<void, StarException> ArmorItem::fire(FireMode, bool, bool) { return {}; }
std::expected<void, StarException> ArmorItem::fireTriggered() { return {}; }

List<String> const& ArmorItem::colorOptions() {
  return m_colorOptions;
}

Directives const& ArmorItem::directives(bool flip) const {
  return (flip && m_flipDirectives) ? *m_flipDirectives : m_directives;
}

bool ArmorItem::fullbright() const {
  return m_fullbright;
}

bool ArmorItem::flipping() const {
  return m_flipDirectives.has_value();
}

bool ArmorItem::visible(bool extraCosmetic) const {
  return extraCosmetic || !m_hideInVanillaSlots;
}

HashSet<ArmorType> const& ArmorItem::armorTypesToHide() {
  if (!m_armorTypesToHide) {
    m_armorTypesToHide.emplace();
    m_armorTypesToHide->add(armorType());
  }
  return *m_armorTypesToHide;
}

bool ArmorItem::hideBody() const {
  return m_hideBody;
}

bool ArmorItem::bypassNude() const {
  return m_bypassNude;
}


std::optional<String> const& ArmorItem::techModule() const {
  return m_techModule;
}


std::expected<void, StarException> ArmorItem::refreshIconDrawables() {
  auto drawables = iconDrawables();
  if (!drawables)
    return std::unexpected(std::move(drawables.error()));
  for (auto& drawable : *drawables) {
    if (drawable.isImage()) {
      auto metadata = root().imageMetadataDatabase();
      if (!metadata)
        return std::unexpected(std::move(metadata.error()));
      auto removed = drawable.imagePart().removeDirectives(**metadata, true);
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      auto added = drawable.imagePart().addDirectives(**metadata, m_directives, true);
      if (!added)
        return std::unexpected(std::move(added.error()));
    }
  }
  return setIconDrawables(std::move(*drawables));
}

std::expected<void, StarException> ArmorItem::refreshStatusEffects() {
  for (auto const& [key, target] : {std::pair{std::string_view("statusEffects"), std::ref(m_statusEffects)}, std::pair{std::string_view("cosmeticStatusEffects"), std::ref(m_cosmeticStatusEffects)}}) {
    auto values = instanceValue(key, JsonArray()).and_then([](Json const& value) { return value.toArray(); });
    if (!values)
      return std::unexpected(std::move(values.error()));
    target.get().clear();
    for (auto const& value : *values) {
      auto effect = jsonToPersistentStatusEffect(value);
      if (!effect)
        return std::unexpected(std::move(effect.error()));
      target.get().push_back(std::move(*effect));
    }
  }
  auto leveled = instanceValue("leveledStatusEffects", Json());
  if (!leveled)
    return std::unexpected(std::move(leveled.error()));
  if (*leveled) {
    auto database = root().functionDatabase();
    if (!database)
      return std::unexpected(std::move(database.error()));
    auto level = instanceValue("level", 1).and_then([](Json const& value) { return value.toFloat(); });
    if (!level)
      return std::unexpected(std::move(level.error()));
    auto values = leveled->iterateArray();
    if (!values)
      return std::unexpected(std::move(values.error()));
    for (auto const& effectConfig : *values) {
      auto name = effectConfig.getString("levelFunction");
      if (!name)
        return std::unexpected(std::move(name.error()));
      auto function = (*database)->function(*name);
      if (!function)
        return std::unexpected(std::move(function.error()));
      auto factor = (*function)->evaluate(*level);
      if (!factor)
        return std::unexpected(std::move(factor.error()));
      auto statModifier = jsonToStatModifier(effectConfig);
      if (!statModifier)
        return std::unexpected(std::move(statModifier.error()));
      if (std::holds_alternative<StatBaseMultiplier>(*statModifier)) {
        auto& modifier = std::get<StatBaseMultiplier>(*statModifier);
        modifier.baseMultiplier = 1 + (modifier.baseMultiplier - 1) * *factor;
      } else if (std::holds_alternative<StatValueModifier>(*statModifier)) {
        std::get<StatValueModifier>(*statModifier).value *= *factor;
      } else if (std::holds_alternative<StatEffectiveMultiplier>(*statModifier)) {
        auto& modifier = std::get<StatEffectiveMultiplier>(*statModifier);
        modifier.effectiveMultiplier = 1 + (modifier.effectiveMultiplier - 1) * *factor;
      }
      m_statusEffects.push_back(std::move(*statModifier));
    }
  }
  auto augment = instanceValue("currentAugment", Json());
  if (!augment)
    return std::unexpected(std::move(augment.error()));
  if (*augment) {
    auto values = augment->getArray("effects", JsonArray());
    if (!values)
      return std::unexpected(std::move(values.error()));
    for (auto const& value : *values) {
      auto effect = jsonToPersistentStatusEffect(value);
      if (!effect)
        return std::unexpected(std::move(effect.error()));
      m_statusEffects.push_back(std::move(*effect));
    }
  }
  return {};
}

std::expected<std::shared_ptr<HeadArmor>, StarException> HeadArmor::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<HeadArmor>(new HeadArmor(root));
  auto initialized = item->initializeArmor(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto male = config.getString("maleFrames");
  if (!male)
    return std::unexpected(std::move(male.error()));
  item->m_maleImage = AssetPath::relative_to(directory, *male);
  auto female = config.getString("femaleFrames");
  if (!female)
    return std::unexpected(std::move(female.error()));
  item->m_femaleImage = AssetPath::relative_to(directory, *female);
  auto mask = item->instanceValue("mask").and_then([](Json const& value) { return value.toString(); });
  if (!mask)
    return std::unexpected(std::move(mask.error()));
  if (!mask->empty()) {
    auto contains = mask->contains("?");
    if (!contains)
      return std::unexpected(std::move(contains.error()));
    if (!*contains)
      item->m_maskDirectives = "?addmask=" + AssetPath::relative_to(directory, *mask) + ";0;0";
    else
      item->m_maskDirectives = *mask;
  } else {
    item->m_maskDirectives = *mask;
  }
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> HeadArmor::clone() const {
  return std::make_shared<HeadArmor>(*this);
}

ArmorType HeadArmor::armorType() const {
  return ArmorType::Head;
}

String const& HeadArmor::frameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleImage;
  else
    return m_femaleImage;
}

Directives const& HeadArmor::maskDirectives() const {
  return m_maskDirectives;
}

std::expected<List<Drawable>, StarException> HeadArmor::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer) const {
  Gender gender = viewer ? viewer->gender() : Gender::Male;
  auto humanoid = viewer ? std::expected<std::shared_ptr<Humanoid>, StarException>(viewer->humanoid()) : Humanoid::makeDummy(root(), gender);
  if (!humanoid)
    return std::unexpected(std::move(humanoid.error()));
  return (*humanoid)->renderDummy(gender, std::cref(*this), std::nullopt, std::nullopt, std::nullopt);
}

std::expected<std::shared_ptr<ChestArmor>, StarException> ChestArmor::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<ChestArmor>(new ChestArmor(root));
  auto initialized = item->initializeArmor(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto male = config.get("maleFrames");
  if (!male)
    return std::unexpected(std::move(male.error()));
  for (auto const& [key, target] : {
      std::pair{std::string_view("body"), std::ref(item->m_maleBodyImage)},
      std::pair{std::string_view("frontSleeve"), std::ref(item->m_maleFrontSleeveImage)},
      std::pair{std::string_view("backSleeve"), std::ref(item->m_maleBackSleeveImage)}}) {
    auto path = male->getString(key);
    if (!path)
      return std::unexpected(std::move(path.error()));
    target.get() = AssetPath::relative_to(directory, *path);
  }
  auto female = config.get("femaleFrames");
  if (!female)
    return std::unexpected(std::move(female.error()));
  for (auto const& [key, target] : {
      std::pair{std::string_view("body"), std::ref(item->m_femaleBodyImage)},
      std::pair{std::string_view("frontSleeve"), std::ref(item->m_femaleFrontSleeveImage)},
      std::pair{std::string_view("backSleeve"), std::ref(item->m_femaleBackSleeveImage)}}) {
    auto path = female->getString(key);
    if (!path)
      return std::unexpected(std::move(path.error()));
    target.get() = AssetPath::relative_to(directory, *path);
  }
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> ChestArmor::clone() const {
  return std::make_shared<ChestArmor>(*this);
}

ArmorType ChestArmor::armorType() const {
  return ArmorType::Chest;
}

String const& ChestArmor::bodyFrameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleBodyImage;
  else
    return m_femaleBodyImage;
}

String const& ChestArmor::frontSleeveFrameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleFrontSleeveImage;
  else
    return m_femaleFrontSleeveImage;
}

String const& ChestArmor::backSleeveFrameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleBackSleeveImage;
  else
    return m_femaleBackSleeveImage;
}

std::expected<List<Drawable>, StarException> ChestArmor::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer) const {
  Gender gender = viewer ? viewer->gender() : Gender::Male;
  auto humanoid = viewer ? std::expected<std::shared_ptr<Humanoid>, StarException>(viewer->humanoid()) : Humanoid::makeDummy(root(), gender);
  if (!humanoid)
    return std::unexpected(std::move(humanoid.error()));
  return (*humanoid)->renderDummy(gender, std::nullopt, std::cref(*this), std::nullopt, std::nullopt);
}

std::expected<std::shared_ptr<LegsArmor>, StarException> LegsArmor::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<LegsArmor>(new LegsArmor(root));
  auto initialized = item->initializeArmor(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto male = config.getString("maleFrames");
  if (!male)
    return std::unexpected(std::move(male.error()));
  item->m_maleImage = AssetPath::relative_to(directory, *male);
  auto female = config.getString("femaleFrames");
  if (!female)
    return std::unexpected(std::move(female.error()));
  item->m_femaleImage = AssetPath::relative_to(directory, *female);
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> LegsArmor::clone() const {
  return std::make_shared<LegsArmor>(*this);
}

ArmorType LegsArmor::armorType() const {
  return ArmorType::Legs;
}

String const& LegsArmor::frameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleImage;
  else
    return m_femaleImage;
}

std::expected<List<Drawable>, StarException> LegsArmor::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer) const {
  Gender gender = viewer ? viewer->gender() : Gender::Male;
  auto humanoid = viewer ? std::expected<std::shared_ptr<Humanoid>, StarException>(viewer->humanoid()) : Humanoid::makeDummy(root(), gender);
  if (!humanoid)
    return std::unexpected(std::move(humanoid.error()));
  return (*humanoid)->renderDummy(gender, std::nullopt, std::nullopt, std::cref(*this), std::nullopt);
}

std::expected<std::shared_ptr<BackArmor>, StarException> BackArmor::create(Root& root, Json const& config, String const& directory, Json const& data) {
  auto item = std::shared_ptr<BackArmor>(new BackArmor(root));
  auto initialized = item->initializeArmor(config, directory, data);
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  auto male = config.getString("maleFrames");
  if (!male)
    return std::unexpected(std::move(male.error()));
  item->m_maleImage = AssetPath::relative_to(directory, *male);
  auto female = config.getString("femaleFrames");
  if (!female)
    return std::unexpected(std::move(female.error()));
  item->m_femaleImage = AssetPath::relative_to(directory, *female);
  return item;
}

std::expected<std::shared_ptr<Item>, StarException> BackArmor::clone() const {
  return std::make_shared<BackArmor>(*this);
}

ArmorType BackArmor::armorType() const {
  return ArmorType::Back;
}

String const& BackArmor::frameset(Gender gender) const {
  if (gender == Gender::Male)
    return m_maleImage;
  else
    return m_femaleImage;
}

std::expected<List<Drawable>, StarException> BackArmor::preview(std::optional<std::reference_wrapper<TileDrawer>>, std::shared_ptr<Player> const& viewer) const {
  Gender gender = viewer ? viewer->gender() : Gender::Male;
  auto humanoid = viewer ? std::expected<std::shared_ptr<Humanoid>, StarException>(viewer->humanoid()) : Humanoid::makeDummy(root(), gender);
  if (!humanoid)
    return std::unexpected(std::move(humanoid.error()));
  return (*humanoid)->renderDummy(gender, std::nullopt, std::nullopt, std::nullopt, std::cref(*this));
}

}
