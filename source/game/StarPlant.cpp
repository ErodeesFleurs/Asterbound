import star.json_extra;
import star.json;
import star.data_stream_extra;
import star.id_map;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.casting;
import star.strong_typedef;
import star.rpc_promise;
import star.image;
import star.audio;
import star.map;
import star.set;
import star.vector;
import star.bi_map;
import star.color;
import star.string;
import star.game_types;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;
import std;
import star.format;

import star.inspectable_entity;
import star.plant;
import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.item_descriptor;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.lua_root;
import star.force_regions;
import star.world;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.physics_entity;
import star.movement_controller;
import star.mobile_entity;
import star.mixer;
import star.drawable;
import star.entity_rendering_types;
import star.animation;
import star.particle;

import star.light_source;
import star.entity_rendering;

import star.particle_database;
import star.object_database;
import star.image_metadata_database;
import star.plant_database;


import star.plant_drop;

import star.data_stream_devices;

import star.net_compatibility;
import star.random;
import star.byte_array;
import star.math_common;
import star.algorithm;
namespace Star {

float const Plant::PlantScanThreshold = 0.1f;

std::expected<EnumMap<Plant::RotationType>, StarException> const Plant::RotationTypeNames = EnumMap<Plant::RotationType>::create({
  {Plant::RotationType::DontRotate, "dontRotate"},
  {Plant::RotationType::RotateBranch, "rotateBranch"},
  {Plant::RotationType::RotateLeaves, "rotateLeaves"},
  {Plant::RotationType::RotateCrownBranch, "rotateCrownBranch"},
  {Plant::RotationType::RotateCrownLeaves, "rotateCrownLeaves"},
});

Plant::PlantPiece::PlantPiece() {
  image = "";
  imagePath = AssetPath();
  offset = {};
  segmentIdx = 0;
  structuralSegment = 0;
  kind = PlantPieceKind::None;
  zLevel = 0;
  rotationType = RotationType::DontRotate;
  rotationOffset = 0;
  spaces = {};
  flip = false;
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& rootContext, TreeVariant const& config, std::uint64_t seed) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto damageStatus = EntityTileDamageStatus::create();
  if (!damageStatus) return std::unexpected(std::move(damageStatus.error()));
  entity->m_tileDamageStatus = std::move(*damageStatus);
  if (auto result = entity->initialize(config, seed); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(TreeVariant const& config, std::uint64_t seed) {
  m_broken = false;
  m_tilePosition = Vec2I();
  m_windTime = 0.0f;
  m_windLevel = 0.0f;
  m_ceiling = config.ceiling;
  m_piecesScanned = false;
  m_fallsWhenDead = true;
  m_piecesUpdated = true;
  m_tileDamageEvent = false;

  m_stemDropConfig = config.stemDropConfig;
  m_foliageDropConfig = config.foliageDropConfig;
  if (m_stemDropConfig.is_null())
    m_stemDropConfig = JsonObject();
  if (m_foliageDropConfig.is_null())
    m_foliageDropConfig = JsonObject();

  auto result429 = m_stemDropConfig.set("hueshift", config.stemHueShift);
if (!result429) return std::unexpected(std::move(result429.error()));
m_stemDropConfig = std::move(*result429);
  auto result430 = m_foliageDropConfig.set("hueshift", config.foliageHueShift);
if (!result430) return std::unexpected(std::move(result430.error()));
m_foliageDropConfig = std::move(*result430);

  JsonObject saplingDropConfig;
  saplingDropConfig["stemName"] = config.stemName;
  saplingDropConfig["stemHueShift"] = config.stemHueShift;
  if (!m_foliageDropConfig.is_null()) {
    saplingDropConfig["foliageName"] = config.foliageName;
    saplingDropConfig["foliageHueShift"] = config.foliageHueShift;
  }
  m_saplingDropConfig = saplingDropConfig;

  RandomSource rnd(seed);

  float xOffset = 0;
  float yOffset = 0;

  float roffset = this->root().random().uniform_float() * 0.5f;

  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tileDamageParameters = config.tileDamageParameters;

  int segment = 0;

  auto result431 = this->root().assets();
if (!result431) return std::unexpected(std::move(result431.error()));
auto assets = std::move(*result431);

  // base
  {
    auto result432 = config.stemSettings.get("base");
if (!result432) return std::unexpected(std::move(result432.error()));
auto result433 = std::move(*result432).to_object();
if (!result433) return std::unexpected(std::move(result433.error()));
JsonObject bases = std::move(*result433);
    auto result434 = rnd.uniform_int(bases.size() - 1);
if (!result434) return std::unexpected(std::move(result434.error()));
String baseKey = bases.keys()[*result434];
    auto result435 = bases[baseKey].to_object();
if (!result435) return std::unexpected(std::move(result435.error()));
JsonObject baseSettings = std::move(*result435);

    auto result436 = baseSettings["attachment"].to_object();
if (!result436) return std::unexpected(std::move(result436.error()));
JsonObject attachmentSettings = std::move(*result436);

    auto result437 = attachmentSettings.get("bx");
if (!result437) return std::unexpected(std::move(result437.error()));
auto result438 = result437->get().to_double();
if (!result438) return std::unexpected(std::move(result438.error()));
xOffset += std::move(*result438) / tile_pixels;
    auto result439 = attachmentSettings.get("by");
if (!result439) return std::unexpected(std::move(result439.error()));
auto result440 = result439->get().to_double();
if (!result440) return std::unexpected(std::move(result440.error()));
yOffset += std::move(*result440) / tile_pixels;

    auto result441 = baseSettings.get("image");
if (!result441) return std::unexpected(std::move(result441.error()));
auto result442 = result441->get().to_string();
if (!result442) return std::unexpected(std::move(result442.error()));
String baseFile = AssetPath::relative_to(config.stemDirectory, std::move(*result442));
auto result443 = assets->image(baseFile);
if (!result443) return std::unexpected(std::move(result443.error()));
float baseImageHeight = std::move(*result443)->height();
    if (config.ceiling)
      yOffset = 1.0 - baseImageHeight / tile_pixels;

    {
      PlantPiece piece;
      piece.image = Star::format("{}?hueshift={}", baseFile, config.stemHueShift);
      piece.offset = Vec2F(xOffset, yOffset);
      piece.segmentIdx = segment;
      piece.structuralSegment = true;
      piece.kind = PlantPieceKind::Stem;
      piece.zLevel = 0.0f;
      piece.rotationType = DontRotate;
      piece.rotationOffset = this->root().random().uniform_float() + roffset;
      m_pieces.push_back(piece);
    }

    // base leaves
    auto result444 = config.foliageSettings.get_object("baseLeaves", {});
if (!result444) return std::unexpected(std::move(result444.error()));
JsonObject baseLeaves = std::move(*result444);
    if (baseLeaves.contains(baseKey)) {
      auto result445 = baseLeaves.get(baseKey);
if (!result445) return std::unexpected(std::move(result445.error()));
auto result446 = result445->get().to_object();
if (!result446) return std::unexpected(std::move(result446.error()));
JsonObject baseLeavesSettings = std::move(*result446);
      auto result447 = baseLeavesSettings["attachment"].to_object();
if (!result447) return std::unexpected(std::move(result447.error()));
JsonObject attachmentSettings = std::move(*result447);

      auto result448 = attachmentSettings.get("bx");
if (!result448) return std::unexpected(std::move(result448.error()));
auto result449 = result448->get().to_double();
if (!result449) return std::unexpected(std::move(result449.error()));
float xOf = xOffset + std::move(*result449) / tile_pixels;
      auto result450 = attachmentSettings.get("by");
if (!result450) return std::unexpected(std::move(result450.error()));
auto result451 = result450->get().to_double();
if (!result451) return std::unexpected(std::move(result451.error()));
float yOf = yOffset + std::move(*result451) / tile_pixels;

      bool condition454 = static_cast<bool>(baseLeavesSettings.contains("image"));
if (condition454) {
auto result452 = baseLeavesSettings.get("image");
if (!result452) return std::unexpected(std::move(result452.error()));
auto result453 = result452->get().to_string();
if (!result453) return std::unexpected(std::move(result453.error()));
condition454 = static_cast<bool>(!std::move(*result453).empty());
}
if (condition454) {
        auto result455 = baseLeavesSettings.get("image");
if (!result455) return std::unexpected(std::move(result455.error()));
auto result456 = result455->get().to_string();
if (!result456) return std::unexpected(std::move(result456.error()));
String baseLeavesFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result456));

PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", baseLeavesFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = 3.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = this->root().random().uniform_float() + roffset;
m_pieces.push_back(piece);
      }

      bool condition459 = static_cast<bool>(baseLeavesSettings.contains("backimage"));
if (condition459) {
auto result457 = baseLeavesSettings.get("backimage");
if (!result457) return std::unexpected(std::move(result457.error()));
auto result458 = result457->get().to_string();
if (!result458) return std::unexpected(std::move(result458.error()));
condition459 = static_cast<bool>(!std::move(*result458).empty());
}
if (condition459) {
        auto result460 = baseLeavesSettings.get("backimage");
if (!result460) return std::unexpected(std::move(result460.error()));
auto result461 = result460->get().to_string();
if (!result461) return std::unexpected(std::move(result461.error()));
String baseLeavesBackFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result461));
PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", baseLeavesBackFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = -1.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = this->root().random().uniform_float() + roffset;
m_pieces.push_back(piece);
      }
    }

    auto result462 = attachmentSettings.get("x");
