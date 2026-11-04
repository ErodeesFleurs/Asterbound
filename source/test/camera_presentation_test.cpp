import std;
import star.world_camera;
import star.world_geometry;
import star.vector;

#include "gtest/gtest.h"

using namespace Star;

TEST(CameraPresentationTest, RenderSamplingDoesNotBecomeTheNextLogicEndpoint) {
  WorldCamera camera;
  camera.setScreenSize({800, 600});
  camera.setWorldGeometry(WorldGeometry(Vec2U{1000, 1000}));
  camera.setCenterWorldPosition({100, 100});
  camera.beginUpdate();
  camera.setCenterWorldPosition({102, 100});
  camera.setRenderInterpolation(0.25f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(100.5f, 100));
  camera.setRenderInterpolation(0.75f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(101.5f, 100));
  camera.setRenderInterpolation(0.25f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(100.5f, 100));
  camera.beginUpdate();
  camera.setCenterWorldPosition({104, 100});
  camera.setRenderInterpolation(0.0f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(102, 100));
  camera.setRenderInterpolation(0.5f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(103, 100));
}

TEST(CameraPresentationTest, WrapAndDiscontinuityDoNotCrossTheWorld) {
  WorldCamera camera;
  camera.setScreenSize({8, 8});
  camera.setWorldGeometry(WorldGeometry(Vec2U{100, 100}));
  camera.setCenterWorldPosition({99, 5});
  camera.beginUpdate();
  camera.setCenterWorldPosition({1, 5});
  camera.setRenderInterpolation(0.5f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(0, 5));
  camera.setCenterWorldPosition({60, 40});
  camera.resetPresentation();
  camera.setRenderInterpolation(0.0f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(60, 40));
  camera.setRenderInterpolation(0.5f);
  EXPECT_EQ(camera.centerWorldPosition(), Vec2F(60, 40));
}

TEST(CameraPresentationTest, ZoomSamplingAndOddSizeHitTestingShareOneTransform) {
  WorldCamera camera;
  camera.setScreenSize({801, 601});
  camera.setWorldGeometry(WorldGeometry(Vec2U{1000, 1000}));
  camera.setCenterWorldPosition({100, 100});
  camera.beginUpdate();
  camera.setTargetPixelRatio(2.0f);
  camera.update(1.0f / 60.0f);
  camera.setRenderInterpolation(1.0f);
  float currentRatio = camera.pixelRatio();
  camera.setRenderInterpolation(0.5f);
  EXPECT_FLOAT_EQ(camera.pixelRatio(), std::lerp(1.0f, currentRatio, 0.5f));
  auto point = Vec2F(104.125f, 103.75f);
  auto restored = camera.screenToWorld(camera.worldToScreen(point));
  EXPECT_NEAR(restored[0], point[0], 0.0001f);
  EXPECT_NEAR(restored[1], point[1], 0.0001f);
  camera.setRenderInterpolation(0.0f);
  camera.beginUpdate();
  EXPECT_FLOAT_EQ(camera.pixelRatio(), currentRatio);
}

TEST(CameraPresentationTest, InputBetweenPresentsUsesTheLastShownTransform) {
  WorldCamera camera;
  camera.setScreenSize({800, 600});
  camera.setWorldGeometry(WorldGeometry(Vec2U{1000, 1000}));
  camera.setCenterWorldPosition({100, 100});
  auto displayed = camera;
  camera.beginUpdate();
  camera.setCenterWorldPosition({102, 100});
  camera.restoreRenderTransform(displayed);
  EXPECT_EQ(camera.screenToWorld({400, 300}), Vec2F(100, 100));
  camera.beginUpdate();
  camera.setCenterWorldPosition({104, 100});
  camera.restoreRenderTransform(displayed);
  EXPECT_EQ(camera.screenToWorld({400, 300}), Vec2F(100, 100));
  camera.setRenderInterpolation(0.5f);
  EXPECT_EQ(camera.screenToWorld({400, 300}), Vec2F(103, 100));
}
