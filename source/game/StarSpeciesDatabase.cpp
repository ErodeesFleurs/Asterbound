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
  auto files = (*assets)->scan_extension("species");
  if (!files) return std::unexpected(std::move(files.error()));
  auto queued = (*assets)->queue_jsons(files->get());
  if (!queued) return std::unexpected(std::move(queued.error()));
  for (auto const& file : files->get()) {
    auto loaded = [&]() -> std::expected<void, StarException> {
    auto config = (*assets)->json(file.string());
    if (!config) return std::unexpected(std::move(config.error()));
    auto definition = SpeciesDefinition::create(root, *config);
    if (!definition) return std::unexpected(std::move(definition.error()));
    if (result->m_species.contains((*definition)->kind()))
      return std::unexpected(StarException::format("Duplicate species asset with kind {}. configfile {}", (*definition)->kind(), file.string()));
    auto kind = (*definition)->kind().to_lower();
    if (!kind) return std::unexpected(std::move(kind.error()));
    result->m_species[*kind] = std::move(*definition);
    return {};
    }();
    if (!loaded)
      return std::unexpected((*assets)->with_asset_context(std::move(loaded.error()), file.string(), "Loading species"));
  }
  return result;
}

std::expected<std::shared_ptr<SpeciesDefinition>, StarException> SpeciesDatabase::species(String const& kind) const {
  auto key = kind.to_lower();
  if (!key) return std::unexpected(std::move(key.error()));
  auto found = m_species.find(*key);
  if (found == m_species.end())
    return std::unexpected(StarException::format("Unknown species kind '{}'.", kind));
  return found->second;
}

StringMap<std::shared_ptr<SpeciesDefinition>> SpeciesDatabase::allSpecies() const {
  return m_species;
}

std::expected<Json, StarException> SpeciesDatabase::humanoid_config(HumanoidIdentity identity, JsonObject parameters, Json config, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) const {
  auto definition = species(identity.species);
  if (!definition) return std::unexpected(std::move(definition.error()));
  if ((*definition)->m_buildScripts.empty() && config.is_type(Json::Type::Object))
    return config;
  auto base_config = (*definition)->humanoid_config();
  if (!base_config) return std::unexpected(std::move(base_config.error()));
  if (!(*definition)->m_buildScripts.empty()) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lock_with_diagnostics(locker);
    auto context = m_luaRoot->create_context((*definition)->m_buildScripts);
    if (!context) return std::unexpected(std::move(context.error()));
    auto removed = context->remove("root");
    if (!removed) return std::unexpected(std::move(removed.error()));
    auto root_callbacks = LuaBindings::makeRootCallbacks(m_root, item_catalog);
    if (!root_callbacks) return std::unexpected(std::move(root_callbacks.error()));
    auto installed = context->set_callbacks("root", *root_callbacks);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto utility = LuaBindings::make_utility_callbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utility) return std::unexpected(std::move(utility.error()));
    installed = context->set_callbacks("sb", *utility);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto identity_json = identity.to_json();
    if (!identity_json) return std::unexpected(std::move(identity_json.error()));
    return context->invoke_path<Json>("build", *identity_json, parameters, *base_config, config);
  }
  return json_merge(*base_config, parameters);
}

