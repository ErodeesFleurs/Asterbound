import std;
import star.algorithm;
import star.assets;
import star.collision_block;
import star.file;
import star.format;
import star.game_test;
import star.game_types;
import star.image_metadata_database;
import star.json;
import star.liquid_types;
import star.logging;
import star.map;
import star.material_database;
import star.material_render_profile;
import star.material_types;
import star.particle_database;
import star.root;
import star.string;
import star.tile_damage;
import star.unicode;

#include "gtest/gtest.h"

using namespace Star;

TEST(MaterialHueTest, FiniteDegreesTruncateAndWrap) {
  struct HueCase {
    float degrees;
    unsigned hue;
  };
  HueCase const cases[] = {
    {0.0f, 0}, {45.0f, 31}, {-45.0f, 225},
    {360.0f, 0}, {-360.0f, 0}, {405.0f, 31}, {-405.0f, 225},
    {359.0f, 254}, {-359.0f, 2}, {0.5f, 0}, {-0.5f, 0}
  };
  for (auto const& test : cases)
    EXPECT_EQ(static_cast<unsigned>(materialHueFromDegrees(test.degrees)), test.hue) << test.degrees;
}

namespace {

const GameTestRegistration material_falling_behavior("MaterialDatabaseTest", "FallingBehavior", [](GameTest& fixture) {
  auto materials = fixture.root().material_database();
  ASSERT_TRUE(materials) << materials.error().what();
  for (auto const& [name, expected] : {std::pair{String("dirt"), FallingBehavior::Stationary},
                                      std::pair{String("redtoyblock"), FallingBehavior::Falling},
                                      std::pair{String("sand"), FallingBehavior::Cascading}}) {
    auto id = (*materials)->material_id(name);
    ASSERT_TRUE(id) << id.error().what();
    auto behavior = (*materials)->falling_behavior(*id);
    ASSERT_TRUE(behavior) << behavior.error().what();
    EXPECT_EQ(*behavior, expected);
  }
  for (auto id : {FirstMetaMaterialId, StructureMaterialId, EmptyMaterialId, NullMaterialId}) {
    auto behavior = (*materials)->falling_behavior(id);
    ASSERT_TRUE(behavior) << behavior.error().what();
    EXPECT_EQ(*behavior, FallingBehavior::Stationary);
  }
  MaterialId missing = 0;
  while (isRealMaterial(missing) && (*materials)->is_valid_material_id(missing))
    ++missing;
  ASSERT_TRUE(isRealMaterial(missing));
  EXPECT_FALSE((*materials)->falling_behavior(missing));
});

const GameTestRegistration
  material_rule_join_names("MaterialRenderProfile", "RuleJoinNamesPreserveDefaultsCaseAndErrors", [](GameTest& fixture) {
    auto metadata = fixture.root().image_metadata_database();
    ASSERT_TRUE(metadata) << metadata.error().what();
    auto make_spec = [](Json rule) -> Json {
      return JsonObject{{"representativePiece", ""},
                        {"rules", JsonObject{{"test", std::move(rule)}}},
                        {"pieces", JsonObject{}},
                        {"matches", JsonArray{JsonArray{"main", JsonArray{}}}}};
    };
    for (auto const& [rule, expected] : {std::pair{Json(JsonObject{}), MaterialJoinType::All},
                                         std::pair{Json(JsonObject{{"join", "aLl"}}), MaterialJoinType::All},
                                         std::pair{Json(JsonObject{{"join", "aNy"}}), MaterialJoinType::Any}}) {
      auto profile = parseMaterialRenderProfile(**metadata, make_spec(rule));
      ASSERT_TRUE(profile) << profile.error().what();
      auto parsed = profile->rules.get("test");
      ASSERT_TRUE(parsed) << parsed.error().what();
      EXPECT_EQ(parsed->get()->join, expected);
    }
    for (String const& name : {String("unknown"), String("日本語"), String(std::string_view("All\0", 4))}) {
      auto profile = parseMaterialRenderProfile(**metadata, make_spec(JsonObject{{"join", name}}));
      ASSERT_FALSE(profile);
      EXPECT_TRUE(profile.error().is<MapException>());
    }
    auto invalid =
      parseMaterialRenderProfile(**metadata, make_spec(JsonObject{{"join", String(std::string_view("All\xff", 4))}}));
    ASSERT_FALSE(invalid);
    EXPECT_TRUE(invalid.error().is<UnicodeException>());
  });

JsonObject material_fixture(std::int64_t id, String name = "fixture") {
  return {{"materialId", id}, {"materialName", std::move(name)}, {"category", "fixture"}};
}

JsonObject mod_fixture(std::int64_t id, String name = "fixture") {
  return {{"modId", id}, {"modName", std::move(name)}};
}

std::expected<std::shared_ptr<MaterialDatabase>, StarException> load_material_fixtures(
    GameTest& fixture, String const& directory, Logger& logger,
    std::initializer_list<std::pair<String, Json>> files, Json const& metamaterials = JsonArray{}) {
  auto write = [&](String const& name, Json const& json) -> std::expected<void, StarException> {
    auto path = File::relative_to(directory, name);
    auto parent = File::create_directories(File::dir_name(path));
    if (!parent) return std::unexpected(std::move(parent.error()));
    auto contents = json.print_json();
    if (!contents) return std::unexpected(std::move(contents.error()));
    return File::write_file(*contents, path);
  };
  for (auto const& [name, json] : std::initializer_list<std::pair<String, Json>>{
      {"preload.config", JsonArray{}}, {"metamaterials.config", metamaterials},
      {"client.config", JsonObject{{"defaultFootstepSound", "/fixture.ogg"}}},
      {"tiles/defaultDamage.config", JsonObject{{"damageFactors", JsonObject{}}, {"damageRecovery", 0}}}}) {
    auto written = write(name, json);
    if (!written) return std::unexpected(std::move(written.error()));
  }
  for (auto const& [name, json] : files) {
    auto written = write(name, json);
    if (!written) return std::unexpected(std::move(written.error()));
  }

  auto& root = fixture.root();
  auto particles = root.particleDatabase();
  if (!particles) return std::unexpected(std::move(particles.error()));
  auto image_metadata = root.image_metadata_database();
  if (!image_metadata) return std::unexpected(std::move(image_metadata.error()));
  Assets::Settings settings{};
  settings.worker_pool_size = 1;
  settings.asset_time_to_live = 10;
  auto assets = Assets::create(logger, root.random(), root.logMap(), settings, {directory});
  if (!assets) return std::unexpected(std::move(assets.error()));
  return MaterialDatabase::create(**assets, **particles, **image_metadata, logger, root.random());
}

const GameTestRegistration material_catalog_guards("MaterialDatabaseTest", "RejectsInvalidRegistrationAndFieldWidths",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), directory ? "create isolated material fixtures" : directory.error().what())) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove material fixtures"); });
  Logger logger;
  unsigned case_number = 0;
  auto reject = [&](String const& filename, Json spec, std::string_view field, bool require_field = true) {
    auto loaded = load_material_fixtures(fixture, File::relative_to(*directory, std::to_string(case_number++)),
                                         logger, {{filename, std::move(spec)}});
    fixture.check(!loaded && loaded.error().is<MaterialException>() && loaded.error().what().contains(filename.utf8()) &&
                    (!require_field || loaded.error().what().contains(field)),
                  std::format("{} rejects {}: {}", filename.utf8(), field,
                              loaded ? "unexpectedly loaded" : loaded.error().what()));
  };

  constexpr auto id_overflow = std::int64_t{std::numeric_limits<MaterialId>::max()} + 1;
  constexpr auto liquid_overflow = std::int64_t{std::numeric_limits<LiquidId>::max()} + 1;
  constexpr auto harvest_overflow = std::int64_t{std::numeric_limits<unsigned>::max()} + 1;
  for (auto id : {std::int64_t{-1}, std::int64_t{FirstMetaMaterialId}, id_overflow + 1})
    reject("fixture.material", material_fixture(id), "materialId");
  for (auto id : {std::int64_t{-1}, std::int64_t{FirstMetaMod}, id_overflow + 1})
    reject("fixture.matmod", mod_fixture(id), "modId");

  constexpr std::pair<std::string_view, std::int64_t> material_limits[] = {
    {"tillableMod", id_overflow}, {"requiredHarvestLevel", harvest_overflow}};
  for (auto const& [field, overflow] : material_limits) {
    for (auto value : {std::int64_t{-1}, overflow}) {
      auto material = material_fixture(1);
      material[String(field)] = value;
      reject("fixture.material", std::move(material), field);
    }
  }
  for (auto value : {std::int64_t{-1}, harvest_overflow}) {
    auto mod = mod_fixture(1);
    mod["harvestLevel"] = value;
    reject("fixture.matmod", std::move(mod), "harvestLevel");
  }
  for (bool mod : {false, true}) {
    std::string_view transform = mod ? "transformModId" : "transformMaterialId";
    std::pair<std::string_view, std::int64_t> limits[] = {
      {"liquidId", -1}, {"liquidId", liquid_overflow}, {transform, -1}, {transform, id_overflow}};
    for (auto const& [field, value] : limits) {
      auto spec = mod ? mod_fixture(1) : material_fixture(1);
      JsonObject rule{{"liquidId", 1}};
      rule[String(field)] = value;
      spec["liquidInteractions"] = JsonArray{std::move(rule)};
      reject(mod ? String("fixture.matmod") : String("fixture.material"), std::move(spec), field);
    }
  }

  for (bool mod : {false, true}) {
    auto spec = mod ? mod_fixture(1) : material_fixture(1);
    spec["shortdescription"] = false;
    reject(mod ? String("fixture.matmod") : String("fixture.material"), std::move(spec), "shortdescription", false);
  }

  for (bool duplicate_id : {false, true}) {
    auto second_id = duplicate_id ? 1 : 2;
    auto second_name = duplicate_id ? String("other") : String("fixture");
    auto materials = load_material_fixtures(fixture, File::relative_to(*directory, std::to_string(case_number++)),
      logger, {{"a.material", material_fixture(1)}, {"b.material", material_fixture(second_id, second_name)}});
    fixture.check(!materials && materials.error().is<MaterialException>() &&
                    materials.error().what().contains("/a.material") && materials.error().what().contains("/b.material"),
                  materials ? "reject duplicate material identity" : materials.error().what());
    auto mods = load_material_fixtures(fixture, File::relative_to(*directory, std::to_string(case_number++)),
      logger, {{"a.matmod", mod_fixture(1)}, {"b.matmod", mod_fixture(second_id, second_name)}});
    fixture.check(!mods && mods.error().is<MaterialException>() &&
                    mods.error().what().contains("/a.matmod") && mods.error().what().contains("/b.matmod"),
                  mods ? "reject duplicate mod identity" : mods.error().what());
  }
});