if (!result462) return std::unexpected(std::move(result462.error()));
auto result463 = result462->get().to_double();
if (!result463) return std::unexpected(std::move(result463.error()));
xOffset += std::move(*result463) / tile_pixels;
    auto result464 = attachmentSettings.get("y");
if (!result464) return std::unexpected(std::move(result464.error()));
auto result465 = result464->get().to_double();
if (!result465) return std::unexpected(std::move(result465.error()));
yOffset += std::move(*result465) / tile_pixels; // trunk height

    segment++;
  }

  float branchYOffset = yOffset;

  // trunk
  {
    auto result466 = config.stemSettings.get("middle");
if (!result466) return std::unexpected(std::move(result466.error()));
auto result467 = std::move(*result466).to_object();
if (!result467) return std::unexpected(std::move(result467.error()));
JsonObject middles = std::move(*result467);

    auto result468 = config.stemSettings.get_int("middleMinSize", 1);
if (!result468) return std::unexpected(std::move(result468.error()));
auto result469 = config.stemSettings.get_int("middleMaxSize", 6);
if (!result469) return std::unexpected(std::move(result469.error()));
auto result470 = config.stemSettings.get_int("middleMinSize", 1);
if (!result470) return std::unexpected(std::move(result470.error()));
auto result471 = rnd.uniform_int(std::move(*result469) - std::move(*result470));
if (!result471) return std::unexpected(std::move(result471.error()));
int middleHeight = std::move(*result468) + *result471;

    auto result472 = config.stemSettings.contains("branch");
if (!result472) return std::unexpected(std::move(result472.error()));
bool hasBranches = std::move(*result472);
    JsonObject branches;
    if (hasBranches) {
      auto result473 = config.stemSettings.get("branch");
if (!result473) return std::unexpected(std::move(result473.error()));
auto result474 = std::move(*result473).to_object();
if (!result474) return std::unexpected(std::move(result474.error()));
branches = std::move(*result474);
      if (branches.size() == 0)
        hasBranches = false;
    }

    for (int i = 0; i < middleHeight; i++) {
      auto result475 = rnd.uniform_int(middles.size() - 1);
if (!result475) return std::unexpected(std::move(result475.error()));
String middleKey = middles.keys()[*result475];
      auto result476 = middles[middleKey].to_object();
if (!result476) return std::unexpected(std::move(result476.error()));
JsonObject middleSettings = std::move(*result476);
      auto result477 = middleSettings["attachment"].to_object();
if (!result477) return std::unexpected(std::move(result477.error()));
JsonObject attachmentSettings = std::move(*result477);

      auto result478 = attachmentSettings.get("bx");
if (!result478) return std::unexpected(std::move(result478.error()));
auto result479 = result478->get().to_double();
if (!result479) return std::unexpected(std::move(result479.error()));
xOffset += std::move(*result479) / tile_pixels;
      auto result480 = attachmentSettings.get("by");
if (!result480) return std::unexpected(std::move(result480.error()));
auto result481 = result480->get().to_double();
if (!result481) return std::unexpected(std::move(result481.error()));
yOffset += std::move(*result481) / tile_pixels;

      auto result482 = middleSettings.get("image");
if (!result482) return std::unexpected(std::move(result482.error()));
auto result483 = result482->get().to_string();
if (!result483) return std::unexpected(std::move(result483.error()));
String middleFile = AssetPath::relative_to(config.stemDirectory, std::move(*result483));

{
  PlantPiece piece;
  piece.image = Star::format("{}?hueshift={}", middleFile, config.stemHueShift);
  piece.offset = Vec2F(xOffset, yOffset);
  piece.segmentIdx = segment;
  piece.structuralSegment = true;
  piece.kind = PlantPieceKind::Stem;
  piece.zLevel = 1.0f;
  piece.rotationType = DontRotate;
  piece.rotationOffset = this->root().random().uniform_float() + roffset;
  m_pieces.push_back(piece);
}

      // trunk leaves
      auto result484 = config.foliageSettings.get_object("trunkLeaves", {});
if (!result484) return std::unexpected(std::move(result484.error()));
JsonObject trunkLeaves = std::move(*result484);
      if (trunkLeaves.contains(middleKey)) {
        auto result485 = trunkLeaves.get(middleKey);
if (!result485) return std::unexpected(std::move(result485.error()));
auto result486 = result485->get().to_object();
if (!result486) return std::unexpected(std::move(result486.error()));
JsonObject trunkLeavesSettings = std::move(*result486);
        auto result487 = trunkLeavesSettings["attachment"].to_object();
if (!result487) return std::unexpected(std::move(result487.error()));
JsonObject attachmentSettings = std::move(*result487);

        auto result488 = attachmentSettings.get("bx");
if (!result488) return std::unexpected(std::move(result488.error()));
auto result489 = result488->get().to_double();
if (!result489) return std::unexpected(std::move(result489.error()));
float xOf = xOffset + std::move(*result489) / tile_pixels;
        auto result490 = attachmentSettings.get("by");
if (!result490) return std::unexpected(std::move(result490.error()));
auto result491 = result490->get().to_double();
if (!result491) return std::unexpected(std::move(result491.error()));
float yOf = yOffset + std::move(*result491) / tile_pixels;

        bool condition494 = static_cast<bool>(trunkLeavesSettings.contains("image"));
if (condition494) {
auto result492 = trunkLeavesSettings.get("image");
if (!result492) return std::unexpected(std::move(result492.error()));
auto result493 = result492->get().to_string();
if (!result493) return std::unexpected(std::move(result493.error()));
condition494 = static_cast<bool>(!std::move(*result493).empty());
}
if (condition494) {
          auto result495 = trunkLeavesSettings.get("image");
if (!result495) return std::unexpected(std::move(result495.error()));
auto result496 = result495->get().to_string();
if (!result496) return std::unexpected(std::move(result496.error()));
String trunkLeavesFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result496));
PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", trunkLeavesFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = 3.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = this->root().random().uniform_float() + roffset;
m_pieces.push_back(piece);
        }

        bool condition499 = static_cast<bool>(trunkLeavesSettings.contains("backimage"));
if (condition499) {
auto result497 = trunkLeavesSettings.get("backimage");
if (!result497) return std::unexpected(std::move(result497.error()));
auto result498 = result497->get().to_string();
if (!result498) return std::unexpected(std::move(result498.error()));
condition499 = static_cast<bool>(!std::move(*result498).empty());
}
if (condition499) {
          auto result500 = trunkLeavesSettings.get("backimage");
if (!result500) return std::unexpected(std::move(result500.error()));
auto result501 = result500->get().to_string();
if (!result501) return std::unexpected(std::move(result501.error()));
String trunkLeavesBackFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result501));
PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", trunkLeavesBackFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = -1.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = this->root().random().uniform_float() + roffset;
m_pieces.push_back(piece);
        }
      }

      auto result502 = attachmentSettings.get("x");
if (!result502) return std::unexpected(std::move(result502.error()));
auto result503 = result502->get().to_double();
if (!result503) return std::unexpected(std::move(result503.error()));
xOffset += std::move(*result503) / tile_pixels;
      auto result504 = attachmentSettings.get("y");
