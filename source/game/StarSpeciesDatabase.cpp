import star.json;
import star.json_extra;
import star.set;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.strong_typedef;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.id_map;
import star.lru_cache;
import star.time;
import star.ttl_cache;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.lua;
import star.format;
import star.version;

import star.drawable;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.item;
import star.item_descriptor;
import star.item_recipe;
import star.inventory_types;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.status_types;
import star.lua_root;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.utility_lua_bindings;
import star.name_generator;

import star.species_database;
import star.config_lua_bindings;
import star.root_lua_bindings;
import star.item_database;


namespace Star {

SpeciesOption::SpeciesOption()
  : species(),
    headOptionAsHairColor(),
    headOptionAsFacialhair(),
    altOptionAsUndyColor(),
    altOptionAsHairColor(),
    altOptionAsFacialMask(),
    hairColorAsBodySubColor(),
    bodyColorAsFacialMaskSubColor(),
    altColorAsFacialMaskSubColor(),
    genderOptions(),
    bodyColorDirectives(),
    undyColorDirectives(),
    hairColorDirectives() {}

std::expected<std::shared_ptr<SpeciesDatabase>, StarException> SpeciesDatabase::create(Root& root) {
  auto result = std::shared_ptr<SpeciesDatabase>(new SpeciesDatabase(root));
  auto luaRoot = LuaRoot::create(root);
  if (!luaRoot) return std::unexpected(std::move(luaRoot.error()));
  result->m_luaRoot = std::move(*luaRoot);
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto files = (*assets)->scanExtension("species");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queueJsons(files->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto loaded = [&]() -> std::expected<void, StarException> {
    auto config = (*assets)->json(file.string());
    if (!config) return std::unexpected(std::move(config.error()));
    auto definition = SpeciesDefinition::create(root, *config);
    if (!definition) return std::unexpected(std::move(definition.error()));
    if (result->m_species.contains((*definition)->kind()))
      return std::unexpected(StarException::format("Duplicate species asset with kind {}. configfile {}", (*definition)->kind(), file.string()));
    auto kind = (*definition)->kind().toLower();
    if (!kind) return std::unexpected(std::move(kind.error()));
    result->m_species[*kind] = std::move(*definition);
    return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->withAssetContext(std::move(loaded.error()), file.string(), "Loading species"));
  }
  return result;
}

std::expected<std::shared_ptr<SpeciesDefinition>, StarException> SpeciesDatabase::species(String const& kind) const {
  auto key = kind.toLower();
  if (!key) return std::unexpected(std::move(key.error()));
  auto found = m_species.find(*key);
  if (found == m_species.end())
    return std::unexpected(StarException::format("Unknown species kind '{}'.", kind));
  return found->second;
}

StringMap<std::shared_ptr<SpeciesDefinition>> SpeciesDatabase::allSpecies() const {
  return m_species;
}

std::expected<Json, StarException> SpeciesDatabase::humanoidConfig(HumanoidIdentity identity, JsonObject parameters, Json config, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  auto definition = species(identity.species);
  if (!definition) return std::unexpected(std::move(definition.error()));
  if ((*definition)->m_buildScripts.empty() && config.isType(Json::Type::Object))
    return config;
  auto baseConfig = (*definition)->humanoidConfig();
  if (!baseConfig) return std::unexpected(std::move(baseConfig.error()));
  if (!(*definition)->m_buildScripts.empty()) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    auto context = m_luaRoot->createContext((*definition)->m_buildScripts);
    if (!context) return std::unexpected(std::move(context.error()));
    auto removed = context->remove("root");
    if (!removed) return std::unexpected(std::move(removed.error()));
    auto rootCallbacks = LuaBindings::makeRootCallbacks(m_root, itemCatalog);
    if (!rootCallbacks) return std::unexpected(std::move(rootCallbacks.error()));
    auto installed = context->setCallbacks("root", *rootCallbacks);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto utility = LuaBindings::makeUtilityCallbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utility) return std::unexpected(std::move(utility.error()));
    installed = context->setCallbacks("sb", *utility);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto identityJson = identity.toJson();
    if (!identityJson) return std::unexpected(std::move(identityJson.error()));
    return context->invokePath<Json>("build", *identityJson, parameters, *baseConfig, config);
  }
  return jsonMerge(*baseConfig, parameters);
}