std::expected<CharacterCreationResult, StarException> SpeciesDatabase::create_humanoid(
  String name,
  String species_choice,
  std::size_t gender_choice,
  std::size_t body_color_choice,
  std::size_t alty,
  std::size_t hair_choice,
  std::size_t heady,
  std::size_t shirt_choice,
  std::size_t shirt_color,
  std::size_t pants_choice,
  std::size_t pants_color,
  std::size_t personality,
  LuaVariadic<LuaValue> ext,
  std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog
) const {
  CharacterCreationResult result;

  auto definition = species(species_choice);
  if (!definition) return std::unexpected(std::move(definition.error()));
  auto const& species_definition = *definition;
  if (!species_definition->m_creationScripts.empty()) {
    std::unique_lock locker(m_luaMutex, std::defer_lock);
    lock_with_diagnostics(locker);
    auto context = m_luaRoot->create_context(species_definition->m_creationScripts);
    if (!context) return std::unexpected(std::move(context.error()));
    auto removed = context->remove("root");
    if (!removed) return std::unexpected(std::move(removed.error()));
    auto callbacks = LuaBindings::makeRootCallbacks(m_root, item_catalog);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    auto installed = context->set_callbacks("root", *callbacks);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto utility = LuaBindings::make_utility_callbacks(m_root.random(), m_root.logger(), m_root.logMap());
    if (!utility) return std::unexpected(std::move(utility.error()));
    installed = context->set_callbacks("sb", *utility);
    if (!installed) return std::unexpected(std::move(installed.error()));
    auto created = context->invoke_path<LuaTupleReturn<Json, JsonObject, JsonObject>>(
      "create", name, species_choice, gender_choice, body_color_choice, alty, hair_choice,
      heady, shirt_choice, shirt_color, pants_choice, pants_color, personality, ext);
    if (!created) return std::unexpected(std::move(created.error()));
    Json identity_return;
    lua_tie(identity_return, result.humanoid_parameters, result.armor) = std::move(*created);
    auto identity = HumanoidIdentity::create(identity_return);
    if (!identity) return std::unexpected(std::move(identity.error()));
    result.identity = std::move(*identity);
  } else {
    auto species = species_definition->options();
    auto gender_result = species.genderOptions.wrap(gender_choice);
    if (!gender_result) return std::unexpected(std::move(gender_result.error()));
    auto const& gender = gender_result->get();
    auto body_result = species.bodyColorDirectives.wrap(body_color_choice);
    if (!body_result) return std::unexpected(std::move(body_result.error()));
    auto body_color = body_result->get();

    String alt_color;

    if (species.altOptionAsUndyColor) {
      // undyColor
      auto selected = species.undyColorDirectives.wrap(alty);
      if (!selected) return std::unexpected(std::move(selected.error()));
      alt_color = selected->get();
    }

    auto hair = gender.hairOptions.wrap(hair_choice);
    if (!hair) return std::unexpected(std::move(hair.error()));
    String hair_color = body_color;
    if (species.headOptionAsHairColor) {
      auto selected = species.hairColorDirectives.wrap(heady);
      if (!selected) return std::unexpected(std::move(selected.error()));
      hair_color = selected->get();
      if (species.altOptionAsHairColor) {
        auto alt = species.undyColorDirectives.wrap(alty);
        if (!alt) return std::unexpected(std::move(alt.error()));
        hair_color += alt->get();
      }
    }

    if (species.hairColorAsBodySubColor)
      body_color += hair_color;

    String facial_hair;
    String facial_hair_group;
    String facial_hair_directive;
    if (species.headOptionAsFacialhair) {
      auto selected = gender.facialHairOptions.wrap(heady);
      if (!selected) return std::unexpected(std::move(selected.error()));
      facial_hair = selected->get();
      facial_hair_group = gender.facialHairGroup;
      facial_hair_directive = hair_color;
    }

    String facial_mask;
    String facial_mask_group;
    String facial_mask_directive;
    if (species.altOptionAsFacialMask) {
      auto selected = gender.facialMaskOptions.wrap(alty);
      if (!selected) return std::unexpected(std::move(selected.error()));
      facial_mask = selected->get();
      facial_mask_group = gender.facialMaskGroup;
      facial_mask_directive = "";
    }
    if (species.bodyColorAsFacialMaskSubColor)
      facial_mask_directive += body_color;
    if (species.altColorAsFacialMaskSubColor)
      facial_mask_directive += alt_color;

    auto shirt = gender.shirtOptions.wrap(shirt_choice);
    if (!shirt) return std::unexpected(std::move(shirt.error()));
    auto pants = gender.pantsOptions.wrap(pants_choice);
    if (!pants) return std::unexpected(std::move(pants.error()));

    auto personality_result = species_definition->personalities().wrap(personality);
    if (!personality_result) return std::unexpected(std::move(personality_result.error()));

    result.identity.name = name;
    result.identity.species = species.species;
    result.identity.body_directives = body_color + alt_color;
    auto selected_gender = enum_value<gender_names>(gender.name);
    if (!selected_gender) return std::unexpected(std::move(selected_gender.error()));
    result.identity.gender = *selected_gender;
    result.identity.hair_group = gender.hairGroup;
    result.identity.hair_type = hair->get();
    result.identity.hair_directives = hair_color;
    result.identity.emote_directives = body_color + alt_color;
    result.identity.facial_hair_group = facial_hair_group;
    result.identity.facial_hair_type = facial_hair;
    result.identity.facial_hair_directives = facial_hair_directive;
    result.identity.facial_mask_group = facial_mask_group;
    result.identity.facial_mask_type = facial_mask;
    result.identity.facial_mask_directives = facial_mask_directive;
    result.identity.personality = personality_result->get();

    auto chest_slot = enum_name<equipment_slot_names>(EquipmentSlot::Chest);
    if (!chest_slot) return std::unexpected(std::move(chest_slot.error()));
    auto leg_slot = enum_name<equipment_slot_names>(EquipmentSlot::Legs);
    if (!leg_slot) return std::unexpected(std::move(leg_slot.error()));
    auto chest = ItemDescriptor::create(shirt->get(), 1, JsonObject{{"colorIndex", shirt_color}});
    if (!chest) return std::unexpected(std::move(chest.error()));
    auto legs = ItemDescriptor::create(pants->get(), 1, JsonObject{{"colorIndex", pants_color}});
    if (!legs) return std::unexpected(std::move(legs.error()));
    result.armor.set(*chest_slot, chest->to_json());
    result.armor.set(*leg_slot, legs->to_json());

    result.humanoid_parameters.set("choices", JsonArray({gender_choice, body_color_choice, alty, hair_choice, heady, shirt_choice, shirt_color, pants_choice, pants_color, personality}));
  }

  return result;
}

