import star.json;
import star.json_extra;
import star.list;
import star.rect;
import star.color;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.bi_map;
import star.game_types;
import star.material_types;
import star.vector;
import star.lexical_cast;
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
import star.material_render_profile;
import star.asset_source;
import star.assets;


import star.image_metadata_database;

import star.math_common;
namespace Star {

[[nodiscard]]
std::expected<MaterialRenderMatchList, StarException> parseMaterialRenderMatchList(Json const& matchSpec, RuleMap const& ruleMap, PieceMap const& pieceMap, MatchMap const& matchMap) {
  MaterialRenderMatchList result;
  auto configs = matchSpec.toArray();
  if (!configs) return std::unexpected(std::move(configs.error()));
  for (auto const& config : *configs) {
    auto match = std::make_shared<MaterialRenderMatch>();
    Json points = JsonArray();
    auto all = config.opt("matchAllPoints");
    if (!all) return std::unexpected(std::move(all.error()));
    if (*all) {
      points = **all;
      match->matchJoin = MaterialJoinType::All;
    } else {
      auto any = config.opt("matchAnyPoints");
      if (!any) return std::unexpected(std::move(any.error()));
      if (*any) {
        points = **any;
        match->matchJoin = MaterialJoinType::Any;
      }
    }
    auto pointConfigs = points.iterateArray();
    if (!pointConfigs) return std::unexpected(std::move(pointConfigs.error()));
    for (auto const& pointConfig : *pointConfigs) {
      MaterialMatchPoint point;
      auto position = pointConfig.get(0);
      if (!position) return std::unexpected(std::move(position.error()));
      {
        auto value = jsonToVec2I(*position);
        if (!value) return std::unexpected(std::move(value.error()));
        point.position = std::move(*value);
      }
      if (std::abs(point.position[0]) > MaterialRenderProfileMaxNeighborDistance || std::abs(point.position[1]) > MaterialRenderProfileMaxNeighborDistance)
        return std::unexpected(MaterialRenderProfileException(strf("Match position {} outside of maximum rule distance {}", point.position, MaterialRenderProfileMaxNeighborDistance)));
      auto name = pointConfig.getString(1);
      if (!name) return std::unexpected(std::move(name.error()));
      auto rule = ruleMap.get(*name);
      if (!rule) return std::unexpected(std::move(rule.error()));
      point.rule = rule->get();
      match->matchPoints.push_back(std::move(point));
    }
    auto pieces = config.getArray("pieces", {});
    if (!pieces) return std::unexpected(std::move(pieces.error()));
    for (auto const& piece : *pieces) {
      auto name = piece.getString(0);
      if (!name) return std::unexpected(std::move(name.error()));
      auto renderPiece = pieceMap.get(*name);
      if (!renderPiece) return std::unexpected(std::move(renderPiece.error()));
      auto positionConfig = piece.get(1);
      if (!positionConfig) return std::unexpected(std::move(positionConfig.error()));
      auto position = jsonToVec2F(*positionConfig);
      if (!position) return std::unexpected(std::move(position.error()));
      match->resultingPieces.push_back({renderPiece->get(), *position});
    }
    auto subMatches = config.get("subMatches", {});
    if (!subMatches) return std::unexpected(std::move(subMatches.error()));
    if (subMatches->isType(Json::Type::String)) {
      auto name = subMatches->toString();
      if (!name) return std::unexpected(std::move(name.error()));
      auto list = matchMap.get(*name);
      if (!list) return std::unexpected(std::move(list.error()));
      match->subMatches = list->get();
    } else if (!subMatches->isNull()) {
      {
        auto value = parseMaterialRenderMatchList(*subMatches, ruleMap, pieceMap, matchMap);
        if (!value) return std::unexpected(std::move(value.error()));
        match->subMatches = std::move(*value);
      }
    }
    auto layer = config.optString("requiredLayer");
    if (!layer) return std::unexpected(std::move(layer.error()));
    if (*layer) {
      auto value = enumValue<TileLayerNames>(**layer);
      if (!value) return std::unexpected(std::move(value.error()));
      match->requiredLayer = *value;
    }
    {
      auto value = config.getBool("haltOnMatch", false);
      if (!value) return std::unexpected(std::move(value.error()));
      match->haltOnMatch = std::move(*value);
    }
    {
      auto value = config.getBool("haltOnSubMatch", false);
      if (!value) return std::unexpected(std::move(value.error()));
      match->haltOnSubMatch = std::move(*value);
    }
    {
      auto value = config.optBool("occlude");
      if (!value) return std::unexpected(std::move(value.error()));
      match->occlude = std::move(*value);
    }
    result.push_back(std::move(match));
  }
  return result;
}

std::expected<String, StarException> MaterialRenderProfile::pieceImage(String const& pieceName, unsigned variant, MaterialColorVariant colorVariant, MaterialHue hueShift) const {
  auto piece = pieces.get(pieceName);
  if (!piece) return std::unexpected(std::move(piece.error()));
  String texture = piece->get()->texture;
  if (hueShift != MaterialHue()) texture = strf("{}?hueshift={}", texture, materialHueToDegrees(hueShift));
  auto variants = piece->get()->variants.get(colorVariant);
  if (!variants) return std::unexpected(std::move(variants.error()));
  auto rect = variants->get().wrap(variant);
  if (!rect) return std::unexpected(std::move(rect.error()));
  return strf("{}?crop={};{};{};{}", texture, rect->get().xMin(), rect->get().yMin(), rect->get().xMax(), rect->get().yMax());
}

std::expected<std::reference_wrapper<std::pair<String, Vec2F> const>, StarException> MaterialRenderProfile::damageImage(float damageLevel, TileDamageType damageType) const {
  if (damageType == TileDamageType::Protected)
    return protectedFrames.at(std::clamp<unsigned>(damageLevel * crackingFrames.size(), 0, crackingFrames.size() - 1));
  return crackingFrames.at(std::clamp<unsigned>(damageLevel * crackingFrames.size(), 0, crackingFrames.size() - 1));
}

std::expected<MaterialRenderProfile, StarException> parseMaterialRenderProfile(ImageMetadataDatabase const& imageMetadata, Json const& spec, String const& relativePath) {
  MaterialRenderProfile profile;
  auto transparent = spec.getBool("lightTransparent", false);
  if (!transparent) return std::unexpected(std::move(transparent.error()));
  {
    auto value = spec.getBool("foregroundLightTransparent", *transparent);
    if (!value) return std::unexpected(std::move(value.error()));
    profile.foregroundLightTransparent = std::move(*value);
  }
  {
    auto value = spec.getBool("backgroundLightTransparent", *transparent);
    if (!value) return std::unexpected(std::move(value.error()));
    profile.backgroundLightTransparent = std::move(*value);
  }
  auto multiColored = spec.getBool("multiColored", false);
  if (!multiColored) return std::unexpected(std::move(multiColored.error()));
  profile.colorVariants = 0;
  if (*multiColored) {
    {
      auto value = spec.getUInt("colorVariants", static_cast<std::uint64_t>(MaxMaterialColorVariant) + 1);
      if (!value) return std::unexpected(std::move(value.error()));
      profile.colorVariants = std::move(*value);
    }
  }
  auto directives = spec.getArray("colorDirectives", JsonArray());
  if (!directives) return std::unexpected(std::move(directives.error()));
  for (auto const& entry : *directives) {
    auto text = entry.toString();
    if (!text) return std::unexpected(std::move(text.error()));
    profile.colorDirectives.push_back(Directives(*text));
  }
  {
    auto value = spec.getBool("occludesBelow", true);
    if (!value) return std::unexpected(std::move(value.error()));
    profile.occludesBehind = std::move(*value);
  }
  {
    auto value = spec.getUInt("zLevel", 0);
    if (!value) return std::unexpected(std::move(value.error()));
    profile.zLevel = std::move(*value);
  }
  auto lightConfig = spec.get("radiantLight", JsonArray{0, 0, 0});
  if (!lightConfig) return std::unexpected(std::move(lightConfig.error()));
  auto light = jsonToVec3B(*lightConfig);
  if (!light) return std::unexpected(std::move(light.error()));
  profile.radiantLight = Color::rgb(*light).toRgbF();
  {
    auto value = spec.getString("representativePiece");
    if (!value) return std::unexpected(std::move(value.error()));
    profile.representativePiece = std::move(*value);
  }
  auto rulesConfig = spec.get("rules");
  if (!rulesConfig) return std::unexpected(std::move(rulesConfig.error()));
  auto rules = rulesConfig->iterateObject();
  if (!rules) return std::unexpected(std::move(rules.error()));
  for (auto const& pair : *rules) {
    auto rule = std::make_shared<MaterialRule>();
    auto joinName = pair.second.getString("join", "all");
    if (!joinName) return std::unexpected(std::move(joinName.error()));
    auto join = enumValue<MaterialJoinTypeNames>(*joinName);
    if (!join) return std::unexpected(std::move(join.error()));
    rule->join = *join;
    auto entries = pair.second.getArray("entries", {});
    if (!entries) return std::unexpected(std::move(entries.error()));
    for (auto const& entryConfig : *entries) {
      auto inverse = entryConfig.getBool("inverse", false);
      if (!inverse) return std::unexpected(std::move(inverse.error()));
      auto type = entryConfig.getString("type");
      if (!type) return std::unexpected(std::move(type.error()));
      if (auto match0 = type->equals_ignore_case("Connects"); !match0) return std::unexpected(std::move(match0.error()));
      else if (*match0) {
        rule->entries.push_back({MaterialRule::RuleConnects(), *inverse});
      }
      else if (auto match1 = type->equals_ignore_case("Shadows"); !match1) return std::unexpected(std::move(match1.error()));
      else if (*match1) {
        rule->entries.push_back({MaterialRule::RuleShadows(), *inverse});
      }
      else if (auto match2 = type->equals_ignore_case("EqualsSelf"); !match2) return std::unexpected(std::move(match2.error()));
      else if (*match2) {
        auto matchHue = entryConfig.getBool("matchHue", false);
        if (!matchHue) return std::unexpected(std::move(matchHue.error()));
        MaterialRule::RuleEntry entry{{}, *inverse};
        entry.rule.emplace<4>(MaterialRule::RuleEqualsSelf{*matchHue});
        rule->entries.push_back(std::move(entry));
      }
      else if (auto match3 = type->equals_ignore_case("EqualsId"); !match3) return std::unexpected(std::move(match3.error()));
      else if (*match3) {
        auto id = entryConfig.getUInt("id");
        if (!id) return std::unexpected(std::move(id.error()));
        rule->entries.push_back({MaterialRule::RuleEqualsId{static_cast<std::uint16_t>(*id)}, *inverse});
      }
      else if (auto match4 = type->equals_ignore_case("PropertyEquals"); !match4) return std::unexpected(std::move(match4.error()));
      else if (*match4) {
        auto name = entryConfig.getString("propertyName");
        if (!name) return std::unexpected(std::move(name.error()));
        auto value = entryConfig.get("propertyValue");
        if (!value) return std::unexpected(std::move(value.error()));
        rule->entries.push_back({MaterialRule::RulePropertyEquals{std::move(*name), std::move(*value)}, *inverse});
      }
    }
    profile.rules[pair.first] = std::move(rule);
  }
  auto piecesConfig = spec.get("pieces");
  if (!piecesConfig) return std::unexpected(std::move(piecesConfig.error()));
  auto pieces = piecesConfig->iterateObject();
  if (!pieces) return std::unexpected(std::move(pieces.error()));
  for (auto const& pair : *pieces) {
    auto piece = std::make_shared<MaterialRenderPiece>();
    piece->pieceId = profile.pieces.size();
    auto defaultTexture = spec.getString("texture");
    if (!defaultTexture) return std::unexpected(std::move(defaultTexture.error()));
    auto texture = pair.second.getString("texture", *defaultTexture);
    if (!texture) return std::unexpected(std::move(texture.error()));
    piece->texture = AssetPath::relative_to(relativePath, *texture);
    auto defaultVariants = spec.getUInt("variants", 1);
    if (!defaultVariants) return std::unexpected(std::move(defaultVariants.error()));
    auto variants = pair.second.getUInt("variants", *defaultVariants);
    if (!variants) return std::unexpected(std::move(variants.error()));
    auto sizeConfig = pair.second.get("textureSize");
    if (!sizeConfig) return std::unexpected(std::move(sizeConfig.error()));
    auto size = jsonToVec2F(*sizeConfig);
    if (!size) return std::unexpected(std::move(size.error()));
    auto positionConfig = pair.second.get("texturePosition");
    if (!positionConfig) return std::unexpected(std::move(positionConfig.error()));
    auto position = jsonToVec2F(*positionConfig);
    if (!position) return std::unexpected(std::move(position.error()));
    auto variantStrideConfig = pair.second.get("variantStride", JsonArray{0, 0});
    if (!variantStrideConfig) return std::unexpected(std::move(variantStrideConfig.error()));
    auto variantStride = jsonToVec2F(*variantStrideConfig);
    if (!variantStride) return std::unexpected(std::move(variantStride.error()));
    auto colorStrideConfig = pair.second.get("colorStride", JsonArray{0, 0});
    if (!colorStrideConfig) return std::unexpected(std::move(colorStrideConfig.error()));
    auto colorStride = jsonToVec2F(*colorStrideConfig);
    if (!colorStride) return std::unexpected(std::move(colorStride.error()));
    auto imageSize = imageMetadata.imageSize(piece->texture);
    if (!imageSize) return std::unexpected(std::move(imageSize.error()));
    auto imageHeight = (*imageSize)[1];
    auto flip = [imageHeight](RectF const& rect) { return RectF::withSize(Vec2F(rect.xMin(), imageHeight - rect.yMax()), rect.size()); };
    for (unsigned v = 0; v < *variants; ++v) {
      auto i = DefaultMaterialColorVariant;
      auto rect = RectF::withSize(*position + *variantStride * v, *size);
      piece->variants[i].push_back(flip(rect));
      for (MaterialColorVariant c = 0; c != profile.colorVariants; ++c) {
        auto rect = RectF::withSize(*position + *variantStride * v + *colorStride * ++i, *size);
        piece->variants[i].push_back(flip(rect));
      }
    }
    profile.pieces[pair.first] = std::move(piece);
  }
  auto matchesConfig = spec.get("matches");
  if (!matchesConfig) return std::unexpected(std::move(matchesConfig.error()));
  auto matches = matchesConfig->iterateArray();
  if (!matches) return std::unexpected(std::move(matches.error()));
  for (auto const& entry : *matches) {
    auto name = entry.getString(0);
    if (!name) return std::unexpected(std::move(name.error()));
    auto config = entry.get(1);
    if (!config) return std::unexpected(std::move(config.error()));
    {
      auto value = parseMaterialRenderMatchList(*config, profile.rules, profile.pieces, profile.matches);
      if (!value) return std::unexpected(std::move(value.error()));
      profile.matches[*name] = std::move(*value);
    }
  }
  auto main = profile.matches.get("main");
  if (!main) return std::unexpected(std::move(main.error()));
  profile.mainMatchList = main->get();

  // TODO: Completely hard-coded for now
  profile.crackingFrames = {
    {"/tiles/blockdamage.png:1", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:2", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:3", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:4", Vec2F(0, 0)},
    {"/tiles/blockdamage.png:5", Vec2F(0, 0)}
  };

  profile.protectedFrames = {
    {"/tiles/blockprotection.png:1", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:2", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:3", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:4", Vec2F(0, 0)},
    {"/tiles/blockprotection.png:5", Vec2F(0, 0)}
  };

  {
    auto value = spec.get("ruleProperties", JsonObject());
    if (!value) return std::unexpected(std::move(value.error()));
    profile.ruleProperties = std::move(*value);
  }

  return profile;
}

}
