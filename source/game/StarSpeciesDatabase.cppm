module;
import std;
import star.format;
import star.lua;
import star.json;
import star.thread;
import star.data_stream;
import star.bi_map;
import star.game_types;
import star.string;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.asset_path;
import star.random;
import star.periodic_function;
import star.ordered_map;
import star.audio;
import star.map;
import star.set;
import star.strong_typedef;
import star.id_map;


import star.item_descriptor;
import star.drawable;
import star.animation;
import star.particle;
import star.animated_part_set;
import star.mixer;
import star.light_source;
import star.networked_animator;
import star.humanoid;
import star.status_types;
import star.lua_root;

namespace Star {

class Root;
class ItemDatabase;
class SpeciesDatabase;

struct SpeciesCharCreationTooltip {
  String title;
  String subTitle;
  String description;
};

struct SpeciesGenderOption {
  Gender gender;
  String name;
  String image;
  String characterImage;
  List<String> hairOptions;
  String hairGroup;
  List<String> shirtOptions;
  List<String> pantsOptions;
  String facialHairGroup;
  List<String> facialHairOptions;
  String facialMaskGroup;
  List<String> facialMaskOptions;
};

struct SpeciesOption {
  SpeciesOption();

  String species;
  bool headOptionAsHairColor;
  bool headOptionAsFacialhair;
  bool altOptionAsUndyColor;
  bool altOptionAsHairColor;
  bool altOptionAsFacialMask;
  bool hairColorAsBodySubColor;
  bool bodyColorAsFacialMaskSubColor;
  bool altColorAsFacialMaskSubColor;
  List<SpeciesGenderOption> genderOptions;
  List<String> bodyColorDirectives;
  List<String> undyColorDirectives;
  List<String> hairColorDirectives;
};

struct CharacterCreationResult {
  HumanoidIdentity identity;
  JsonObject humanoid_parameters;
  JsonObject armor;
};

class SpeciesDefinition {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SpeciesDefinition>, StarException> create(Root& root, Json const& config);

  Json config() const;
  String kind() const;
  bool playerSelectable() const;
  SpeciesOption const& options() const;
  [[nodiscard]] std::expected<Json, StarException> humanoid_config() const;
  List<Personality> const& personalities() const;
  String nameGen(Gender gender) const;
  String ouchNoise(Gender gender) const;
  List<ItemDescriptor> defaultItems() const;
  List<ItemDescriptor> defaultBlueprints() const;
  StringList charGenTextLabels() const;
  String skull() const;
  List<PersistentStatusEffect> statusEffects() const;
  String effectDirectives() const;

  SpeciesCharCreationTooltip const& tooltip() const;

private:
  explicit SpeciesDefinition(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> init(Json const& config);
  Root& m_root;
  String m_kind;
  SpeciesCharCreationTooltip m_tooltip;
  bool m_playerSelectable;
  Json m_config;
  String m_humanoid_config;
  Json m_humanoid_overrides;
  List<Personality> m_personalities;
  List<String> m_nameGen;
  List<String> m_ouchNoises;
  SpeciesOption m_options;
  List<ItemDescriptor> m_defaultItems;
  List<ItemDescriptor> m_defaultBlueprints;
  StringList m_charGenTextLabels;
  String m_skull;
  List<PersistentStatusEffect> m_statusEffects;
  String m_effectDirectives;

  List<String> m_buildScripts;
  List<String> m_creationScripts;

  friend class SpeciesDatabase;
};

class SpeciesDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SpeciesDatabase>, StarException> create(Root& root);

  [[nodiscard]] std::expected<std::shared_ptr<SpeciesDefinition>, StarException> species(String const& kind) const;
  StringMap<std::shared_ptr<SpeciesDefinition>> allSpecies() const;

  [[nodiscard]] std::expected<Json, StarException> humanoid_config(HumanoidIdentity identity, JsonObject parameters = JsonObject(), Json config = Json(), std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;
  [[nodiscard]] std::expected<CharacterCreationResult, StarException> create_humanoid(String name, String species_choice, std::size_t gender_choice, std::size_t body_color, std::size_t alty, std::size_t hair_choice, std::size_t heady, std::size_t shirt_choice, std::size_t shirt_color, std::size_t pants_choice, std::size_t pants_color, std::size_t personality, LuaVariadic<LuaValue> ext = {}, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;

  [[nodiscard]] std::expected<CharacterCreationResult, StarException> generate_humanoid(String species, std::int64_t seed, std::optional<Gender> = {}, std::optional<std::reference_wrapper<ItemDatabase const>> item_catalog = std::nullopt) const;

private:
  explicit SpeciesDatabase(Root& root) : m_root(root) {}
  Root& m_root;

  StringMap<std::shared_ptr<SpeciesDefinition>> m_species;

  mutable std::recursive_mutex m_luaMutex;
  std::shared_ptr<LuaRoot> m_luaRoot;
};

}

export module star.species_database;

export namespace Star {
  using ::Star::SpeciesCharCreationTooltip;
  using ::Star::SpeciesGenderOption;
  using ::Star::SpeciesOption;
  using ::Star::CharacterCreationResult;
  using ::Star::SpeciesDefinition;
  using ::Star::SpeciesDatabase;
}