if (!result504) return std::unexpected(std::move(result504.error()));
auto result505 = result504->get().to_double();
if (!result505) return std::unexpected(std::move(result505.error()));
yOffset += std::move(*result505) / tile_pixels;

      // branch
      while (hasBranches && (yOffset >= branchYOffset) && ((middleHeight - i) > 0)) {
        auto result506 = rnd.uniform_int(branches.size() - 1);
if (!result506) return std::unexpected(std::move(result506.error()));
String branchKey = branches.keys()[*result506];
        auto result507 = branches[branchKey].to_object();
if (!result507) return std::unexpected(std::move(result507.error()));
JsonObject branchSettings = std::move(*result507);
        auto result508 = branchSettings["attachment"].to_object();
if (!result508) return std::unexpected(std::move(result508.error()));
JsonObject attachmentSettings = std::move(*result508);

        auto result509 = attachmentSettings.get("h");
if (!result509) return std::unexpected(std::move(result509.error()));
auto result510 = result509->get().to_double();
if (!result510) return std::unexpected(std::move(result510.error()));
float h = std::move(*result510) / tile_pixels;
        if (yOffset < branchYOffset + (h / 2.0f))
          break;

        auto result511 = attachmentSettings.get("bx");
if (!result511) return std::unexpected(std::move(result511.error()));
auto result512 = result511->get().to_double();
if (!result512) return std::unexpected(std::move(result512.error()));
float xO = xOffset + std::move(*result512) / tile_pixels;
        auto result513 = attachmentSettings.get("by");
if (!result513) return std::unexpected(std::move(result513.error()));
auto result514 = result513->get().to_double();
if (!result514) return std::unexpected(std::move(result514.error()));
float yO = branchYOffset + std::move(*result514) / tile_pixels;

        auto result515 = config.stemSettings.get_bool("alwaysBranch", false);
if (!result515) return std::unexpected(std::move(result515.error()));
bool condition517 = static_cast<bool>(std::move(*result515));
if (!condition517) {
auto result516 = rnd.uniform_int(2 + i);
if (!result516) return std::unexpected(std::move(result516.error()));
condition517 = static_cast<bool>(*result516 != 0);
}
if (condition517) {
          float boffset = this->root().random().uniform_float() + roffset;
          auto result518 = branchSettings.get("image");
if (!result518) return std::unexpected(std::move(result518.error()));
auto result519 = result518->get().to_string();
if (!result519) return std::unexpected(std::move(result519.error()));
String branchFile = AssetPath::relative_to(config.stemDirectory, std::move(*result519));
{
  PlantPiece piece;
  piece.image = Star::format("{}?hueshift={}", branchFile, config.stemHueShift);
  piece.offset = Vec2F{xO, yO};
  piece.segmentIdx = segment;
  piece.structuralSegment = false;
  piece.kind = PlantPieceKind::Stem;
  piece.zLevel = 0.0f;
  piece.rotationType = m_ceiling ? DontRotate : RotateBranch;
  piece.rotationOffset = boffset;
  m_pieces.push_back(piece);
}
          branchYOffset += h;

          // branch leaves
          auto result520 = config.foliageSettings.get_object("branchLeaves", {});
if (!result520) return std::unexpected(std::move(result520.error()));
JsonObject branchLeaves = std::move(*result520);
          if (branchLeaves.contains(branchKey)) {
            auto result521 = branchLeaves.get(branchKey);
if (!result521) return std::unexpected(std::move(result521.error()));
auto result522 = result521->get().to_object();
if (!result522) return std::unexpected(std::move(result522.error()));
JsonObject branchLeavesSettings = std::move(*result522);
            auto result523 = branchLeavesSettings["attachment"].to_object();
if (!result523) return std::unexpected(std::move(result523.error()));
JsonObject attachmentSettings = std::move(*result523);

            auto result524 = attachmentSettings.get("bx");
if (!result524) return std::unexpected(std::move(result524.error()));
auto result525 = result524->get().to_double();
if (!result525) return std::unexpected(std::move(result525.error()));
float xOf = xO + std::move(*result525) / tile_pixels;
            auto result526 = attachmentSettings.get("by");
if (!result526) return std::unexpected(std::move(result526.error()));
auto result527 = result526->get().to_double();
if (!result527) return std::unexpected(std::move(result527.error()));
float yOf = yO + std::move(*result527) / tile_pixels;

            bool condition530 = static_cast<bool>(branchLeavesSettings.contains("image"));
if (condition530) {
auto result528 = branchLeavesSettings.get("image");
if (!result528) return std::unexpected(std::move(result528.error()));
auto result529 = result528->get().to_string();
if (!result529) return std::unexpected(std::move(result529.error()));
condition530 = static_cast<bool>(!std::move(*result529).empty());
}
if (condition530) {
              auto result531 = branchLeavesSettings.get("image");
if (!result531) return std::unexpected(std::move(result531.error()));
auto result532 = result531->get().to_string();
if (!result532) return std::unexpected(std::move(result532.error()));
String branchLeavesFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result532));
PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", branchLeavesFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = 3.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = boffset;
m_pieces.push_back(piece);
            }

            bool condition535 = static_cast<bool>(branchLeavesSettings.contains("backimage"));
if (condition535) {
auto result533 = branchLeavesSettings.get("backimage");
if (!result533) return std::unexpected(std::move(result533.error()));
auto result534 = result533->get().to_string();
if (!result534) return std::unexpected(std::move(result534.error()));
condition535 = static_cast<bool>(!std::move(*result534).empty());
}
if (condition535) {
              auto result536 = branchLeavesSettings.get("backimage");
if (!result536) return std::unexpected(std::move(result536.error()));
auto result537 = result536->get().to_string();
if (!result537) return std::unexpected(std::move(result537.error()));
String branchLeavesBackFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result537));
PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", branchLeavesBackFile, config.foliageHueShift);
piece.offset = Vec2F{xOf, yOf};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = -1.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateLeaves;
piece.rotationOffset = boffset;
m_pieces.push_back(piece);
            }
          }

        } else {
          auto result538 = attachmentSettings.get("h");
if (!result538) return std::unexpected(std::move(result538.error()));
auto result539 = result538->get().to_double();
if (!result539) return std::unexpected(std::move(result539.error()));
auto result540 = rnd.uniform_int(4);
if (!result540) return std::unexpected(std::move(result540.error()));
branchYOffset += (std::move(*result539) / tile_pixels) / static_cast<float>(1 + *result540);
        }
      }
      segment++;
    }
  }

  // crown
  {
    auto result541 = config.stemSettings.get_object("crown", {});
if (!result541) return std::unexpected(std::move(result541.error()));
JsonObject crowns = std::move(*result541);
    bool hasCrown = crowns.size() > 0;
    if (hasCrown) {
      auto result542 = rnd.uniform_int(crowns.size() - 1);
if (!result542) return std::unexpected(std::move(result542.error()));
String crownKey = crowns.keys()[*result542];
      auto result543 = crowns[crownKey].to_object();
if (!result543) return std::unexpected(std::move(result543.error()));
JsonObject crownSettings = std::move(*result543);

      auto result544 = crownSettings["attachment"].to_object();
if (!result544) return std::unexpected(std::move(result544.error()));
JsonObject attachmentSettings = std::move(*result544);

      auto result545 = attachmentSettings.get("bx");
if (!result545) return std::unexpected(std::move(result545.error()));
auto result546 = result545->get().to_double();
if (!result546) return std::unexpected(std::move(result546.error()));
xOffset += std::move(*result546) / tile_pixels;
      auto result547 = attachmentSettings.get("by");
if (!result547) return std::unexpected(std::move(result547.error()));
auto result548 = result547->get().to_double();
if (!result548) return std::unexpected(std::move(result548.error()));
yOffset += std::move(*result548) / tile_pixels;

      float coffset = roffset + this->root().random().uniform_float();

      auto result549 = crownSettings.get("image");
if (!result549) return std::unexpected(std::move(result549.error()));
auto result550 = result549->get().to_string();
if (!result550) return std::unexpected(std::move(result550.error()));
String crownFile = AssetPath::relative_to(config.stemDirectory, std::move(*result550));
{
  PlantPiece piece;
  piece.image = Star::format("{}?hueshift={}", crownFile, config.stemHueShift);
  piece.offset = Vec2F{xOffset, yOffset};
  piece.segmentIdx = segment;
  piece.structuralSegment = false;
  piece.kind = PlantPieceKind::Stem;
  piece.zLevel = 0.0f;
  piece.rotationType = m_ceiling ? DontRotate : RotateCrownBranch;
  piece.rotationOffset = coffset;
  m_pieces.push_back(piece);
}
      // crown leaves
      auto result551 = config.foliageSettings.get_object("crownLeaves", {});
if (!result551) return std::unexpected(std::move(result551.error()));
JsonObject crownLeaves = std::move(*result551);
      if (crownLeaves.contains(crownKey)) {
        auto result552 = crownLeaves.get(crownKey);
if (!result552) return std::unexpected(std::move(result552.error()));
auto result553 = result552->get().to_object();
if (!result553) return std::unexpected(std::move(result553.error()));
JsonObject crownLeavesSettings = std::move(*result553);

        auto result554 = crownLeavesSettings["attachment"].to_object();
if (!result554) return std::unexpected(std::move(result554.error()));
JsonObject attachmentSettings = std::move(*result554);

        auto result555 = attachmentSettings.get("bx");
if (!result555) return std::unexpected(std::move(result555.error()));
auto result556 = result555->get().to_double();
if (!result556) return std::unexpected(std::move(result556.error()));
float xO = xOffset + std::move(*result556) / tile_pixels;
        auto result557 = attachmentSettings.get("by");
if (!result557) return std::unexpected(std::move(result557.error()));
auto result558 = result557->get().to_double();
if (!result558) return std::unexpected(std::move(result558.error()));
float yO = yOffset + std::move(*result558) / tile_pixels;

        bool condition561 = static_cast<bool>(crownLeavesSettings.contains("image"));
if (condition561) {
auto result559 = crownLeavesSettings.get("image");
if (!result559) return std::unexpected(std::move(result559.error()));
auto result560 = result559->get().to_string();
if (!result560) return std::unexpected(std::move(result560.error()));
condition561 = static_cast<bool>(!std::move(*result560).empty());
}
if (condition561) {
          auto result562 = crownLeavesSettings.get("image");
if (!result562) return std::unexpected(std::move(result562.error()));
auto result563 = result562->get().to_string();
if (!result563) return std::unexpected(std::move(result563.error()));
String crownLeavesFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result563));

PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", crownLeavesFile, config.foliageHueShift);
piece.offset = Vec2F{xO, yO};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = 3.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateCrownLeaves;
piece.rotationOffset = coffset;
m_pieces.push_back(piece);
        }

        bool condition566 = static_cast<bool>(crownLeavesSettings.contains("backimage"));
if (condition566) {
auto result564 = crownLeavesSettings.get("backimage");
if (!result564) return std::unexpected(std::move(result564.error()));
auto result565 = result564->get().to_string();
if (!result565) return std::unexpected(std::move(result565.error()));
condition566 = static_cast<bool>(!std::move(*result565).empty());
}
if (condition566) {
          auto result567 = crownLeavesSettings.get("backimage");
if (!result567) return std::unexpected(std::move(result567.error()));
auto result568 = result567->get().to_string();
if (!result568) return std::unexpected(std::move(result568.error()));
String crownLeavesBackFile = AssetPath::relative_to(config.foliageDirectory, std::move(*result568));

PlantPiece piece;
piece.image = Star::format("{}?hueshift={}", crownLeavesBackFile, config.foliageHueShift);
piece.offset = Vec2F{xO, yO};
piece.segmentIdx = segment;
piece.structuralSegment = false;
piece.kind = PlantPieceKind::Foliage;
piece.zLevel = -1.0f;
piece.rotationType = m_ceiling ? DontRotate : RotateCrownLeaves;
piece.rotationOffset = coffset;
m_pieces.push_back(piece);
        }
      }
    }
  }

  std::sort(m_pieces.begin(), m_pieces.end(), [](PlantPiece const& a, PlantPiece const& b) { return a.zLevel < b.zLevel; });
  auto result569 = validatePieces();