class MetamaterialLog final : public LogSink {
public:
  MetamaterialLog() { set_level(LogLevel::Info); }

  std::expected<void, StarException> log(std::string_view message, LogLevel) override {
    if (message.contains("metamaterial:"))
      messages.emplace_back(message);
    return {};
  }

  std::vector<std::string> messages;
};

const GameTestRegistration material_meta_catalog("MaterialDatabaseTest", "LogsAndSkipsInvalidMetamaterials",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), directory ? "create isolated metamaterial fixtures" : directory.error().what())) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove metamaterial fixtures"); });
  Logger logger;
  auto sink = std::make_shared<MetamaterialLog>();
  logger.add_sink(sink);
  constexpr auto meta_id = FirstMetaMaterialId;
  constexpr auto wrapped_id = std::int64_t{std::numeric_limits<MaterialId>::max()} + 1 + meta_id;
  auto meta = [](String name, std::int64_t id) -> Json {
    return JsonObject{{"name", std::move(name)}, {"materialId", id}, {"collisionKind", "platform"}};
  };
  auto loaded = load_material_fixtures(fixture, *directory, logger, {}, JsonArray{
    meta("negative", -1), meta("wrapped", wrapped_id), meta("real", meta_id - 1),
    meta("engine", FirstEngineMetaMaterialId), meta("fixture", meta_id), meta("collision", meta_id),
    meta("fixture", meta_id + 1), meta("next", meta_id + 2)});
  if (!fixture.check(bool(loaded), loaded ? "load catalog after logging invalid metamaterials" : loaded.error().what())) return;
  auto const& catalog = **loaded;
  constexpr std::string_view skipped[] = {"negative", "wrapped", "real", "engine", "collision", "fixture"};
  for (auto name : skipped) {
    auto qualified = "metamaterial:" + std::string(name);
    fixture.check(std::ranges::any_of(sink->messages, [&](std::string const& message) { return message.contains(qualified); }),
                  "log skipped " + qualified);
    if (name != "fixture")
      fixture.check(!catalog.is_material_name(String(qualified)), "skip " + qualified);
  }
  fixture.check(catalog.is_meta_material_name("metamaterial:fixture") &&
                  catalog.material_id("metamaterial:fixture") == meta_id &&
                  !catalog.is_valid_material_id(meta_id + 1) &&
                  catalog.material_id("metamaterial:next") == meta_id + 2,
                "retain first registration and continue loading after duplicates");
  fixture.check(catalog.material_collision_kind(meta_id) == CollisionKind::Platform &&
                  catalog.blocks_liquid_flow(meta_id) == false &&
                  catalog.material_collision_kind(meta_id + 1) == CollisionKind::Block &&
                  !catalog.blocks_liquid_flow(meta_id + 1),
                "registered meta flags differ from an unregistered meta-range ID");
});