std::expected<CharacterCreationResult, StarException> SpeciesDatabase::generate_humanoid(String species_choice, std::int64_t seed, std::optional<Gender> gender, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog) const {
  RandomSource rand_source(seed);
  auto definition = species(species_choice);
  if (!definition) return std::unexpected(std::move(definition.error()));
  auto chosen_gender = gender.value_or(rand_source.next_bool() ? Gender::Male : Gender::Female);
  auto generator = m_root.nameGenerator();
  if (!generator) return std::unexpected(std::move(generator.error()));
  auto name = (*generator)->generateName((*definition)->nameGen(chosen_gender), rand_source);
  if (!name) return std::unexpected(std::move(name.error()));
  return create_humanoid(std::move(*name), species_choice, static_cast<unsigned>(chosen_gender), rand_source.next_u32(),
                        rand_source.next_u32(), rand_source.next_u32(), rand_source.next_u32(), rand_source.next_u32(),
                        rand_source.next_u32(), rand_source.next_u32(), rand_source.next_u32(), rand_source.next_u32(), {},
                        item_catalog);
}

std::expected<std::shared_ptr<SpeciesDefinition>, StarException> SpeciesDefinition::create(Root& root, Json const& config) {
  auto result = std::shared_ptr<SpeciesDefinition>(new SpeciesDefinition(root));
  auto initialized = result->init(config);
  if (!initialized) return std::unexpected(std::move(initialized.error()));
  return result;
}

