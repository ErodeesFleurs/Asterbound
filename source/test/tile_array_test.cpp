import star.rect;
import star.list;
import star.multi_array;
import star.set;
import star.vector;
import star.thread;
import star.worker_pool;

import std;
import star.format;
import star.sector_array_2d;

import star.worker_pool;
import star.tile_sector_array;

#include "gtest/gtest.h"

import star.array;
import star.game_test;
import star.root;
using namespace Star;

const GameTestRegistration tileSectorArrayAll("TileSectorArrayTest", "All", [](GameTest& fixture) {
  typedef TileSectorArray<int, 32> TileArray;
  TileArray tileSectorArray({100, 100}, -1);

  EXPECT_TRUE(tileSectorArray.sectorValid(TileArray::Sector(1, 1)));
  EXPECT_TRUE(tileSectorArray.sectorValid(TileArray::Sector(3, 3)));
  EXPECT_FALSE(tileSectorArray.sectorValid(TileArray::Sector(4, 4)));

  auto validSectors = tileSectorArray.validSectorsFor(RectI(0, -32, 64, 32));
  ASSERT_TRUE(validSectors);
  EXPECT_EQ(*validSectors, List<TileArray::Sector>({{0, 0}, {1, 0}}));

  EXPECT_TRUE(TileArray::Sector(0, 0) == tileSectorArray.sectorFor({0, 0}));
  EXPECT_TRUE(TileArray::Sector(3, 1) == tileSectorArray.sectorFor({-1, 33}));
  EXPECT_TRUE(-1 == tileSectorArray.tile({-1, -1}));
  EXPECT_FALSE(tileSectorArray.modifyTile({-1, -1}));
  EXPECT_TRUE(RectI(32, 32, 64, 64) == tileSectorArray.sectorRegion({1, 1}));
  EXPECT_TRUE(TileArray::Sector(0, 3) == tileSectorArray.adjacentSector({3, 3}, {1, 0}));

  tileSectorArray.loadSector({0, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.loadSector({1, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.loadSector({2, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.loadSector({3, 0}, std::make_unique<TileArray::Array>(1));
  tileSectorArray.loadSector({0, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.loadSector({1, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.loadSector({2, 1}, std::make_unique<TileArray::Array>(2));
  tileSectorArray.loadSector({3, 1}, std::make_unique<TileArray::Array>(2));

  Set<Vec2I> found;
  ASSERT_TRUE(tileSectorArray.tileEach(RectI(-2, 0, 3, 1),
      [&found](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        found.add(pos);
        EXPECT_TRUE(pos[0] >= -2 && pos[0] < 3);
        EXPECT_TRUE(pos[1] == 0);
        EXPECT_EQ(1, tile);
        return {};
      }));
  EXPECT_TRUE(found.contains(Vec2I(0, 0)));
  EXPECT_TRUE(found.contains(Vec2I(-1, 0)));
  EXPECT_TRUE(found.contains(Vec2I(-2, 0)));
  EXPECT_TRUE(found.contains(Vec2I(1, 0)));

  ASSERT_TRUE(tileSectorArray.tileEach(RectI(-10, 0, -1, 1),
      [](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        EXPECT_TRUE(pos[0] >= -10 && pos[0] < -1);
        EXPECT_EQ(1, tile);
        return {};
      }));

  ASSERT_TRUE(tileSectorArray.tileEach(RectI(-10, -1, -1, 0),
      [](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        EXPECT_TRUE(pos[0] >= -10 && pos[0] < -1);
        EXPECT_TRUE(pos[1] == -1);
        EXPECT_EQ(-1, tile);
        return {};
      }));

  found.clear();
  ASSERT_TRUE(tileSectorArray.tileEach(RectI(110, 101, 120, 102),
      [&found](Vec2I const& pos, int tile) -> std::expected<void, StarException> {
        found.add(pos);
        EXPECT_TRUE(pos[0] >= 110 && pos[0] < 120);
        EXPECT_TRUE(pos[1] == 101);
        EXPECT_EQ(-1, tile);
        return {};
      }));
  EXPECT_TRUE(found.contains(Vec2I(110, 101)));
  EXPECT_TRUE(found.contains(Vec2I(119, 101)));

  auto workers = fixture.root().workers();
  ASSERT_TRUE(workers);
  auto res1 = tileSectorArray.tileEachResult(workers->get(), RectI(110, 110, 120, 120),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        return (pos[0] >= 110 && pos[0] < 120 && pos[1] >= 110 && pos[1] < 120 && tile == -1) ? 1 : 0;
      });
  ASSERT_TRUE(res1);
  MultiArray<int, 2> res1comp({10, 10}, 1);

  EXPECT_TRUE(res1->extents() == res1comp.extents());
  res1->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });

  auto res2 = tileSectorArray.tileEachResult(workers->get(), RectI(32, 32, 64, 64),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        return (pos[0] >= 32 && pos[0] < 64 && pos[1] >= 32 && pos[1] < 64 && tile == 2) ? 1 : 0;
      });
  ASSERT_TRUE(res2);
  MultiArray<int, 2> res2comp({32, 32}, 1);

  EXPECT_TRUE(res2->extents() == res2comp.extents());
  res2->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });

  auto res3 = tileSectorArray.tileEachResult(workers->get(), RectI(-10, -10, 1, 1),
      [](Vec2I const& pos, int tile) -> std::expected<int, StarException> {
        if (pos[1] < 0)
          return tile == -1;
        else
          return tile == 1;
      });
  ASSERT_TRUE(res3);
  MultiArray<int, 2> res3comp({11, 11}, 1);

  EXPECT_TRUE(res3->extents() == res3comp.extents());
  res3->for_each([](Array2S const&, int elem) { EXPECT_TRUE(elem == 1); });
});