const GameTestRegistration material_catalog_lookup("MaterialDatabaseTest", "PreservesBorrowedAssetsAndLookupSemantics",
    [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!fixture.check(bool(directory), directory ? "create isolated lookup fixtures" : directory.error().what())) return;
  auto cleanup = make_scope_exit([&] { fixture.check(bool(File::remove_all(*directory)), "remove lookup fixtures"); });
  Logger logger;
  constexpr MaterialId material_id = 1;
  constexpr MaterialId next_material_id = 2;
  constexpr ModId mod_id = 1;
  constexpr ModId next_mod_id = 2;

  auto material = material_fixture(material_id);
  material["description"] = "material fallback";
  material["shortdescription"] = "material short";
  material["humanDescription"] = "human material";
  material["avianDescription"] = false;
  material["soil"] = true;
  material["foregroundOnly"] = true;
  material["tillableMod"] = next_mod_id;
  material["renderTemplate"] = "probe.template";
  material["renderParameters"] = JsonObject{{"foregroundLightTransparent", true}};
  material["requiredHarvestLevel"] = 7;
  material["harvestLevel"] = "not read for a material";
  material["liquidInteractions"] = JsonArray{
    JsonObject{{"liquidId", std::numeric_limits<LiquidId>::max()}, {"transformMaterialId", next_material_id},
               {"consumeLiquid", 0.25}, {"topOnly", true}},
    JsonObject{{"liquidId", 0}, {"transformMaterialId", NullMaterialId}},
    JsonObject{{"liquidId", 1}, {"transformMaterialId", FirstMetaMaterialId}}};
  auto next_material = material_fixture(next_material_id, "second");
  next_material["tillableMod"] = NoModId;
  next_material["liquidInteractions"] = JsonArray{
    JsonObject{{"liquidId", std::numeric_limits<LiquidId>::max()}, {"transformMaterialId", material_id}},
    JsonObject{{"liquidId", 1}}};

  auto mod = mod_fixture(mod_id);
  mod["description"] = "mod fallback";
  mod["shortdescription"] = "mod short";
  mod["humanDescription"] = "human mod";
  mod["avianDescription"] = false;
  mod["tilled"] = true;
  mod["breaksWithTile"] = true;
  mod["renderTemplate"] = "probe.template";
  mod["renderParameters"] = JsonObject{};
  mod["harvestLevel"] = 9;
  mod["requiredHarvestLevel"] = "not read for a mod";
  mod["liquidInteractions"] = JsonArray{
    JsonObject{{"liquidId", std::numeric_limits<LiquidId>::max()}, {"transformModId", next_mod_id},
               {"consumeLiquid", 0.5}, {"topOnly", true}},
    JsonObject{{"liquidId", 0}, {"transformModId", NoModId}},
    JsonObject{{"liquidId", 1}, {"transformModId", BiomeModId}}};
  auto next_mod = mod_fixture(next_mod_id, "second");
  next_mod["shortdescription"] = Json{};
  next_mod["harvestLevel"] = std::numeric_limits<unsigned>::max();
  next_mod["liquidInteractions"] = JsonArray{
    JsonObject{{"liquidId", std::numeric_limits<LiquidId>::max()}, {"transformModId", mod_id}},
    JsonObject{{"liquidId", 1}}};

  auto render_template = [](unsigned z_level) -> Json {
    return JsonObject{{"zLevel", z_level}, {"representativePiece", ""}, {"rules", JsonObject{}},
                      {"pieces", JsonObject{}}, {"matches", JsonArray{JsonArray{"main", JsonArray{}}}}};
  };
  auto loaded = load_material_fixtures(fixture, *directory, logger,
    {{"nested/a.material", std::move(material)}, {"nested/a.matmod", std::move(mod)},
     {"nested/z.material", std::move(next_material)}, {"nested/z.matmod", std::move(next_mod)},
     {"probe.template", render_template(3)}, {"nested/probe.template", render_template(5)}});
  if (!fixture.check(bool(loaded), loaded ? "load lookup fixtures" : loaded.error().what())) return;
  auto const& catalog = **loaded;

  fixture.check(catalog.material_id("fixture") == material_id && catalog.mod_id("fixture") == mod_id &&
                  catalog.is_material_name("fixture") && !catalog.is_meta_material_name("fixture") &&
                  catalog.is_valid_material_id(material_id) && catalog.is_valid_mod_id(mod_id),
                "register real materials and mods with independent names");
  fixture.check(catalog.is_material_name("metamaterial:empty") && catalog.is_meta_material_name("metamaterial:empty") &&
                  catalog.is_valid_material_id(EmptyMaterialId) && catalog.is_valid_mod_id(NoModId) &&
                  catalog.is_mod_name("metamod:none") && catalog.mod_id("metamod:none") == NoModId,
                "built-in meta names and IDs are registered too");
  fixture.check(catalog.is_soil(material_id) == true && catalog.blocks_liquid_flow(material_id) == true &&
                  catalog.supports_mod(material_id, mod_id) == true && catalog.is_tilled_mod(mod_id) == true &&
                  catalog.mod_breaks_with_tile(mod_id) == true &&
                  catalog.can_place_in_layer(material_id, TileLayer::Foreground) == true &&
                  catalog.can_place_in_layer(material_id, TileLayer::Background) == false,
                "real records retain configured flags and block defaults");
  fixture.check(catalog.is_soil(StructureMaterialId) == false && catalog.blocks_liquid_flow(StructureMaterialId) == true &&
                  catalog.blocks_liquid_flow(EmptyMaterialId) == false && catalog.is_tilled_mod(NoModId) == false &&
                  catalog.mod_breaks_with_tile(NoModId) == false &&
                  catalog.supports_mod(StructureMaterialId, mod_id) == false &&
                  catalog.supports_mod(StructureMaterialId, NoModId) == true,
                "meta properties and explicit NoMod support preserve their defaults");

  constexpr auto max_liquid = std::numeric_limits<LiquidId>::max();
  auto material_forward = catalog.liquid_material_interaction(max_liquid, material_id);
  auto material_back = catalog.liquid_material_interaction(max_liquid, next_material_id);
  auto mod_forward = catalog.liquid_mod_interaction(max_liquid, mod_id);
  auto mod_back = catalog.liquid_mod_interaction(max_liquid, next_mod_id);
  // Mutual references exercise a forward target regardless of scan order.
  fixture.check(material_forward && material_forward->transform_to == next_material_id &&
                  material_forward->consume_liquid == 0.25f && material_forward->top_only &&
                  material_back && material_back->transform_to == material_id &&
                  mod_forward && mod_forward->transform_to == next_mod_id &&
                  mod_forward->consume_liquid == 0.5f && mod_forward->top_only &&
                  mod_back && mod_back->transform_to == mod_id,
                "preserve forward transforms and the largest liquid ID");
  auto material_sentinel = catalog.liquid_material_interaction(0, material_id);
  auto material_meta = catalog.liquid_material_interaction(1, material_id);
  auto material_default = catalog.liquid_material_interaction(1, next_material_id);
  fixture.check(material_sentinel && material_sentinel->transform_to == NullMaterialId &&
                  material_meta && material_meta->transform_to == FirstMetaMaterialId &&
                  material_default && material_default->transform_to == NullMaterialId &&
                  !material_default->top_only && material_default->consume_liquid == 0 &&
                  !catalog.is_valid_material_id(FirstMetaMaterialId),
                "stored material transforms may reference sentinels or unregistered meta-range IDs");
  auto mod_sentinel = catalog.liquid_mod_interaction(0, mod_id);
  auto mod_meta = catalog.liquid_mod_interaction(1, mod_id);
  auto mod_default = catalog.liquid_mod_interaction(1, next_mod_id);
  fixture.check(mod_sentinel && mod_sentinel->transform_to == NoModId &&
                  mod_meta && mod_meta->transform_to == BiomeModId &&
                  mod_default && mod_default->transform_to == NoModId &&
                  !mod_default->top_only && mod_default->consume_liquid == 0 &&
                  catalog.tilled_mod_for(material_id) == next_mod_id && catalog.tilled_mod_for(next_material_id) == NoModId,
                "preserve sentinel and forward mod IDs in reactions and tillableMod");

  fixture.check(catalog.material_description(material_id) == String("material fallback") &&
                  catalog.material_description(material_id, "floran") == String("material fallback") &&
                  catalog.material_description(material_id, "human") == String("human material") &&
                  catalog.material_short_description(material_id) == String("material short"),
                "material descriptions honor fallback, species and short values");
  fixture.check(catalog.mod_description(mod_id) == String("mod fallback") &&
                  catalog.mod_description(mod_id, "floran") == String("mod fallback") &&
                  catalog.mod_description(mod_id, "human") == String("human mod") &&
                  catalog.mod_short_description(mod_id) == String("mod short"),
                "mod descriptions honor fallback, species and short values");
  auto bad_material_description = catalog.material_description(material_id, "avian");
  auto bad_mod_description = catalog.mod_description(mod_id, "avian");
  fixture.check(!bad_material_description && bad_material_description.error().is<JsonException>() &&
                  !bad_mod_description && bad_mod_description.error().is<JsonException>() &&
                  catalog.material_description(next_material_id) == String{} &&
                  catalog.mod_description(next_mod_id) == String{} &&
                  catalog.material_short_description(next_material_id) == String{} &&
                  catalog.mod_short_description(next_mod_id) == String{},
                "malformed species values fail on lookup, while omitted descriptions return empty strings");

  auto material_asset = catalog.material_asset(material_id);
  auto mod_asset = catalog.mod_asset(mod_id);
  fixture.check(material_asset && *material_asset && (**material_asset).get().path == String("/nested/a.material") &&
                  (**material_asset).get().config.get_string("materialName") == String("fixture") &&
                  mod_asset && *mod_asset && (**mod_asset).get().path == String("/nested/a.matmod") &&
                  (**mod_asset).get().config.get_string("modName") == String("fixture"),
                "each real asset supplies its original path and config together");
  auto meta_material_asset = catalog.material_asset(FirstMetaMaterialId);
  auto meta_mod_asset = catalog.mod_asset(FirstMetaMod);
  fixture.check(meta_material_asset && !*meta_material_asset && meta_mod_asset && !*meta_mod_asset &&
                  !catalog.material_asset(0) && !catalog.mod_asset(0),
                "reserved-range asset lookups are empty, but real holes are errors");

  auto material_render = catalog.material_render_profile(material_id);
  auto mod_render = catalog.mod_render_profile(mod_id);
  static_assert(std::same_as<decltype(material_render), std::optional<std::reference_wrapper<MaterialRenderProfile const>>>);
  static_assert(std::same_as<decltype(mod_render), decltype(material_render)>);
  fixture.check(material_render && material_render->get().zLevel == 5 && mod_render && mod_render->get().zLevel == 3 &&
                  catalog.foreground_light_transparent(material_id) == true &&
                  catalog.background_light_transparent(material_id) == false,
                "material resolves its template relative to the asset, while mod resolves from the asset root");
  fixture.check(!catalog.material_render_profile(0) && !catalog.mod_render_profile(0) &&
                  !catalog.material_render_profile(next_material_id) && !catalog.mod_render_profile(next_mod_id) &&
                  !catalog.material_render_profile(StructureMaterialId) && !catalog.mod_render_profile(NoModId) &&
                  catalog.foreground_light_transparent(next_material_id) == true &&
                  catalog.foreground_light_transparent(StructureMaterialId) == false,
                "render profile absence covers holes, unrendered real records, and meta IDs");

  auto material_damage = catalog.material_damage_parameters(material_id);
  auto mod_damage = catalog.mod_damage_parameters(mod_id);
  static_assert(std::same_as<decltype(material_damage),
                             std::expected<std::reference_wrapper<TileDamageParameters const>, StarException>>);
  static_assert(std::same_as<decltype(mod_damage), decltype(material_damage)>);
  fixture.check(material_damage && material_damage->get().required_harvest_level() == 7 &&
                  mod_damage && mod_damage->get().required_harvest_level() == 9,
                "material and mod read different harvest override keys");
  auto real_default = catalog.material_damage_parameters(next_material_id);
  auto maximum_harvest = catalog.mod_damage_parameters(next_mod_id);
  fixture.check(maximum_harvest && maximum_harvest->get().required_harvest_level() == std::numeric_limits<unsigned>::max(),
                "harvest level accepts the largest representable value");
  auto meta_material_damage = catalog.material_damage_parameters(FirstMetaMaterialId);
  auto meta_mod_damage = catalog.mod_damage_parameters(NoModId);
  fixture.check(real_default && real_default->get().required_harvest_level() == 1 && real_default->get().total_health() == 1 &&
                  meta_material_damage && meta_material_damage->get().required_harvest_level() == 0 &&
                  meta_material_damage->get().total_health() == 0 &&
                  meta_mod_damage && meta_mod_damage->get().required_harvest_level() == 0 &&
                  meta_mod_damage->get().total_health() == 0 &&
                  !catalog.material_damage_parameters(0) && !catalog.mod_damage_parameters(0),
                "real holes fail, while real config defaults and meta damage defaults differ");
});

}// namespace
