import std;
import star.byte_array;
import star.celestial_coordinate;
import star.context_net_state;
import star.damage_types;
import star.data_stream;
import star.data_stream_devices;
import star.data_stream_extra;
import star.json;
import star.net_compatibility;
import star.player_types;
import star.string;
import star.uuid;
import star.vector;
import star.warping;
import star.world_storage;
#include "gtest/gtest.h"

using namespace Star;

static_assert(!std::is_move_constructible_v<ContextNetState> && !std::is_copy_constructible_v<ContextNetState>);

TEST(ContextLayout, ShipUpgradeKeysAndWireOrder) {
  Json document = JsonObject{{"shipLevel", 2}, {"maxFuel", 3000}, {"crewSize", 5},
      {"fuelEfficiency", 1.5}, {"shipSpeed", 42}, {"capabilities", JsonArray{"teleport"}}};
  auto upgrades = ShipUpgrades::create(document);
  ASSERT_TRUE(upgrades);
  auto round_trip = ShipUpgrades::create(upgrades->to_json());
  ASSERT_TRUE(round_trip);
  EXPECT_EQ(*round_trip, *upgrades);

  DataStreamBuffer legacy;
  ASSERT_TRUE(legacy << 2u << 3000u << 5u << 1.5f << 42.0f << StringSet{"teleport"});
  auto encoded = DataStreamBuffer::serialize(*upgrades);
  ASSERT_TRUE(encoded);
  EXPECT_EQ(*encoded, legacy.data());

  ShipUpgrades previous;
  previous.ship_level = 9;
  previous.capabilities.insert("old");
  auto decoded = previous;
  for (std::size_t size = 0; size < legacy.size(); ++size) {
    DataStreamReader reader(legacy.span().first(size));
    EXPECT_FALSE(reader.read(decoded)) << size;
    EXPECT_EQ(decoded, previous) << size;
  }
  ASSERT_TRUE(DataStreamBuffer::deserialize(decoded, legacy.span()));
  EXPECT_EQ(decoded, *upgrades);
  EXPECT_FALSE(decoded.apply(JsonObject{{"shipLevel", 99}, {"capabilities", JsonArray{false}}}));
  EXPECT_EQ(decoded, *upgrades);
}

TEST(ContextLayout, FullRecordMatchesLegacySixFieldOrder) {
  for (auto version : {14u, 15u}) {
    NetCompatibilityRules rules(version);
    ContextNetState sender, scratch, receiver;
    ASSERT_TRUE(sender.initialize());
    ASSERT_TRUE(scratch.initialize());
    std::optional<std::pair<WarpAction, WarpMode>> orbit(std::in_place, WarpAlias::OwnShip, WarpMode::DeployOnly);
    WorldId world = InstanceWorldId("mission");
    EntityDamageTeam team(TeamType::PVP, 23);
    ShipUpgrades upgrades;
    upgrades.max_fuel = 112;
    upgrades.capabilities.insert("teleport");
    CelestialCoordinate coordinate(Vec3I(1, 2, 3), 4, 5);
    sender.orbit_warp_action.set(orbit);
    sender.player_world_id.set(world);
    sender.is_admin.set(true);
    sender.team.set(team);
    sender.ship_upgrades.set(upgrades);
    sender.ship_coordinate.set(coordinate);

    DataStreamBuffer legacy;
    legacy.set_stream_compatibility_version(rules);
    ASSERT_TRUE(legacy << true << orbit << world << true << team << upgrades << coordinate);
    auto record = sender.group.write_net_state(0, rules);
    ASSERT_TRUE(record);
    EXPECT_EQ(record->first, legacy.data());
    ASSERT_TRUE(scratch.stage_update(legacy.span(), rules));
    EXPECT_FALSE(receiver.is_admin.get());
    scratch.publish_to(receiver);
    EXPECT_EQ(receiver.orbit_warp_action.get(), orbit);
    EXPECT_EQ(receiver.player_world_id.get(), world);
    EXPECT_TRUE(receiver.is_admin.get());
    EXPECT_EQ(receiver.team.get(), team);
    EXPECT_EQ(receiver.ship_upgrades.get(), upgrades);
    EXPECT_EQ(receiver.ship_coordinate.get(), coordinate);
  }
}