std::expected<CharacterCreationResult, StarException> SpeciesDatabase::createHumanoid(
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
  LuaVariadic<LuaValue> ext,
  std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog
) const {
  CharacterCreationResult result;

  auto definition = species(speciesChoice);
  if (!definition) return std::unexpected(std::move(definition.error()));
  auto const& speciesDefinition = *definition;
  if (!speciesDefinition->m_creationScripts.empty()) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lockWithDiagnostics(locker);
    auto context = m_luaRoot->createContext(speciesDefinition->m_creationScripts);
    if (!context) return std::unexpected(std::move(context.error()));
    auto removed = context->remove("root");
    if (!removed) return std::unexpected(std::move(removed.error()));
    auto callbacks = LuaBindings::makeRootCallbacks(m_root, itemCatalog);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    auto installed = context->setCallbacks("root", *callbacks);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto utility = LuaBindings::makeUtilityCallbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utility) return std::unexpected(std::move(utility.error()));
    installed = context->setCallbacks("sb", *utility);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto created = context->invokePath<LuaTupleReturn<Json, JsonObject, JsonObject>>(
      "create", name, speciesChoice, genderChoice, bodyColorChoice, alty, hairChoice,
      heady, shirtChoice, shirtColor, pantsChoice, pantsColor, personality, ext);
    if (!created) return std::unexpected(std::move(created.error()));
    Json identityReturn;
    luaTie(identityReturn, result.humanoidParameters, result.armor) = std::move(*created);
    auto identity = HumanoidIdentity::create(identityReturn);
    if (!identity) return std::unexpected(std::move(identity.error()));
    result.identity = std::move(*identity);
  } else {
    auto species = speciesDefinition->options();
    auto genderResult = species.genderOptions.wrap(genderChoice);
    if (!genderResult) return std::unexpected(std::move(genderResult.error()));
    auto const& gender = genderResult->get();
    auto bodyResult = species.bodyColorDirectives.wrap(bodyColorChoice);
    if (!bodyResult) return std::unexpected(std::move(bodyResult.error()));
    auto bodyColor = bodyResult->get();

    String altColor;

    if (species.altOptionAsUndyColor) {
      // undyColor
      auto selected = species.undyColorDirectives.wrap(alty);
      if (!selected) return std::unexpected(std::move(selected.error()));
      altColor = selected->get();
    }

    auto hair = gender.hairOptions.wrap(hairChoice);
    if (!hair) return std::unexpected(std::move(hair.error()));
    String hairColor = bodyColor;
    if (species.headOptionAsHairColor) {
      auto selected = species.hairColorDirectives.wrap(heady);
      if (!selected) return std::unexpected(std::move(selected.error()));
      hairColor = selected->get();
      if (species.altOptionAsHairColor) {
        auto alt = species.undyColorDirectives.wrap(alty);
        if (!alt) return std::unexpected(std::move(alt.error()));
        hairColor += alt->get();
      }
    }

    if (species.hairColorAsBodySubColor)
      bodyColor += hairColor;

    String facialHair;
    String facialHairGroup;
    String facialHairDirective;
    if (species.headOptionAsFacialhair) {
      auto selected = gender.facialHairOptions.wrap(heady);
      if (!selected) return std::unexpected(std::move(selected.error()));
      facialHair = selected->get();
      facialHairGroup = gender.facialHairGroup;
      facialHairDirective = hairColor;
    }

    String facialMask;
    String facialMaskGroup;
    String facialMaskDirective;
    if (species.altOptionAsFacialMask) {
      auto selected = gender.facialMaskOptions.wrap(alty);
      if (!selected) return std::unexpected(std::move(selected.error()));
      facialMask = selected->get();
      facialMaskGroup = gender.facialMaskGroup;
      facialMaskDirective = "";
    }
    if (species.bodyColorAsFacialMaskSubColor)
      facialMaskDirective += bodyColor;
    if (species.altColorAsFacialMaskSubColor)
      facialMaskDirective += altColor;

    auto shirt = gender.shirtOptions.wrap(shirtChoice);
    if (!shirt) return std::unexpected(std::move(shirt.error()));
    auto pants = gender.pantsOptions.wrap(pantsChoice);
    if (!pants) return std::unexpected(std::move(pants.error()));

    auto personalityResult = speciesDefinition->personalities().wrap(personality);
    if (!personalityResult) return std::unexpected(std::move(personalityResult.error()));

    result.identity.name = name;
    result.identity.species = species.species;
    result.identity.bodyDirectives = bodyColor + altColor;
    auto selectedGender = enumValue<GenderNames>(gender.name);
    if (!selectedGender) return std::unexpected(std::move(selectedGender.error()));
    result.identity.gender = *selectedGender;
    result.identity.hairGroup = gender.hairGroup;
    result.identity.hairType = hair->get();
    result.identity.hairDirectives = hairColor;
    result.identity.emoteDirectives = bodyColor + altColor;
    result.identity.facialHairGroup = facialHairGroup;
    result.identity.facialHairType = facialHair;
    result.identity.facialHairDirectives = facialHairDirective;
    result.identity.facialMaskGroup = facialMaskGroup;
    result.identity.facialMaskType = facialMask;
    result.identity.facialMaskDirectives = facialMaskDirective;
    result.identity.personality = personalityResult->get();

    if (!EquipmentSlotNames) return std::unexpected(EquipmentSlotNames.error());
    auto chestSlot = EquipmentSlotNames->getRight(EquipmentSlot::Chest);
    if (!chestSlot) return std::unexpected(std::move(chestSlot.error()));
    auto legSlot = EquipmentSlotNames->getRight(EquipmentSlot::Legs);
    if (!legSlot) return std::unexpected(std::move(legSlot.error()));
    auto chest = ItemDescriptor::create(shirt->get(), 1, JsonObject{{"colorIndex", shirtColor}});
    if (!chest) return std::unexpected(std::move(chest.error()));
    auto legs = ItemDescriptor::create(pants->get(), 1, JsonObject{{"colorIndex", pantsColor}});
    if (!legs) return std::unexpected(std::move(legs.error()));
    result.armor.set(chestSlot->get(), chest->toJson());
    result.armor.set(legSlot->get(), legs->toJson());

    result.humanoidParameters.set("choices", JsonArray({genderChoice, bodyColorChoice, alty, hairChoice, heady, shirtChoice, shirtColor, pantsChoice, pantsColor, personality}));
  }

  return result;
}

