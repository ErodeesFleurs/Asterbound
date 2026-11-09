#include <cstdio>
import star.logging;
import std;
import star.format;
import star.root_loader;
import star.game_test;
import star.native_runtime;

#include "gtest/gtest.h"

using namespace Star;

GTEST_API_ int main(int argc, char** argv) {
  auto nativeRuntime = Star::NativeRuntime::create();
  if (!nativeRuntime) {
    if (!Star::print(stderr, "{}\n", nativeRuntime.error().what()))
      return 1;
    return 1;
  }
  testing::InitGoogleTest(&argc, argv);
  auto loader = RootLoader::create({{}, {}, {}, LogLevel::Error, true, {}});
  if (!loader) {
    if (!Star::print(stderr, "{}\n", loader.error().what()))
      return 1;
    return 1;
  }
  auto parsed = loader->commandParseOrDie(argc, argv);
  if (!parsed) {
    if (!Star::print(stderr, "{}\n", parsed.error().what()))
      return 1;
    return 1;
  }
  auto environment = std::make_unique<GameTestEnvironment>(std::move(parsed->first));
  auto& environmentRef = *environment;
  testing::AddGlobalTestEnvironment(environment.release());
  registerGameTests(environmentRef);
  return RUN_ALL_TESTS();
}
