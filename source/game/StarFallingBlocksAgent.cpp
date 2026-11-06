import star.format;
import star.xx_hash;
import star.json;
import star.vector;
import star.set;
import star.map;
import star.random;
import star.game_types;
import star.list;
import star.bi_map;
import std;
import star.rect;
import star.thread;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.weighted_pool;
import star.color;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;
import star.version;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.asset_source;
import star.assets;

import star.configuration;
import star.root;

import star.falling_blocks_agent;

import star.algorithm;
namespace Star {

FallingBlocksAgent::FallingBlocksAgent(Root& root, std::shared_ptr<FallingBlocksFacade> worldFacade)
  : m_facade(std::move(worldFacade)), m_random(root.random().randu64()) {
}

std::expected<std::shared_ptr<FallingBlocksAgent>, StarException> FallingBlocksAgent::create(Root& root, std::shared_ptr<FallingBlocksFacade> worldFacade) {
  auto result = std::shared_ptr<FallingBlocksAgent>(new FallingBlocksAgent(root, std::move(worldFacade)));
  auto assets = root.assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/worldserver.config:fallingBlocksImmediateUpwardPropogateProbability");
  if (!config) return std::unexpected(std::move(config.error()));
  auto probability = config->toFloat();
  if (!probability) return std::unexpected(std::move(probability.error()));
  result->m_immediateUpwardPropagateProbability = *probability;
  return result;
}

std::expected<void, StarException> FallingBlocksAgent::update() {
  HashSet<Vec2I> processing = std::exchange(m_pending, {});

  while (!processing.empty()) {
    List<Vec2I> positions;
    for (auto const& pos : std::exchange(processing, {}))
      positions.append(pos);

    m_random.shuffle(positions);

    positions.sort([](auto const& a, auto const& b) {
        return a[1] < b[1];
      });

    for (auto const& pos : positions) {
      Vec2I belowPos = pos + Vec2I(0, -1);
      Vec2I belowLeftPos = pos + Vec2I(-1, -1);
      Vec2I belowRightPos = pos + Vec2I(1, -1);

      auto thisBlockResult = m_facade->blockType(pos);


      if (!thisBlockResult) return std::unexpected(std::move(thisBlockResult.error()));


      FallingBlockType thisBlock = *thisBlockResult;
      auto belowBlockResult = m_facade->blockType(belowPos);

      if (!belowBlockResult) return std::unexpected(std::move(belowBlockResult.error()));

      FallingBlockType belowBlock = *belowBlockResult;

      std::optional<Vec2I> moveTo;

      if (thisBlock == FallingBlockType::Falling) {
        if (belowBlock == FallingBlockType::Open)
          moveTo = belowPos;
      } else if (thisBlock == FallingBlockType::Cascading) {
        if (belowBlock == FallingBlockType::Open) {
          moveTo = belowPos;
        } else {
          auto belowLeftBlockResult = m_facade->blockType(belowLeftPos);

          if (!belowLeftBlockResult) return std::unexpected(std::move(belowLeftBlockResult.error()));

          FallingBlockType belowLeftBlock = *belowLeftBlockResult;
          auto belowRightBlockResult = m_facade->blockType(belowRightPos);

          if (!belowRightBlockResult) return std::unexpected(std::move(belowRightBlockResult.error()));

          FallingBlockType belowRightBlock = *belowRightBlockResult;

          if (belowLeftBlock == FallingBlockType::Open && belowRightBlock == FallingBlockType::Open)
            moveTo = m_random.randb() ? belowLeftPos : belowRightPos;
          else if (belowLeftBlock == FallingBlockType::Open)
            moveTo = belowLeftPos;
          else if (belowRightBlock == FallingBlockType::Open)
            moveTo = belowRightPos;
        }
      }

      if (moveTo) {
        if (auto r = m_facade->moveBlock(pos, *moveTo); !r) return std::unexpected(std::move(r.error()));
        if (m_random.randf() < m_immediateUpwardPropagateProbability) {
          processing.add(pos + Vec2I(0, 1));
          processing.add(pos + Vec2I(-1, 1));
          processing.add(pos + Vec2I(1, 1));
        }

        visitLocation(pos);
        visitLocation(*moveTo);
      }
    }
  }
  return {};
}

void FallingBlocksAgent::visitLocation(Vec2I const& location) {
  visitRegion(RectI::withSize(location, Vec2I(1, 1)));
}

void FallingBlocksAgent::visitRegion(RectI const& region) {
  for (int x = region.xMin() - 1; x <= region.xMax(); ++x) {
    for (int y = region.yMin(); y <= region.yMax(); ++y) 
      m_pending.add({x, y});
  }
}

}
