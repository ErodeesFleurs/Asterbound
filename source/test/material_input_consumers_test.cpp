import std;
import star.algorithm;
import star.buffer;
import star.dungeon_generator;
import star.file;
import star.format;
import star.game_types;
import star.game_test;
import star.image;
import star.json;
import star.material_types;
import star.root;
import star.string;
import star.tileset_database;
import star.vector;
import star.world_structure;

namespace {
using namespace Star;

bool check(bool passed, std::string_view message, std::source_location at = std::source_location::current()) {
  return GameTest::check(passed, message, at);
}

template <typename T>
bool check_result(std::expected<T, StarException> const& result, std::string_view message,
    std::source_location at = std::source_location::current()) {
  return check(result.has_value(), result ? message : std::string_view(result.error().what()), at);
}

Json structure_store(Json background, Json foreground) {
  return JsonObject{{"region", JsonArray{0, 0, 2, 1}}, {"anchorPosition", JsonArray{0, 0}}, {"config", JsonObject{}},
      {"backgroundOverlays", JsonArray{}}, {"foregroundOverlays", JsonArray{}},
      {"backgroundBlocks", JsonArray{std::move(background)}}, {"foregroundBlocks", JsonArray{std::move(foreground)}},
      {"objects", JsonArray{}}, {"flaggedBlocks", JsonObject{}}};
}

const GameTestRegistration stored_material_values("WorldStructureMaterialInput", "StoredFieldsCheckWidthAndPreserveLayout", [](GameTest&) {
  Json legacy = JsonObject{{"position", JsonArray{0, 0}}, {"materialId", structure_material_id}, {"residual", false}};
  Json explicit_defaults = JsonObject{{"position", JsonArray{0, 0}}, {"materialId", structure_material_id}, {"residual", false},
      {"materialColor", 0}, {"materialHue", 0}, {"modId", no_mod_id}};
  Json extended = JsonObject{{"position", JsonArray{1, 0}}, {"materialId", 65499}, {"residual", true},
      {"materialColor", 255}, {"materialHue", 255.0}, {"modId", biome_mod_id}};
  auto restored = WorldStructure::create(structure_store(legacy, extended));
  if (!check_result(restored, "read a legacy save"))
    return;
  auto stored = restored->store();
  check(stored && *stored == structure_store(explicit_defaults, extended), "legacy defaults, forward ID and byte extrema round-trip");

  for (std::string_view field : {"materialId", "modId", "materialColor", "materialHue"}) {
    int overflow = field.ends_with("Id") ? 65536 : 256;
    for (Json const& value : std::array<Json, 5>{-1, overflow, 0.5, std::int64_t{1} << 32, "0"}) {
      auto invalid = extended.set(field, value);
      if (!check(invalid.has_value(), "prepare invalid stored field"))
        return;
      check(!WorldStructure::create(structure_store(legacy, *invalid)), Star::format("reject invalid stored {}", field));
    }
    auto nullable = explicit_defaults.set(field, Json());
    if (!check(nullable.has_value(), "prepare explicit null"))
      return;
    auto parsed = WorldStructure::create(structure_store(*nullable, extended));
    if (field == "materialId") {
      check(!parsed && parsed.error().is<JsonException>(), "required material ID rejects explicit null");
    } else if (check(parsed.has_value(), "optional fields accept explicit null")) {
      auto canonical = parsed->store();
      check(canonical && *canonical == structure_store(explicit_defaults, extended), "null uses the legacy default");
    }
  }
  Json missing = JsonObject{{"position", JsonArray{0, 0}}, {"residual", false}};
  auto no_id = WorldStructure::create(structure_store(missing, extended));
  check(!no_id && no_id.error().is<JsonException>(), "required material ID rejects missing field");
});

const GameTestRegistration authored_block_keys("WorldStructureMaterialInput", "BlockImageKeyChecksChannelsAndMaterialBytes", [](GameTest& fixture) {
  auto directory = File::temporary_directory(fixture.root().random());
  if (!check(directory.has_value(), "create block key fixture directory"))
    return;
  auto cleanup = make_scope_exit([&] { check(bool(File::remove_all(*directory)), "remove block key fixture directory"); });
  auto overlay = File::relative_to(*directory, "assets");
  if (!check(bool(File::create_directory(overlay)), "create loose asset source"))
    return;
  auto write = [&](String const& path, String const& text) { return File::write_file(text, File::relative_to(overlay, path)); };
  if (!check(write("preload.config", "[]") && write("metamaterials.config", "[]") &&
          write("client.config", R"({"defaultFootstepSound":"/unused.ogg"})"), "write minimal catalog settings"))
    return;
  auto buffer = std::make_shared<Buffer>();
  if (!check(Image::filled({1, 1}, {13, 17, 19, 255}).write_png(fixture.root().logger(), buffer) &&
          File::write_file(buffer->take_data(), File::relative_to(overlay, "blocks.png")), "write block key image"))
    return;
  auto write_structure = [&](String const& file, Json const& key) -> std::expected<void, StarException> {
    auto text = Json(JsonObject{{"blockImage", "blocks.png"}, {"blockKey", JsonArray{key}}}).repr();
    if (!text)
      return std::unexpected(std::move(text.error()));
    return write(file, *text);
  };
  Json valid = JsonObject{{"foregroundBlock", true}, {"foregroundMat", "metamaterial:biome"}, {"foregroundMod", "metamod:biome"},
      {"foregroundMatColor", 255}, {"foregroundMatHue", 225}, {"backgroundBlock", true}, {"backgroundMatColor", Json()}};
  std::array<Json, 3> valid_colors{JsonArray{13.0, 17, 19}, JsonArray{13, 17, 19, 255}, "#0d1113"};
  for (std::size_t i = 0; i < valid_colors.size(); ++i) {
    auto key = valid.set("value", valid_colors[i]);
    if (!check(key && write_structure(Star::format("valid-{}.structure", i), *key), "write valid block key"))
      return;
  }
  auto base_key = valid.set("value", valid_colors[0]);
  if (!check(base_key.has_value(), "prepare block key"))
    return;
  std::array<std::pair<std::string_view, Json>, 10> invalid_values{{
      {"foregroundMatColor", 256}, {"backgroundMatColor", -1}, {"foregroundMatHue", 0.5},
      {"backgroundMatHue", std::int64_t{1} << 32}, {"value", JsonArray{269, 17, 19}},
      {"value", JsonArray{13, 17, 19, -1}}, {"value", JsonArray{13, 17, "19"}}, {"value", JsonArray{13.5, 17, 19}},
      {"value", JsonArray{13, 17}}, {"foregroundMat", 1}}};
  for (std::size_t i = 0; i < invalid_values.size(); ++i) {
    auto const& [field, value] = invalid_values[i];
    auto key = base_key->set(field, value);
    if (!check(key && write_structure(Star::format("invalid-{}.structure", i), *key), "write invalid block key"))
      return;
  }
  auto settings = fixture.rootSettings();
  settings.assetDirectories.clear();
  settings.assetSources = {overlay};
  settings.includeUGC = false;
  settings.storageDirectory = File::relative_to(*directory, "storage");
  settings.runtimeConfigFile = {};
  settings.logDirectory = {};
  settings.logFile = {};
  auto root = Root::create(std::move(settings));
  if (!check_result(root, "create dedicated fixture root"))
    return;
  for (std::size_t i = 0; i < valid_colors.size(); ++i) {
    auto structure = WorldStructure::create(**root, Star::format("/valid-{}.structure", i));
    if (!check_result(structure, "load a valid structure"))
      return;
    auto const& foreground = structure->foregroundBlocks();
    auto const& background = structure->backgroundBlocks();
    if (!check(foreground.size() == 1 && background.size() == 1, "valid key matches the image pixel in both layers"))
      return;
    auto const& block = foreground[0];
    check(block.materialId == biome_material_id && block.materialMod == biome_mod_id &&
            block.materialColor == 255 && block.materialHue == 225, "registered names and raw byte values reach the structure block");
    check(background[0].materialId == structure_material_id && background[0].materialColor == 0,
        "omitted name and null variant use their legacy defaults");
  }
  for (std::size_t i = 0; i < invalid_values.size(); ++i) {
    auto invalid = WorldStructure::create(**root, Star::format("/invalid-{}.structure", i));
    check(!invalid, Star::format("reject out-of-domain block key {} (case {})", invalid_values[i].first, i));
  }
});

const GameTestRegistration tiled_material_settings("TilesetMaterialInput", "TiledPropertiesCheckIntegerTextAndFiniteDegrees", [](GameTest& fixture) {
  auto create = [&](std::string_view brush, Json settings) {
    return Tiled::Tileset::create(fixture.root(), JsonObject{{"tilecount", 1}, {"properties", std::move(settings)},
        {"tileproperties", JsonObject{{"0", JsonObject{{brush, "forward-reference"}}}}}});
  };
  for (auto brush : {"material", "front", "back"}) {
    auto valid = create(brush, JsonObject{{"colorVariant", "255"}, {"hueshift", "-45"}, {"modhueshift", "720"}, {"mod", "forward-mod"}});
    if (check_result(valid, "accept raw variant byte, signed degree and forward references")) {
      auto foreground = (*valid)->getTile(0, TileLayer::Foreground);
      auto background = (*valid)->getTile(0, TileLayer::Background);
      check(foreground && *foreground && (*foreground)->brushes.size() == 1 &&
              background && *background && (*background)->brushes.size() == 2,
          "foreground material and background clear/material retain the legacy tile layout");
    }
    check(bool(create(brush, JsonObject{})), "accept omitted Tiled material settings");
    check(bool(create(brush, JsonObject{{"colorVariant", Json()}, {"hueshift", Json()}, {"modhueshift", Json()}, {"mod", Json()}})),
        "null Tiled settings remain optional");
    for (auto const& value : std::array<Json, 5>{"-1", "256", "18446744073709551616", "0.5", 1})
      check(!create(brush, JsonObject{{"colorVariant", value}}), "reject invalid Tiled color variant");
    for (std::string_view field : {"hueshift", "modhueshift"}) {
      for (auto const& value : std::array<Json, 4>{"nan", "inf", "-inf", 45})
        check(!create(brush, JsonObject{{field, value}}), "reject nonfinite or mistyped Tiled degree input");
    }
    check(!create(brush, JsonObject{{"mod", 1}}), "mod remains a name, not a numeric material field");
  }
});
}
