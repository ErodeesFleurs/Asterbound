#include "gtest/gtest.h"

import std;
import star.armors;
import star.asset_path;
import star.assets;
import star.casting;
import star.drawable;
import star.game_test;
import star.game_types;
import star.humanoid;
import star.item;
import star.item_database;
import star.item_descriptor;
import star.json;
import star.root;
import star.string;

using namespace Star;

const GameTestRegistration
  sleeveLighting("HumanoidRenderTest", "BackSleeveRetainsArmorLightingWhileHolding", [](GameTest& fixture) {
    auto assets = fixture.root().assets();
    ASSERT_TRUE(assets);
    auto config = (*assets)->json("/humanoid.config");
    ASSERT_TRUE(config);
    // Exercise the legacy humanoid path still used by species without an animator.
    auto legacy = config->eraseKey("animation");
    ASSERT_TRUE(legacy);
    auto humanoid = Humanoid::create(fixture.root(), *legacy);
    ASSERT_TRUE(humanoid) << humanoid.error().what();
    HumanoidIdentity identity;
    ASSERT_TRUE((*humanoid)->setIdentity(identity));
    auto database = fixture.root().itemDatabase();
    ASSERT_TRUE(database);
    auto names = (*database)->allItems();
    std::sort(names.begin(), names.end());
    String chestName;
    for (auto const& name : names) {
      auto type = (*database)->itemType(name);
      ASSERT_TRUE(type);
      if (*type != ItemType::ChestArmor)
        continue;
      auto descriptor = ItemDescriptor::create(name, 1);
      ASSERT_TRUE(descriptor);
      auto item = (*database)->item(*descriptor);
      ASSERT_TRUE(item);
      auto chest = asRef<ChestArmor>(**item);
      ASSERT_TRUE(chest);
      if (!chest->get().backSleeveFrameset(identity.gender).empty()) {
        chestName = name;
        break;
      }
    }
    ASSERT_FALSE(chestName.empty());
    for (bool fullbright : {false, true}) {
      auto descriptor = ItemDescriptor::create(chestName, 1, JsonObject{{"fullbright", fullbright}});
      ASSERT_TRUE(descriptor);
      auto item = (*database)->item(*descriptor);
      ASSERT_TRUE(item);
      auto chest = asRef<ChestArmor>(**item);
      ASSERT_TRUE(chest);
      ASSERT_EQ(chest->get().fullbright(), fullbright);
      ASSERT_TRUE((*humanoid)->setWearableFromChest(0, chest->get(), identity.gender));
      auto sleeve = AssetPath::split(chest->get().backSleeveFrameset(identity.gender)).basePath;
      for (auto direction : {Direction::Left, Direction::Right}) {
        (*humanoid)->setFacingDirection(direction);
        for (bool holding : {false, true}) {
          (*humanoid)->setHandParameters(ToolHand::Primary, holding, 0, 0, false, false, false);
          (*humanoid)->setHandParameters(ToolHand::Alt, holding, 0, 0, false, false, false);
          auto drawables = (*humanoid)->render();
          ASSERT_TRUE(drawables) << drawables.error().what();
          unsigned sleeves = 0;
          for (auto const& drawable : *drawables) {
            if (drawable.isImage() && drawable.imagePart().image.basePath == sleeve) {
              ++sleeves;
              EXPECT_EQ(drawable.fullbright, fullbright)
                << "holding=" << holding << ", armor fullbright=" << fullbright;
            }
          }
          EXPECT_EQ(sleeves, 1u);
        }
      }
    }
  });