std::expected<void, StarException> SpeciesDefinition::init(Json const& config) {
  m_config = config;
  auto kind = config.get_string("kind");
  if (!kind) return std::unexpected(std::move(kind.error()));
  m_kind = std::move(*kind);
  auto humanoid_path = config.get_string("humanoidConfig", "/humanoid.config");
  if (!humanoid_path) return std::unexpected(std::move(humanoid_path.error()));
  m_humanoid_config = std::move(*humanoid_path);
  auto overrides = config.get_object("humanoidOverrides", JsonObject());
  if (!overrides) return std::unexpected(std::move(overrides.error()));
  m_humanoid_overrides = std::move(*overrides);
  auto readStrings = [&](String const& key, Json fallback, List<String>& destination) -> std::expected<void, StarException> {
    auto value = config.get(key, std::move(fallback));
    if (!value) return std::unexpected(std::move(value.error()));
    auto strings = json_to_string_list(*value);
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
  auto title = tooltip->get_string("title", "");
  if (!title) return std::unexpected(std::move(title.error()));
  m_tooltip.title = std::move(*title);
  auto subtitle = tooltip->get_string("subTitle", "");
  if (!subtitle) return std::unexpected(std::move(subtitle.error()));
  m_tooltip.subTitle = std::move(*subtitle);
  auto description = tooltip->get_string("description", "");
  if (!description) return std::unexpected(std::move(description.error()));
  m_tooltip.description = std::move(*description);
  auto names = config.get("nameGen");
  if (!names) return std::unexpected(std::move(names.error()));
  auto nameList = json_to_string_list(*names);
  if (!nameList) return std::unexpected(std::move(nameList.error()));
  m_nameGen = std::move(*nameList);
  auto labels = readStrings("charGenTextLabels", JsonArray{}, m_charGenTextLabels);
  if (!labels) return std::unexpected(std::move(labels.error()));
  auto skull = config.get_string("skull", "/humanoid/any/dead.png");
  if (!skull) return std::unexpected(std::move(skull.error()));
  m_skull = std::move(*skull);
  auto noises = config.get("ouchNoises");
  if (!noises) return std::unexpected(std::move(noises.error()));
  auto noiseList = json_to_string_list(*noises);
  if (!noiseList) return std::unexpected(std::move(noiseList.error()));
  m_ouchNoises = std::move(*noiseList);
  if (m_nameGen.size() < gender_names.size() || m_ouchNoises.size() < gender_names.size())
    return std::unexpected(
      StarException::format("Species '{}' must have at least {} nameGen and {} ouchNoises entries, has {} and {}",
                            m_kind, gender_names.size(), gender_names.size(), m_nameGen.size(), m_ouchNoises.size()));
  auto items = config.get_array("defaultItems", JsonArray{});
  if (!items) return std::unexpected(std::move(items.error()));
  for (auto const& value : *items) {
    auto item = ItemDescriptor::create(value);
    if (!item) return std::unexpected(std::move(item.error()));
    m_defaultItems.push_back(std::move(*item));
  }
  auto blueprints = config.get("defaultBlueprints", JsonObject{});
  if (!blueprints) return std::unexpected(std::move(blueprints.error()));
  auto tier = blueprints->get_array("tier1", JsonArray{});
  if (!tier) return std::unexpected(std::move(tier.error()));
  for (auto const& value : *tier) {
    auto item = ItemDescriptor::create(value);
    if (!item) return std::unexpected(std::move(item.error()));
    m_defaultBlueprints.push_back(std::move(*item));
  }
  auto personalitiesLoaded = [&]() -> std::expected<void, StarException> {
  auto humanoid = humanoid_config();
  if (!humanoid) return std::unexpected(std::move(humanoid.error()));
  auto personalities = humanoid->get_array("personalities");
  if (!personalities) return std::unexpected(std::move(personalities.error()));
  for (auto const& value : *personalities) {
    auto personality = parse_personality_array(value);
    if (!personality) return std::unexpected(std::move(personality.error()));
    m_personalities.push_back(std::move(*personality));
  }
  return {};
  }();
  if (!personalitiesLoaded) {
    auto assets = m_root.assets();
    if (!assets) return std::unexpected(std::move(personalitiesLoaded.error()));
    return std::unexpected((*assets)->with_asset_context(std::move(personalitiesLoaded.error()), m_humanoid_config, "Loading species humanoid personalities"));
  }
  auto effects = config.get_array("statusEffects", {});
  if (!effects) return std::unexpected(std::move(effects.error()));
  for (auto const& value : *effects) {
    auto effect = jsonToPersistentStatusEffect(value);
    if (!effect) return std::unexpected(std::move(effect.error()));
    m_statusEffects.push_back(std::move(*effect));
  }
  auto directives = config.get_string("effectDirectives", "");
  if (!directives) return std::unexpected(std::move(directives.error()));
  m_effectDirectives = std::move(*directives);
  SpeciesOption species;
  species.species = m_kind;
  auto readBool = [&](String const& key, bool& destination) -> std::expected<void, StarException> {
    auto value = config.get_bool(key, false);
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
    auto values = config.get_array(key, JsonArray{""});
    if (!values) return std::unexpected(std::move(values.error()));
    auto converted = color_directives_from_config(*values);
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
  auto genders = config.get_array("genders", JsonArray{});
  if (!genders) return std::unexpected(std::move(genders.error()));
  for (auto const& data : *genders) {
    SpeciesGenderOption gender;
    gender.gender = species.genderOptions.empty() ? Gender::Male : Gender::Female;
    auto readString = [&](String const& key, String fallback, String& destination) -> std::expected<void, StarException> {
      auto value = data.get_string(key, std::move(fallback));
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
      auto strings = json_to_string_list(*value);
      if (!strings) return std::unexpected(std::move(strings.error()));
      destination = std::move(*strings);
      return {};
    };
    auto hairOptions = readOptions("hair", gender.hairOptions);
    if (!hairOptions) return std::unexpected(std::move(hairOptions.error()));
    auto hairGroup = config.get_string("HairGroup", "hair");
    if (!hairGroup) return std::unexpected(std::move(hairGroup.error()));
    auto hairGroupRead = readString("hairGroup", *hairGroup, gender.hairGroup);
    if (!hairGroupRead) return std::unexpected(std::move(hairGroupRead.error()));
    auto shirts = readOptions("shirt", gender.shirtOptions);
    if (!shirts) return std::unexpected(std::move(shirts.error()));
    auto pants = readOptions("pants", gender.pantsOptions);
    if (!pants) return std::unexpected(std::move(pants.error()));
    auto facialHairGroup = config.get_string("facialHairGroup", "");
    if (!facialHairGroup) return std::unexpected(std::move(facialHairGroup.error()));
    auto facialHairRead = readString("facialHairGroup", *facialHairGroup, gender.facialHairGroup);
    if (!facialHairRead) return std::unexpected(std::move(facialHairRead.error()));
    auto facialHair = readOptions("facialHair", gender.facialHairOptions);
    if (!facialHair) return std::unexpected(std::move(facialHair.error()));
    auto facialMaskGroup = config.get_string("facialMaskGroup", "");
    if (!facialMaskGroup) return std::unexpected(std::move(facialMaskGroup.error()));
    auto facialMaskRead = readString("facialMaskGroup", *facialMaskGroup, gender.facialMaskGroup);
    if (!facialMaskRead) return std::unexpected(std::move(facialMaskRead.error()));
    auto facialMask = readOptions("facialMask", gender.facialMaskOptions);
    if (!facialMask) return std::unexpected(std::move(facialMask.error()));
    species.genderOptions.push_back(std::move(gender));
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

std::expected<Json, StarException> SpeciesDefinition::humanoid_config() const {
  auto assets = m_root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json(m_humanoid_config);
  if (!config) return std::unexpected((*assets)->with_asset_context(std::move(config.error()), m_humanoid_config, "Loading species humanoid configuration"));
  return json_merge(*config, m_humanoid_overrides);
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