TEST(ContextLayout, RejectedAndAbandonedFieldsDoNotLeakIntoSparseDelta) {
  ContextNetState scratch, receiver;
  ASSERT_TRUE(scratch.initialize());
  receiver.ship_coordinate.set(CelestialCoordinate(Vec3I(-1, -2, -3)));
  DataStreamBuffer admin;
  ASSERT_TRUE(admin << false);
  ASSERT_TRUE(admin.write_vlq_u(3));
  ASSERT_TRUE(admin << true);
  ASSERT_TRUE(admin.write_vlq_u(0));

  DataStreamBuffer upgrade;
  ShipUpgrades upgrades;
  upgrades.capabilities.insert("teleport");
  ASSERT_TRUE(upgrade << false);
  ASSERT_TRUE(upgrade.write_vlq_u(5));
  ASSERT_TRUE(upgrade << upgrades);
  ASSERT_TRUE(upgrade.write_vlq_u(0));

  // A valid but unpublished delta, a late failure and trailing bytes must
  // each leave the live receiver untouched on the next successful delta.
  for (auto extra : {0, -1, 1}) {
    auto bytes = admin.data();
    if (extra < 0)
      bytes.remove_suffix(1);
    if (extra > 0)
      bytes.push_back('x');
    auto staged = scratch.stage_update(bytes.span(), {});
    ASSERT_EQ(staged.has_value(), extra == 0);
    if (!staged)
      scratch.publish_to(receiver);
    ASSERT_TRUE(scratch.stage_update(upgrade.span(), {}));
    scratch.publish_to(receiver);
    EXPECT_FALSE(receiver.is_admin.get());
    EXPECT_EQ(receiver.ship_upgrades.get(), upgrades);
    EXPECT_EQ(receiver.ship_coordinate.get(), CelestialCoordinate(Vec3I(-1, -2, -3)));
  }
}

TEST(ContextLayout, ChunkDifferenceDistinguishesMissingDeletedAndEmpty) {
  auto key = ByteArray::from_string("sector");
  auto data = ByteArray::from_string("contents");
  std::array<WorldChunks, 4> snapshots{{{}, {{key, std::nullopt}}, {{key, ByteArray{}}}, {{key, data}}}};
  // Rows are the old key, columns the new key: absent, tombstone, empty, data.
  constexpr bool emits[4][4] = {
      {false, false, true, true}, {true, false, true, true},
      {true, true, false, true}, {true, true, true, false}};
  for (std::size_t before = 0; before < snapshots.size(); ++before) {
    for (std::size_t after = 0; after < snapshots.size(); ++after) {
      auto delta = WorldStorage::get_world_chunks_update(snapshots[before], snapshots[after]);
      ASSERT_EQ(delta.contains(key), emits[before][after]) << before << " -> " << after;
      if (emits[before][after]) {
        auto it = delta.find(key);
        if (after < 2)
          EXPECT_FALSE(it->second);
        else
          EXPECT_EQ(it->second, snapshots[after].find(key)->second);
      }
    }
  }
  WorldChunks pending{{ByteArray::from_string("unchanged"), data}};
  ASSERT_TRUE(WorldStorage::accumulate_world_chunks_update(pending, snapshots[0], snapshots[3]));
  ASSERT_TRUE(WorldStorage::accumulate_world_chunks_update(pending, snapshots[3], snapshots[0]));
  EXPECT_FALSE(WorldStorage::accumulate_world_chunks_update(pending, snapshots[0], snapshots[1]));
  WorldChunks expected{{ByteArray::from_string("unchanged"), data}, {key, std::nullopt}};
  EXPECT_EQ(pending, expected);
}