if (!result569) return std::unexpected(std::move(result569.error()));
  auto result570 = setupNetStates();
if (!result570) return std::unexpected(std::move(result570.error()));
  return {};
}

std::expected<Json, StarException> Plant::diskStore() const {
  auto result651 = m_tileDamageParameters.to_json();
if (!result651) return std::unexpected(std::move(result651.error()));
auto result652 = writePiecesToJson();
if (!result652) return std::unexpected(std::move(result652.error()));
return JsonObject{
    {"tilePosition", json_from_vec2i(m_tilePosition)},
    {"ceiling", m_ceiling},
    {"stemDropConfig", m_stemDropConfig},
    {"foliageDropConfig", m_foliageDropConfig},
    {"saplingDropConfig", m_saplingDropConfig},
    {"descriptions", m_descriptions},
    {"ephemeral", m_ephemeral},
    {"tileDamageParameters", std::move(*result651)},
    {"fallsWhenDead", m_fallsWhenDead},
    {"pieces", std::move(*result652)},
  };
}

std::expected<ByteArray, StarException> Plant::net_store(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  auto result638 = ds.viwrite(m_tilePosition[0]);
if (!result638) return std::unexpected(std::move(result638.error()));
  auto result639 = ds.viwrite(m_tilePosition[1]);
if (!result639) return std::unexpected(std::move(result639.error()));
  auto result640 = ds.write(m_ceiling);
if (!result640) return std::unexpected(std::move(result640.error()));
  auto result641 = ds.write(m_stemDropConfig);
if (!result641) return std::unexpected(std::move(result641.error()));
  auto result642 = ds.write(m_foliageDropConfig);
if (!result642) return std::unexpected(std::move(result642.error()));
  auto result643 = ds.write(m_saplingDropConfig);
if (!result643) return std::unexpected(std::move(result643.error()));
  auto result644 = ds.write(m_descriptions);
if (!result644) return std::unexpected(std::move(result644.error()));
  auto result645 = ds.write(m_ephemeral);
if (!result645) return std::unexpected(std::move(result645.error()));
  auto result646 = ds.write(m_tileDamageParameters);
if (!result646) return std::unexpected(std::move(result646.error()));
  auto result647 = ds.write(m_fallsWhenDead);
if (!result647) return std::unexpected(std::move(result647.error()));
  auto result648 = m_tileDamageStatus->net_store(ds, rules);
if (!result648) return std::unexpected(std::move(result648.error()));
  auto result649 = writePieces();
if (!result649) return std::unexpected(std::move(result649.error()));
auto result650 = ds.write(std::move(*result649));
if (!result650) return std::unexpected(std::move(result650.error()));

return ds.take_data();
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& rootContext, GrassVariant const& config, std::uint64_t seed) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto damageStatus = EntityTileDamageStatus::create();
  if (!damageStatus) return std::unexpected(std::move(damageStatus.error()));
  entity->m_tileDamageStatus = std::move(*damageStatus);
  if (auto result = entity->initialize(config, seed); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(GrassVariant const& config, std::uint64_t seed) {
  m_broken = false;
  m_tilePosition = Vec2I();
  m_ceiling = false;
  m_windTime = 0.0f;
  m_windLevel = 0.0f;
  m_piecesScanned = false;
  m_fallsWhenDead = false;
  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tileDamageParameters = config.tileDamageParameters;
  m_piecesUpdated = true;

  RandomSource rand(seed);

  auto image = rand.sample_value(config.images);
  if (!image)
    return std::unexpected(std::move(image.error()));
  String imageName = AssetPath::relative_to(config.directory, *image);

  Vec2F offset = Vec2F();
  // If this is a ceiling plant, offset the image so that the [0, 0] space is
  // at the top
  if (config.ceiling) {
    auto metadata = root().image_metadata_database();
    if (!metadata) return std::unexpected(std::move(metadata.error()));
    auto size = (*metadata)->image_size(imageName);
    if (!size) return std::unexpected(std::move(size.error()));
    offset = Vec2F(0.0f, 1.0f - static_cast<float>(size->y()) / tile_pixels);
  }

  PlantPiece piece;
  piece.image = Star::format("{}?hueshift={}", imageName, config.hueShift);
  piece.offset = offset;
  piece.segmentIdx = 0;
  piece.structuralSegment = true;
  piece.kind = PlantPieceKind::None;
  m_pieces = {piece};

  m_ceiling = config.ceiling;
  auto result427 = validatePieces();
if (!result427) return std::unexpected(std::move(result427.error()));
  auto result428 = setupNetStates();
if (!result428) return std::unexpected(std::move(result428.error()));
  return {};
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& rootContext, BushVariant const& config, std::uint64_t seed) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto damageStatus = EntityTileDamageStatus::create();
  if (!damageStatus) return std::unexpected(std::move(damageStatus.error()));
  entity->m_tileDamageStatus = std::move(*damageStatus);
  if (auto result = entity->initialize(config, seed); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(BushVariant const& config, std::uint64_t seed) {
  m_broken = false;
  m_tilePosition = Vec2I();
  m_ceiling = false;
  m_windTime = 0.0f;
  m_windLevel = 0.0f;
  m_piecesScanned = false;
  m_fallsWhenDead = false;
  m_descriptions = config.descriptions;
  m_ephemeral = config.ephemeral;
  m_tileDamageParameters = config.tileDamageParameters;
  m_piecesUpdated = true;

  RandomSource rand(seed);
  auto result421 = this->root().assets();
if (!result421) return std::unexpected(std::move(result421.error()));
auto assets = std::move(*result421);

  auto shapeResult = rand.sample_value(config.shapes);
  if (!shapeResult)
    return std::unexpected(std::move(shapeResult.error()));
  auto shape = std::move(*shapeResult);
  String shapeImageName = AssetPath::relative_to(config.directory, shape.image);
  auto result422 = assets->image(shapeImageName);
if (!result422) return std::unexpected(std::move(result422.error()));
float shapeImageHeight = std::move(*result422)->height();
  Vec2F offset = Vec2F();
  // If this is a ceiling plant, offset the image so that the [0, 0] space is
  // at the top
  if (config.ceiling)
    offset = Vec2F(0.0f, 1.0f - shapeImageHeight / tile_pixels);

  {
    PlantPiece piece;
    piece.image = Star::format("{}?hueshift={}", shapeImageName, config.baseHueShift);
    piece.offset = offset;
    piece.segmentIdx = 0;
    piece.structuralSegment = true;
    piece.kind = PlantPieceKind::None;
    m_pieces.push_back(piece);
  }

  auto modResult = rand.sample_value(shape.mods);
  if (!modResult)
    return std::unexpected(std::move(modResult.error()));
  auto mod = std::move(*modResult);
  if (!mod.empty()) {
    PlantPiece piece;
    piece.image = Star::format("{}?hueshift={}", AssetPath::relative_to(config.directory, mod), config.modHueShift);
    piece.offset = offset;
    piece.segmentIdx = 0;
    piece.structuralSegment = false;
    piece.kind = PlantPieceKind::None;
    m_pieces.push_back(piece);
  }

  m_ceiling = config.ceiling;
  auto result423 = validatePieces();
if (!result423) return std::unexpected(std::move(result423.error()));
  auto result424 = setupNetStates();
if (!result424) return std::unexpected(std::move(result424.error()));
  return {};
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& rootContext, Json const& diskStore) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto damageStatus = EntityTileDamageStatus::create();
  if (!damageStatus) return std::unexpected(std::move(damageStatus.error()));
  entity->m_tileDamageStatus = std::move(*damageStatus);
  if (auto result = entity->initialize(diskStore); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(Json const& diskStore) {
  auto result406 = diskStore.get("tilePosition");
if (!result406) return std::unexpected(std::move(result406.error()));
auto result407 = json_to_vec2i(std::move(*result406));
if (!result407) return std::unexpected(std::move(result407.error()));
m_tilePosition = std::move(*result407);
  auto result408 = diskStore.get_bool("ceiling");
if (!result408) return std::unexpected(std::move(result408.error()));
m_ceiling = std::move(*result408);
  auto result409 = diskStore.get("stemDropConfig");
if (!result409) return std::unexpected(std::move(result409.error()));
m_stemDropConfig = std::move(*result409);
  auto result410 = diskStore.get("foliageDropConfig");
if (!result410) return std::unexpected(std::move(result410.error()));
m_foliageDropConfig = std::move(*result410);
  auto result411 = diskStore.get("saplingDropConfig");
if (!result411) return std::unexpected(std::move(result411.error()));
m_saplingDropConfig = std::move(*result411);
  auto result412 = diskStore.get("descriptions");
if (!result412) return std::unexpected(std::move(result412.error()));
m_descriptions = std::move(*result412);
  auto result413 = diskStore.get_bool("ephemeral");
if (!result413) return std::unexpected(std::move(result413.error()));
m_ephemeral = std::move(*result413);
  auto result414 = root().assets();
if (!result414) return std::unexpected(std::move(result414.error()));
auto result415 = diskStore.get("tileDamageParameters");
if (!result415) return std::unexpected(std::move(result415.error()));
auto result416 = TileDamageParameters::create(*std::move(*result414), std::move(*result415));
if (!result416) return std::unexpected(std::move(result416.error()));
m_tileDamageParameters = std::move(*result416);
  auto result417 = diskStore.get_bool("fallsWhenDead");
if (!result417) return std::unexpected(std::move(result417.error()));
m_fallsWhenDead = std::move(*result417);
  auto result418 = diskStore.get("pieces");
if (!result418) return std::unexpected(std::move(result418.error()));
auto result419 = readPiecesFromJson(std::move(*result418));
if (!result419) return std::unexpected(std::move(result419.error()));

  auto result420 = setupNetStates();
if (!result420) return std::unexpected(std::move(result420.error()));
  return {};
}

std::expected<std::shared_ptr<Plant>, StarException> Plant::create(Root& rootContext, ByteArray const& net_store, NetCompatibilityRules rules) {
  struct MakeShared final : Plant {
    explicit MakeShared(Root& root) : Entity(root), Plant(root) {}
  };
  auto entity = std::make_shared<MakeShared>(rootContext);
  auto damageStatus = EntityTileDamageStatus::create();
  if (!damageStatus) return std::unexpected(std::move(damageStatus.error()));
  entity->m_tileDamageStatus = std::move(*damageStatus);
  if (auto result = entity->initialize(net_store, rules); !result) return std::unexpected(std::move(result.error()));
  return entity;
}

std::expected<void, StarException> Plant::initialize(ByteArray const& net_store, NetCompatibilityRules rules) {
  m_broken = false;
  m_tilePosition = Vec2I();
  m_ceiling = false;
  m_windTime = 0.0f;
  m_windLevel = 0.0f;
  m_piecesScanned = false;
  m_fallsWhenDead = false;
  m_piecesUpdated = true;

  DataStreamReader ds(net_store.span());
  ds.set_stream_compatibility_version(rules);
  auto result392 = ds.viread(m_tilePosition[0]);
if (!result392) return std::unexpected(std::move(result392.error()));
  auto result393 = ds.viread(m_tilePosition[1]);
if (!result393) return std::unexpected(std::move(result393.error()));
  auto result394 = ds.read(m_ceiling);
if (!result394) return std::unexpected(std::move(result394.error()));
  auto result395 = ds.read(m_stemDropConfig);
if (!result395) return std::unexpected(std::move(result395.error()));
  auto result396 = ds.read(m_foliageDropConfig);
if (!result396) return std::unexpected(std::move(result396.error()));
  auto result397 = ds.read(m_saplingDropConfig);
if (!result397) return std::unexpected(std::move(result397.error()));
  auto result398 = ds.read(m_descriptions);
if (!result398) return std::unexpected(std::move(result398.error()));
  auto result399 = ds.read(m_ephemeral);
if (!result399) return std::unexpected(std::move(result399.error()));
  auto result400 = ds.read(m_tileDamageParameters);
if (!result400) return std::unexpected(std::move(result400.error()));
  auto result401 = ds.read(m_fallsWhenDead);
if (!result401) return std::unexpected(std::move(result401.error()));
  auto result402 = m_tileDamageStatus->net_load(ds, rules);
if (!result402) return std::unexpected(std::move(result402.error()));
  auto result403 = ds.read<ByteArray>();
if (!result403) return std::unexpected(std::move(result403.error()));
auto result404 = readPieces(std::move(*result403));
if (!result404) return std::unexpected(std::move(result404.error()));

  auto result405 = setupNetStates();
if (!result405) return std::unexpected(std::move(result405.error()));
  return {};
}

Plant::Plant(Root& rootContext) : Entity(rootContext) {
  m_ephemeral = false;
  m_piecesUpdated = true;
  m_ceiling = false;
  m_broken = false;
  m_fallsWhenDead = false;
  m_windTime = 0.0f;
  m_windLevel = 0.0f;
  m_piecesScanned = false;
  m_tileDamageX = 0.0f;
  m_tileDamageY = 0.0f;
  m_tileDamageEventTrigger = false;
  m_tileDamageEvent = false;
}

EntityType Plant::entity_type() const {
  return EntityType::Plant;
}

std::expected<void, StarException> Plant::init(World& world, EntityId entityId, EntityMode mode) {
  auto result635 = Entity::init(world, entityId, mode);
if (!result635) return std::unexpected(std::move(result635.error()));
  auto result636 = validatePieces();
if (!result636) return std::unexpected(std::move(result636.error()));
  m_tilePosition = world.geometry().xwrap(m_tilePosition);
  return {};
}

std::expected<std::pair<ByteArray, std::uint64_t>, StarException> Plant::write_net_state(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  auto result634 = m_netGroup.write_net_state(fromVersion, rules);
if (!result634) return std::unexpected(std::move(result634.error()));
return std::move(*result634);
}

std::expected<void, StarException> Plant::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  return m_netGroup.read_net_state(data, interpolation_time, rules);
}

std::expected<void, StarException> Plant::enable_interpolation(float extrapolation_hint) {
  // Only enable plant interpolation when it actually matters, for things that
  // generate PlantDrops so that they match when the PlantDrops appear.
  if (m_fallsWhenDead)
    {
auto result632 = m_netGroup.enable_net_interpolation(extrapolation_hint);
if (!result632) return std::unexpected(std::move(result632.error()));
}
  return {};
}

std::expected<void, StarException> Plant::disable_interpolation() {
  auto result631 = m_netGroup.disable_net_interpolation();
if (!result631) return std::unexpected(std::move(result631.error()));
  return {};
}

std::expected<String, StarException> Plant::description() const {
  auto result630 = m_descriptions.get_string("description");
if (!result630) return std::unexpected(std::move(result630.error()));
return std::move(*result630);
}

Vec2F Plant::position() const {
  return Vec2F(m_tilePosition);
}

RectF Plant::meta_bound_box() const {
  return m_meta_bound_box;
}

bool Plant::ephemeral() const {
  return m_ephemeral;
}

Vec2I Plant::tile_position() const {
  return m_tilePosition;
}

std::expected<void, StarException> Plant::set_tile_position(Vec2I const& tilePosition) {
  m_tilePosition = tilePosition;
  return {};
}

List<Vec2I> Plant::spaces() const {
  return m_spaces;
}

List<Vec2I> Plant::roots() const {
  return m_roots;
}

Vec2I Plant::primaryRoot() const {
  return m_ceiling ? Vec2I(0, 1) : Vec2I(0, -1);
}

bool Plant::ceiling() const {
  return m_ceiling;
}

std::expected<bool, StarException> Plant::should_destroy() const {
  return m_broken || m_pieces.size() == 0;
}

std::expected<bool, StarException> Plant::check_broken() {
  if (!m_broken) {
    auto result627 = allSpacesOccupied(m_roots);
if (!result627) return std::unexpected(std::move(result627.error()));
if (!std::move(*result627)) {
      if (m_fallsWhenDead) {
        auto result628 = breakAtPosition(m_tilePosition, Vec2F(m_tilePosition));
if (!result628) return std::unexpected(std::move(result628.error()));
        return false;
      } else
        m_broken = true;
    } else {
auto result629 = anySpacesOccupied(m_spaces);
if (!result629) return std::unexpected(std::move(result629.error()));
if (std::move(*result629))
      m_broken = true;
}
  }

  return m_broken;
}

List<Plant::PlantPiece> Plant::pieces() const {
  return m_pieces;
}

RectF Plant::interactive_bound_box() const {
  return RectF(m_boundBox);
}

std::expected<void, StarException> Plant::scanSpacesAndRoots() {
  auto metadata = root().image_metadata_database();
  if (!metadata) return std::unexpected(std::move(metadata.error()));

  // build spaces
  Set<Vec2I> spaces;

  // always include the base position in spaces, it causes all kinds of problems if you don't
  spaces.add({0, 0});

  for (auto& piece : m_pieces) {
    auto size = (*metadata)->image_size(piece.image);
    if (!size) return std::unexpected(std::move(size.error()));
    piece.imageSize = *size;
    auto occupied = (*metadata)->image_spaces(piece.image, piece.offset * tile_pixels, PlantScanThreshold, piece.flip);
    if (!occupied) return std::unexpected(std::move(occupied.error()));
    piece.spaces = Set<Vec2I>::from(std::move(*occupied));
    spaces.insert_range(piece.spaces);
  }

  m_spaces = spaces.values();

  m_boundBox = RectI::bound_box_of_points(m_spaces);

  for (auto space : m_spaces) {
    if (space[1] == 0) {
      if (m_ceiling)
        m_roots.push_back({space[0], 1});
      else
        m_roots.push_back({space[0], -1});
    }
  }
  return {};
}

void Plant::calcBoundBox() {
  RectF boundBox = RectF::bound_box_of_points(m_spaces);
  // Plants are allowed to visibly occupy one outside space from the spaces
  // they take up.
  m_meta_bound_box = RectF(boundBox.min() - Vec2F(1, 1), boundBox.max() + Vec2F(2, 2));
}

float Plant::branchRotation(float xPos, float rotoffset, float interpolation) const {
  if (!inWorld() || (m_windLevel == 0.0f && m_previousWindLevel == 0.0f))
    return 0.0f;

  float windLevel = std::lerp(m_previousWindLevel, m_windLevel, std::clamp(interpolation, 0.0f, 1.0f));
  float intensity = std::fabs(windLevel);

  float windTime = m_windTime + std::remainder(m_previousWindTime - m_windTime, 628.32f) * (1.0f - std::clamp(interpolation, 0.0f, 1.0f));
  return std::copysign(0.00117f, windLevel) * (std::sin(windTime + rotoffset + xPos / 10.0f) * intensity - intensity / 300.0f);
}

std::expected<void, StarException> Plant::update(float dt, std::uint64_t) {
  m_previousWindTime = m_windTime;
  m_previousWindLevel = m_windLevel;
  m_windTime += dt;
  m_windTime = std::fmod(m_windTime, 628.32f);
  auto result621 = world();
if (!result621) return std::unexpected(std::move(result621.error()));
auto result622 = result621->get().windLevel(Vec2F(m_tilePosition));
if (!result622) return std::unexpected(std::move(result622.error()));
m_windLevel = std::move(*result622);

  if (isMaster()) {
    if (m_tileDamageStatus->damaged())
      m_tileDamageStatus->recover(m_tileDamageParameters, dt);
  } else {
    if (m_tileDamageStatus->damaged() && !m_tileDamageStatus->damage_protected()) {
      float damageEffectPercentage = m_tileDamageStatus->damage_effect_percentage();
      m_windTime += damageEffectPercentage * 10 * dt;
      m_windLevel += damageEffectPercentage * 20;
    }

    auto result623 = m_netGroup.tick_net_interpolation(dt);
if (!result623) return std::unexpected(std::move(result623.error()));
  }
  return {};
}

std::expected<void, StarException> Plant::collect_render_events(RenderCallback& render_callback) {
  auto damageOffset = this->root().random().uniform_float(-0.1f, 0.1f);
  if (!damageOffset)
    return std::unexpected(std::move(damageOffset.error()));
  m_presentationDamageXOffset = *damageOffset * m_tileDamageStatus->damage_effect_percentage();
  if (m_tileDamageEvent) {
    m_tileDamageEvent = false;
    if (m_stemDropConfig.type() == Json::Type::Object) {
      auto result605 = m_stemDropConfig.get("particles", JsonObject());
if (!result605) return std::unexpected(std::move(result605.error()));
auto result606 = std::move(*result605).get("damageTree", JsonObject());
if (!result606) return std::unexpected(std::move(result606.error()));
Json particleConfig = std::move(*result606);
      auto result607 = particleConfig.get_array("options", {});
if (!result607) return std::unexpected(std::move(result607.error()));
JsonArray particleOptions = std::move(*result607);
      auto result608 = m_stemDropConfig.get_float("hueshift", 0);
if (!result608) return std::unexpected(std::move(result608.error()));
auto hueshift = std::move(*result608) / 360.0f;
      auto result609 = particleConfig.get_float("density", 1);
if (!result609) return std::unexpected(std::move(result609.error()));
auto density = std::move(*result609);
      while (density-- > 0) {
        auto configResult = this->root().random().sample_value(particleOptions, {});
        if (!configResult)
          return std::unexpected(std::move(configResult.error()));
        auto config = std::move(*configResult);
        bool condition1282 = static_cast<bool>(config.is_null());
if (!condition1282) {
auto result1281 = config.size();
if (!result1281) return std::unexpected(std::move(result1281.error()));
condition1282 = static_cast<bool>(std::move(*result1281) == 0);
}
if (condition1282)
          continue;
        auto result610 = this->root().particleDatabase();
if (!result610) return std::unexpected(std::move(result610.error()));
auto result611 = std::move(*result610)->particle(config);
if (!result611) return std::unexpected(std::move(result611.error()));
auto particle = std::move(*result611);
        particle.color.hue_shift(hueshift);
        if (!particle.string.empty()) {
          particle.string = Star::format("{}?hueshift={}", particle.string, hueshift);
          particle.image = particle.string;
        }
        particle.position = {m_tileDamageX + this->root().random().uniform_float(), m_tileDamageY + this->root().random().uniform_float()};
        particle.translate(position());
        auto result612 = render_callback.add_particle(std::move(particle));
if (!result612) return std::unexpected(std::move(result612.error()));
      }
      auto result613 = m_stemDropConfig.get("sounds", JsonObject());
if (!result613) return std::unexpected(std::move(result613.error()));
auto result614 = std::move(*result613).get_array("damageTree", JsonArray());
if (!result614) return std::unexpected(std::move(result614.error()));
JsonArray damageTreeSoundOptions = std::move(*result614);
      if (damageTreeSoundOptions.size()) {
        auto result615 = this->root().random().sample_ref(damageTreeSoundOptions);
if (!result615) return std::unexpected(std::move(result615.error()));
auto sound = result615->get();
        Vec2F pos = position() + Vec2F(m_tileDamageX + this->root().random().uniform_float(), m_tileDamageY + this->root().random().uniform_float());
        auto result616 = this->root().assets();
if (!result616) return std::unexpected(std::move(result616.error()));
auto assets = std::move(*result616);
        auto result617 = sound.get_string("file");
if (!result617) return std::unexpected(std::move(result617.error()));
auto result618 = assets->audio(std::move(*result617));
if (!result618) return std::unexpected(std::move(result618.error()));
auto result1277 = AudioInstance::create(*std::move(*result618));
if (!result1277) return std::unexpected(std::move(result1277.error()));
auto audioInstance = std::move(*result1277);
audioInstance->set_position(pos);
auto result619 = sound.get_float("volume", 1.0f);
if (!result619) return std::unexpected(std::move(result619.error()));
audioInstance->set_volume(std::move(*result619));
auto result620 = render_callback.add_audio(std::move(audioInstance));
if (!result620) return std::unexpected(std::move(result620.error()));
      }
    }
  }
  return {};
}

std::expected<void, StarException> Plant::render(RenderCallback& render_callback) {
  float damageXOffset = m_presentationDamageXOffset;

  for (auto const& plantPiece : m_pieces) {
    auto size = Vec2F(plantPiece.imageSize) / tile_pixels;

    Vec2F offset = plantPiece.offset;
    if ((m_ceiling && offset[1] <= m_tileDamageY) || (!m_ceiling && offset[1] + size[1] >= m_tileDamageY))
      offset[0] += damageXOffset;

    auto result603 = this->root().image_metadata_database();
if (!result603) return std::unexpected(std::move(result603.error()));
auto result1283 = Drawable::make_image(*std::move(*result603), plantPiece.imagePath, 1.0f / tile_pixels, false, offset);
if (!result1283) return std::unexpected(std::move(result1283.error()));
auto drawable = std::move(*result1283);
    if (plantPiece.flip)
      drawable.scale(Vec2F(-1, 1));

    if (plantPiece.rotationType == RotateCrownBranch || plantPiece.rotationType == RotateCrownLeaves) {
      drawable.rotate(branchRotation(m_tilePosition[0], plantPiece.rotationOffset * 1.4f, render_callback.presentation().interpolation()) * 0.7f, plantPiece.offset + Vec2F(size[0] / 2.0f, 0));
      drawable.translate(Vec2F(0, -0.40f));
    } else if (plantPiece.rotationType == RotateBranch || plantPiece.rotationType == RotateLeaves) {
      drawable.rotate(branchRotation(m_tilePosition[0], plantPiece.rotationOffset * 1.4f, render_callback.presentation().interpolation()), plantPiece.offset + Vec2F(size) / 2.0f);
    }
    drawable.translate(position());
    auto result604 = render_callback.add_drawable(std::move(drawable), render_layer_plant);
if (!result604) return std::unexpected(std::move(result604.error()));
  }

  return {};
}

std::expected<void, StarException> Plant::readPieces(ByteArray pieces) {
  if (!pieces.empty()) {
    DataStreamBuffer ds(std::move(pieces));
    auto result600 =
      ds.read_container(m_pieces, [](DataStream& ds, PlantPiece& piece) -> std::expected<void, StarException> {
        auto result383 = ds.read(piece.image);
        if (!result383)
          return std::unexpected(std::move(result383.error()));
        auto result384 = ds.read(piece.offset[0]);
        if (!result384)
          return std::unexpected(std::move(result384.error()));
        auto result385 = ds.read(piece.offset[1]);
        if (!result385)
          return std::unexpected(std::move(result385.error()));
        auto result386 = ds.read(piece.rotationType);
        if (!result386)
          return std::unexpected(std::move(result386.error()));
        auto result387 = ds.read(piece.rotationOffset);
        if (!result387)
          return std::unexpected(std::move(result387.error()));
        auto result388 = ds.read(piece.structuralSegment);
        if (!result388)
          return std::unexpected(std::move(result388.error()));
        auto result389 = ds.read(piece.kind);
        if (!result389)
          return std::unexpected(std::move(result389.error()));
        auto result390 = ds.read(piece.segmentIdx);
        if (!result390)
          return std::unexpected(std::move(result390.error()));
        auto result391 = ds.read(piece.flip);
        if (!result391)
          return std::unexpected(std::move(result391.error()));
        return {};
      });
    if (!result600)
      return std::unexpected(std::move(result600.error()));
    m_piecesScanned = false;
    if (inWorld())
      {
auto result601 = validatePieces();
if (!result601) return std::unexpected(std::move(result601.error()));
}
  }
  return {};
}

std::expected<ByteArray, StarException> Plant::writePieces() const {
  auto result599 = DataStreamBuffer::serialize_container(
    m_pieces, [](DataStream& ds, PlantPiece const& piece) -> std::expected<void, StarException> {
      auto result374 = ds.write(piece.image);
      if (!result374)
        return std::unexpected(std::move(result374.error()));
      auto result375 = ds.write(piece.offset[0]);
      if (!result375)
        return std::unexpected(std::move(result375.error()));
      auto result376 = ds.write(piece.offset[1]);
      if (!result376)
        return std::unexpected(std::move(result376.error()));
      auto result377 = ds.write(piece.rotationType);
      if (!result377)
        return std::unexpected(std::move(result377.error()));
      auto result378 = ds.write(piece.rotationOffset);
      if (!result378)
        return std::unexpected(std::move(result378.error()));
      auto result379 = ds.write(piece.structuralSegment);
      if (!result379)
        return std::unexpected(std::move(result379.error()));
      auto result380 = ds.write(piece.kind);
      if (!result380)
        return std::unexpected(std::move(result380.error()));
      auto result381 = ds.write(piece.segmentIdx);
      if (!result381)
        return std::unexpected(std::move(result381.error()));
      auto result382 = ds.write(piece.flip);
      if (!result382)
        return std::unexpected(std::move(result382.error()));
      return {};
    });
  if (!result599)
    return std::unexpected(std::move(result599.error()));
  return std::move(*result599);
}

std::expected<void, StarException> Plant::readPiecesFromJson(Json const& pieces) {
  auto result597 = json_to_list<PlantPiece>(pieces, [](Json const& v) -> std::expected<PlantPiece, StarException> {
      PlantPiece res;
      auto result364 = v.get_string("image");
if (!result364) return std::unexpected(std::move(result364.error()));
res.image = std::move(*result364);
      auto result365 = v.get("offset");
if (!result365) return std::unexpected(std::move(result365.error()));
auto result366 = json_to_vec2f(std::move(*result365));
if (!result366) return std::unexpected(std::move(result366.error()));
res.offset = std::move(*result366);
      auto result367 = v.get_string("rotationType");
if (!result367) return std::unexpected(std::move(result367.error()));
if (!RotationTypeNames) return std::unexpected(RotationTypeNames.error());
auto result368 = RotationTypeNames->get_left(std::move(*result367));
if (!result368) return std::unexpected(std::move(result368.error()));
res.rotationType = result368->get();
      auto result369 = v.get_float("rotationOffset");
if (!result369) return std::unexpected(std::move(result369.error()));
res.rotationOffset = std::move(*result369);
      auto result370 = v.get_bool("structuralSegment");
if (!result370) return std::unexpected(std::move(result370.error()));
res.structuralSegment = std::move(*result370);
      auto result371 = v.get_int("kind");
if (!result371) return std::unexpected(std::move(result371.error()));
res.kind = static_cast<PlantPieceKind>(std::move(*result371));
auto result372 = v.get_int("segmentIdx");
if (!result372) return std::unexpected(std::move(result372.error()));
res.segmentIdx = std::move(*result372);
      auto result373 = v.get_bool("flip");
if (!result373) return std::unexpected(std::move(result373.error()));
res.flip = std::move(*result373);

      return res;
    });
if (!result597) return std::unexpected(std::move(result597.error()));
m_pieces = std::move(*result597);
  m_piecesScanned = false;
  if (inWorld())
    {
auto result598 = validatePieces();
if (!result598) return std::unexpected(std::move(result598.error()));
}
  return {};
}

std::expected<Json, StarException> Plant::writePiecesToJson() const {
  auto result596 = json_from_list<PlantPiece>(m_pieces, [](PlantPiece const& piece) -> std::expected<Json, StarException> {
      if (!RotationTypeNames) return std::unexpected(RotationTypeNames.error());
      auto result363 = RotationTypeNames->get_right(piece.rotationType);
      if (!result363)
        return std::unexpected(std::move(result363.error()));
      return JsonObject{
        {"image", piece.image},
        {"offset", json_from_vec2f(piece.offset)},
        {"rotationType", result363->get()},
        {"rotationOffset", piece.rotationOffset},
        {"structuralSegment", piece.structuralSegment},
        {"kind", piece.kind},
        {"segmentIdx", piece.segmentIdx},
        {"flip", piece.flip},
      };
    });
if (!result596) return std::unexpected(std::move(result596.error()));
return std::move(*result596);
}

std::expected<void, StarException> Plant::validatePieces() {
  for (auto& piece : m_pieces)
    piece.imagePath = piece.image;
  if (!m_piecesScanned) {
    auto result595 = scanSpacesAndRoots();
if (!result595) return std::unexpected(std::move(result595.error()));
    calcBoundBox();
    m_piecesScanned = true;
  }
  return {};
}

std::expected<void, StarException> Plant::setupNetStates() {
  auto result590 = m_netGroup.add_net_element(*m_tileDamageStatus);
if (!result590) return std::unexpected(std::move(result590.error()));
  auto result591 = m_netGroup.add_net_element(m_piecesNetState);
if (!result591) return std::unexpected(std::move(result591.error()));
  auto result592 = m_netGroup.add_net_element(m_tileDamageXNetState);
if (!result592) return std::unexpected(std::move(result592.error()));
  auto result593 = m_netGroup.add_net_element(m_tileDamageYNetState);
if (!result593) return std::unexpected(std::move(result593.error()));
  auto result594 = m_netGroup.add_net_element(m_tileDamageEventNetState);
if (!result594) return std::unexpected(std::move(result594.error()));

m_netGroup.set_needs_store_callback([this]() { return setNetStates(); });
m_netGroup.set_needs_load_callback([this](bool) { return getNetStates(); });
return {};
}

std::expected<void, StarException> Plant::getNetStates() {
  if (m_piecesNetState.pull_updated()) {
    auto result589 = readPieces(m_piecesNetState.get());
if (!result589) return std::unexpected(std::move(result589.error()));
    m_piecesUpdated = true;
  }

  m_tileDamageX = m_tileDamageXNetState.get();
  m_tileDamageY = m_tileDamageYNetState.get();
  if (m_tileDamageEventNetState.pull_occurred()) {
    m_tileDamageEvent = true;
    m_tileDamageEventTrigger = true;
  }
  return {};
}

std::expected<void, StarException> Plant::setNetStates() {
  if (m_piecesUpdated) {
    auto result588 = writePieces();
if (!result588) return std::unexpected(std::move(result588.error()));
m_piecesNetState.set(std::move(*result588));
    m_piecesUpdated = false;
  }
  m_tileDamageXNetState.set(m_tileDamageX);
  m_tileDamageYNetState.set(m_tileDamageY);
  if (m_tileDamageEventTrigger) {
    m_tileDamageEventTrigger = false;
    m_tileDamageEventNetState.trigger();
  }
  return {};
}

std::expected<bool, StarException> Plant::damage_tiles(List<Vec2I> const& positions, Vec2F const& sourcePosition, TileDamage const& tileDamage) {
  auto result584 = baseDamagePosition(positions);
if (!result584) return std::unexpected(std::move(result584.error()));
auto position = std::move(*result584);

  auto result585 = world();
if (!result585) return std::unexpected(std::move(result585.error()));
auto geometry = result585->get().geometry();

  m_tileDamageStatus->damage(m_tileDamageParameters, tileDamage);
  m_tileDamageX = geometry.diff(position[0], tile_position()[0]);
  m_tileDamageY = position[1] - tile_position()[1];
  m_tileDamageEvent = true;
  m_tileDamageEventTrigger = true;

  bool breaking = false;
  if (m_tileDamageStatus->dead()) {
    breaking = true;
    if (m_fallsWhenDead) {
      m_tileDamageStatus->reset();
      auto result587 = breakAtPosition(position, sourcePosition);
if (!result587) return std::unexpected(std::move(result587.error()));
    } else {
      m_broken = true;
    }
  }

  return breaking;
}

std::expected<void, StarException> Plant::breakAtPosition(Vec2I const& position, Vec2F const& sourcePosition) {
  auto result575 = world();
if (!result575) return std::unexpected(std::move(result575.error()));
auto geometry = result575->get().geometry();
  Vec2I internalPos = geometry.diff(position, tile_position());
  std::size_t idx = std::numeric_limits<std::size_t>::max();
  int segmentIdx = std::numeric_limits<int>::max();
  for (std::size_t i = 0; i < m_pieces.size(); ++i) {
    auto& piece = m_pieces[i];
    if (piece.structuralSegment && piece.spaces.contains(internalPos)) {
      if (piece.segmentIdx < segmentIdx) {
        segmentIdx = piece.segmentIdx;
        idx = i;
      }
    }
  }

  // default to highest structural piece
  if (idx >= m_pieces.size()) {
    for (std::size_t i = m_pieces.size(); i > 0; --i) {
      auto& piece = m_pieces[i - 1];
      if (piece.structuralSegment) {
        segmentIdx = piece.segmentIdx;
        idx = i - 1;
        break;
      }
    }
  }

  // plant has no structural segments? this is a terrible fallback because it
  // prevents destruction
  if (idx >= m_pieces.size())
    return {};

  PlantPiece breakPiece = m_pieces[idx];
  Vec2F breakPoint = Vec2F(position) - Vec2F(tile_position());
  if (breakPiece.spaces.size()) {
    RectF bounds = RectF::null();
    for (auto space : breakPiece.spaces) {
      bounds.combine(Vec2F(space));
      bounds.combine(Vec2F(space) + Vec2F(1, 1));
    }
    breakPoint[0] = (bounds.max()[0] + bounds.min()[0]) / 2.0f;
    if (!m_ceiling)
      breakPoint[1] = bounds.min()[1];
    else
      breakPoint[1] = bounds.max()[1];
  }

  List<PlantPiece> droppedPieces;
  if (m_pieces[idx].structuralSegment) {
    idx = 0;
    while (idx < m_pieces.size()) {
      if (m_pieces[idx].segmentIdx >= segmentIdx) {
        auto result576 = m_pieces.take_at(idx);
        if (!result576)
          return std::unexpected(std::move(result576.error()));
        droppedPieces.push_back(std::move(*result576));
        continue;
      }
      idx++;
    }
  } else {
    auto result577 = m_pieces.take_at(idx);
    if (!result577)
      return std::unexpected(std::move(result577.error()));
    droppedPieces.push_back(std::move(*result577));
  }
  m_piecesUpdated = true;

  Vec2I breakPointI = Vec2I(std::round(breakPoint[0]), std::round(breakPoint[1]));

  // Calculate a new origin for the droppedPieces
  for (auto& piece : droppedPieces) {
    piece.offset -= breakPoint;
    Set<Vec2I> spaces = piece.spaces;
    piece.spaces.clear();
    for (auto const& space : spaces) {
      piece.spaces.add(space - breakPointI);
    }
  }

  Vec2F worldSpaceBreakPoint = breakPoint + Vec2F(tile_position());

  List<int> segmentOrder;
  Map<int, List<PlantPiece>> segments;
  for (auto& piece : droppedPieces) {
    if (!segments.contains(piece.segmentIdx))
      segmentOrder.push_back(piece.segmentIdx);
    segments[piece.segmentIdx].push_back(piece);
  }
  std::reverse(segmentOrder.begin(), segmentOrder.end());
  auto result578 = this->root().random().uniform_float(-0.3f, 0.3f);
if (!result578) return std::unexpected(std::move(result578.error()));
float random = std::move(*result578);
  auto fallVector = (worldSpaceBreakPoint - sourcePosition).normalized();
  bool first = true;
  for (auto segmentIdx : segmentOrder) {
    auto segment = segments[segmentIdx];
    auto result579 = world();
if (!result579) return std::unexpected(std::move(result579.error()));
auto result580 = description();
if (!result580) return std::unexpected(std::move(result580.error()));
auto result581 = PlantDrop::create(this->root(), segment,
        worldSpaceBreakPoint,
        fallVector,
        std::move(*result580),
        m_ceiling,
        m_stemDropConfig,
        m_foliageDropConfig,
        m_saplingDropConfig,
        first,
        random);
if (!result581) return std::unexpected(std::move(result581.error()));
auto result582 = result579->get().addEntity(std::move(*result581));
if (!result582) return std::unexpected(std::move(result582.error()));
    first = false;
  }

  m_piecesScanned = false;

  auto result583 = validatePieces();
if (!result583) return std::unexpected(std::move(result583.error()));
  return {};
}

std::expected<Vec2I, StarException> Plant::baseDamagePosition(List<Vec2I> const& positions) const {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((positions.size()));
  }
  auto result572 = positions.at(0);
if (!result572) return std::unexpected(std::move(result572.error()));
auto res = result572->get();

  for (auto const& piece : m_pieces) {
    if (piece.structuralSegment) {
      for (auto space : piece.spaces) {
        for (auto position : positions) {
          auto result573 = world();
if (!result573) return std::unexpected(std::move(result573.error()));
          if (result573->get().geometry().equal(m_tilePosition + space, position)) {
            // if this space is a "better match" for the root of the plant
            if ((res[1] < position[1]) == m_ceiling) {
              res = position;
            }
          }
        }
      }
    }
  }

  return res;
}

std::expected<bool, StarException> Plant::damagable() const {
  if (m_stemDropConfig.type() != Json::Type::Object)
    return true;
  auto result571 = m_stemDropConfig.get_bool("destructable", true);
if (!result571) return std::unexpected(std::move(result571.error()));
if (!std::move(*result571))
    return false;
  return true;
}

}