std::expected<CharacterCreationResult, StarException> SpeciesDatabase::generateHumanoid(String speciesChoice, std::int64_t seed, std::optional<Gender> gender, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const {
  RandomSource randSource(seed);
  auto definition = species(speciesChoice);
  if (!definition) return std::unexpected(std::move(definition.error()));
  auto chosenGender = gender.value_or(randSource.randb() ? Gender::Male : Gender::Female);
  auto generator = m_root.nameGenerator();
  if (!generator) return std::unexpected(std::move(generator.error()));
  auto name = (*generator)->generateName((*definition)->nameGen(chosenGender), randSource);
  if (!name) return std::unexpected(std::move(name.error()));
  return createHumanoid(std::move(*name), speciesChoice, static_cast<unsigned>(chosenGender), randSource.randu32(),
                        randSource.randu32(), randSource.randu32(), randSource.randu32(), randSource.randu32(),
                        randSource.randu32(), randSource.randu32(), randSource.randu32(), randSource.randu32(), {},
                        itemCatalog);
}

std::expected<std::shared_ptr<SpeciesDefinition>, StarException> SpeciesDefinition::create(Root& root, Json const& config) {
  auto result = std::shared_ptr<SpeciesDefinition>(new SpeciesDefinition(root));
  auto initialized = result->init(config);
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return result;
}

std::expected<void, StarException> SpeciesDefinition::init(Json const& config) {
  m_config = config;
  auto kind = config.getString("kind");
  if (!kind) return std::unexpected(std::move(kind.error()));
  m_kind = std::move(*kind);
  auto humanoidPath = config.getString("humanoidConfig", "/humanoid.config");
  if (!humanoidPath) return std::unexpected(std::move(humanoidPath.error()));
  m_humanoidConfig = std::move(*humanoidPath);
  auto overrides = config.getObject("humanoidOverrides", JsonObject());
  if (!overrides) return std::unexpected(std::move(overrides.error()));
  m_humanoidOverrides = std::move(*overrides);
  auto readStrings = [&](String const& key, Json fallback, List<String>& destination) -> std::expected<void, StarException> {
    auto value = config.get(key, std::move(fallback));
    if (!value) return std::unexpected(std::move(value.error()));
    auto strings = jsonToStringList(*value);
    if (!strings) return std::unexpected(std::move(strings.error()));
    destination = std::move(*strings);
    return {};
  };
  auto build = readStrings("buildScripts", JsonArray{}, m_buildScripts);
  if (!build) return std::unexpected(std::move(build.error()));
  auto creation = readStrings("createScripts", JsonArray{}, m_creationScripts);
  if (!creation) return std::unexpected(std::move(creation.error()));
  auto tooltip = config.get("charCreationTooltip");
  if (!tooltip) return std::unexpected(std::move(tooltip.error()));
  auto title = tooltip->getString("title", "");
  if (!title) return std::unexpected(std::move(title.error()));
  m_tooltip.title = std::move(*title);
  auto subtitle = tooltip->getString("subTitle", "");
  if (!subtitle) return std::unexpected(std::move(subtitle.error()));
  m_tooltip.subTitle = std::move(*subtitle);
  auto description = tooltip->getString("description", "");
  if (!description) return std::unexpected(std::move(description.error()));
  m_tooltip.description = std::move(*description);
  auto names = config.get("nameGen");
  if (!names) return std::unexpected(std::move(names.error()));
  auto nameList = jsonToStringList(*names);
  if (!nameList) return std::unexpected(std::move(nameList.error()));
  m_nameGen = std::move(*nameList);
  auto labels = readStrings("charGenTextLabels", JsonArray{}, m_charGenTextLabels);
  if (!labels) return std::unexpected(std::move(labels.error()));
  auto skull = config.getString("skull", "/humanoid/any/dead.png");
  if (!skull) return std::unexpected(std::move(skull.error()));
  m_skull = std::move(*skull);
  auto noises = config.get("ouchNoises");
  if (!noises) return std::unexpected(std::move(noises.error()));
  auto noiseList = jsonToStringList(*noises);
  if (!noiseList) return std::unexpected(std::move(noiseList.error()));
  m_ouchNoises = std::move(*noiseList);
  if (m_nameGen.size() < GenderNames.size() || m_ouchNoises.size() < GenderNames.size())
    return std::unexpected(
      StarException::format("Species '{}' must have at least {} nameGen and {} ouchNoises entries, has {} and {}",
                            m_kind, GenderNames.size(), GenderNames.size(), m_nameGen.size(), m_ouchNoises.size()));
  auto items = config.getArray("defaultItems", JsonArray{});
  if (!items) return std::unexpected(std::move(items.error()));
  for (auto const& value : *items) {
    auto item = ItemDescriptor::create(value);
    if (!item) return std::unexpected(std::move(item.error()));
    m_defaultItems.append(std::move(*item));
  }
  auto blueprints = config.get("defaultBlueprints", JsonObject{});
  if (!blueprints) return std::unexpected(std::move(blueprints.error()));
  auto tier = blueprints->getArray("tier1", JsonArray{});
  if (!tier) return std::unexpected(std::move(tier.error()));
  for (auto const& value : *tier) {
    auto item = ItemDescriptor::create(value);
    if (!item) return std::unexpected(std::move(item.error()));
    m_defaultBlueprints.append(std::move(*item));
  }
  auto personalitiesLoaded = [&]() -> std::expected<void, StarException> {
  auto humanoid = humanoidConfig();
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  auto personalities = humanoid->getArray("personalities");
  if (!personalities) return std::unexpected(std::move(personalities.error()));
  for (auto const& value : *personalities) {
    auto personality = parsePersonalityArray(value);
    if (!personality) return std::unexpected(std::move(personality.error()));
    m_personalities.push_back(std::move(*personality));
  }
  return {};
  }();
  if (!personalitiesLoaded) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(personalitiesLoaded.error()));
    return std::unexpected((*assets)->withAssetContext(std::move(personalitiesLoaded.error()), m_humanoidConfig, "Loading species humanoid personalities"));
  }
  auto effects = config.getArray("statusEffects", {});
  if (!effects) return std::unexpected(std::move(effects.error()));
  for (auto const& value : *effects) {
    auto effect = jsonToPersistentStatusEffect(value);
    if (!effect) return std::unexpected(std::move(effect.error()));
    m_statusEffects.append(std::move(*effect));
  }
  auto directives = config.getString("effectDirectives", "");
  if (!directives) return std::unexpected(std::move(directives.error()));
  m_effectDirectives = std::move(*directives);
  SpeciesOption species;
  species.species = m_kind;
  auto readBool = [&](String const& key, bool& destination) -> std::expected<void, StarException> {
    auto value = config.getBool(key, false);
    if (!value) return std::unexpected(std::move(value.error()));
    destination = *value;
    return {};
  };
  auto headHair = readBool("headOptionAsHairColor", species.headOptionAsHairColor);
  if (!headHair) return std::unexpected(std::move(headHair.error()));
  auto headFacial = readBool("headOptionAsFacialhair", species.headOptionAsFacialhair);
  if (!headFacial) return std::unexpected(std::move(headFacial.error()));
  auto altUndy = readBool("altOptionAsUndyColor", species.altOptionAsUndyColor);
  if (!altUndy) return std::unexpected(std::move(altUndy.error()));
  auto altHair = readBool("altOptionAsHairColor", species.altOptionAsHairColor);
  if (!altHair) return std::unexpected(std::move(altHair.error()));
  auto altMask = readBool("altOptionAsFacialMask", species.altOptionAsFacialMask);
  if (!altMask) return std::unexpected(std::move(altMask.error()));
  auto hairBody = readBool("hairColorAsBodySubColor", species.hairColorAsBodySubColor);
  if (!hairBody) return std::unexpected(std::move(hairBody.error()));
  auto bodyMask = readBool("bodyColorAsFacialMaskSubColor", species.bodyColorAsFacialMaskSubColor);
  if (!bodyMask) return std::unexpected(std::move(bodyMask.error()));
  auto altColor = readBool("altColorAsFacialMaskSubColor", species.altColorAsFacialMaskSubColor);
  if (!altColor) return std::unexpected(std::move(altColor.error()));
  auto readColors = [&](String const& key, List<String>& destination) -> std::expected<void, StarException> {
    auto values = config.getArray(key, JsonArray{""});
    if (!values) return std::unexpected(std::move(values.error()));
    auto converted = colorDirectivesFromConfig(*values);
    if (!converted) return std::unexpected(std::move(converted.error()));
    destination = std::move(*converted);
    return {};
  };
  auto body = readColors("bodyColor", species.bodyColorDirectives);
  if (!body) return std::unexpected(std::move(body.error()));
  auto undy = readColors("undyColor", species.undyColorDirectives);
  if (!undy) return std::unexpected(std::move(undy.error()));
  auto hair = readColors("hairColor", species.hairColorDirectives);
  if (!hair) return std::unexpected(std::move(hair.error()));
  auto genders = config.getArray("genders", JsonArray{});
  if (!genders) return std::unexpected(std::move(genders.error()));
  for (auto const& data : *genders) {
    SpeciesGenderOption gender;
    gender.gender = species.genderOptions.empty() ? Gender::Male : Gender::Female;
    auto readString = [&](String const& key, String fallback, String& destination) -> std::expected<void, StarException> {
      auto value = data.getString(key, std::move(fallback));
      if (!value) return std::unexpected(std::move(value.error()));
      destination = std::move(*value);
      return {};
    };
    auto name = readString("name", "", gender.name);
    if (!name) return std::unexpected(std::move(name.error()));
    auto image = readString("image", "", gender.image);
    if (!image) return std::unexpected(std::move(image.error()));
    auto characterImage = readString("characterImage", "", gender.characterImage);
    if (!characterImage) return std::unexpected(std::move(characterImage.error()));
    auto readOptions = [&](String const& key, List<String>& destination) -> std::expected<void, StarException> {
      auto fallback = config.get(key, JsonArray{""});
      if (!fallback) return std::unexpected(std::move(fallback.error()));
      auto value = data.get(key, *fallback);
      if (!value) return std::unexpected(std::move(value.error()));
      auto strings = jsonToStringList(*value);
      if (!strings) return std::unexpected(std::move(strings.error()));
      destination = std::move(*strings);
      return {};
    };
    auto hairOptions = readOptions("hair", gender.hairOptions);
    if (!hairOptions) return std::unexpected(std::move(hairOptions.error()));
    auto hairGroup = config.getString("HairGroup", "hair");
    if (!hairGroup) return std::unexpected(std::move(hairGroup.error()));
    auto hairGroupRead = readString("hairGroup", *hairGroup, gender.hairGroup);
    if (!hairGroupRead) return std::unexpected(std::move(hairGroupRead.error()));
    auto shirts = readOptions("shirt", gender.shirtOptions);
    if (!shirts) return std::unexpected(std::move(shirts.error()));
    auto pants = readOptions("pants", gender.pantsOptions);
    if (!pants) return std::unexpected(std::move(pants.error()));
    auto facialHairGroup = config.getString("facialHairGroup", "");
    if (!facialHairGroup) return std::unexpected(std::move(facialHairGroup.error()));
    auto facialHairRead = readString("facialHairGroup", *facialHairGroup, gender.facialHairGroup);
    if (!facialHairRead) return std::unexpected(std::move(facialHairRead.error()));
    auto facialHair = readOptions("facialHair", gender.facialHairOptions);
    if (!facialHair) return std::unexpected(std::move(facialHair.error()));
    auto facialMaskGroup = config.getString("facialMaskGroup", "");
    if (!facialMaskGroup) return std::unexpected(std::move(facialMaskGroup.error()));
    auto facialMaskRead = readString("facialMaskGroup", *facialMaskGroup, gender.facialMaskGroup);
    if (!facialMaskRead) return std::unexpected(std::move(facialMaskRead.error()));
    auto facialMask = readOptions("facialMask", gender.facialMaskOptions);
    if (!facialMask) return std::unexpected(std::move(facialMask.error()));
    species.genderOptions.append(std::move(gender));
  }
  m_options = std::move(species);
  return {};
}

Json SpeciesDefinition::config() const {
  return m_config;
}

String SpeciesDefinition::kind() const {
  return m_kind;
}

String SpeciesDefinition::nameGen(Gender gender) const { return m_nameGen[static_cast<unsigned>(gender)]; }

String SpeciesDefinition::ouchNoise(Gender gender) const { return m_ouchNoises[static_cast<unsigned>(gender)]; }

SpeciesOption const& SpeciesDefinition::options() const {
  return m_options;
}

std::expected<Json, StarException> SpeciesDefinition::humanoidConfig() const {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(m_humanoidConfig);
  if (!config) return std::unexpected((*assets)->withAssetContext(std::move(config.error()), m_humanoidConfig, "Loading species humanoid configuration"));
  return jsonMerge(*config, m_humanoidOverrides);
}

List<Personality> const& SpeciesDefinition::personalities() const {
  return m_personalities;
}

List<ItemDescriptor> SpeciesDefinition::defaultItems() const {
  return m_defaultItems;
}

List<ItemDescriptor> SpeciesDefinition::defaultBlueprints() const {
  return m_defaultBlueprints;
}

StringList SpeciesDefinition::charGenTextLabels() const {
  return m_charGenTextLabels;
}

SpeciesCharCreationTooltip const& SpeciesDefinition::tooltip() const {
  return m_tooltip;
}

String SpeciesDefinition::skull() const {
  return m_skull;
}

List<PersistentStatusEffect> SpeciesDefinition::statusEffects() const {
  return m_statusEffects;
}

String SpeciesDefinition::effectDirectives() const {
  return m_effectDirectives;
}

}
