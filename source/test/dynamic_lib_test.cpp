import std;
import star.dynamic_lib;
import star.string;

#include "gtest/gtest.h"

using namespace Star;

extern "C"
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int asterbound_test_executable_symbol() { return 37; }

namespace {
using Transform = int(int);
constexpr char transform_name[] = "asterbound_fixture_transform";

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
static_assert(DynamicLib::extension() == ".dll");
#elif defined(STAR_SYSTEM_MACOS)
static_assert(DynamicLib::extension() == ".dylib");
#else
static_assert(DynamicLib::extension() == ".so");
#endif

TEST(DynamicLibTest, SymbolOutlivesMovedAndDestroyedLibrary) {
  using Lookup = decltype(std::declval<DynamicLib const&>().find<Transform>(transform_name));
  std::optional<Lookup::value_type> retained;
  {
    auto library = DynamicLib::open(STAR_DYNAMIC_LIB_FIXTURE);
    ASSERT_TRUE(library.has_value()) << library.error().what();
    auto symbol = std::as_const(*library).find<Transform>(transform_name);
    ASSERT_TRUE(symbol.has_value()) << symbol.error().what();
    retained.emplace(std::move(*symbol));
    auto moved = std::move(*library);
    auto rejected = library->find<Transform>(transform_name);
    ASSERT_FALSE(rejected.has_value());
    EXPECT_NE("", rejected.error().what());
    auto valid = moved.find<Transform>(transform_name);
    ASSERT_TRUE(valid.has_value()) << valid.error().what();
    EXPECT_EQ((*valid)(4), 13);
  }
  EXPECT_EQ((*retained)(-3), -8);
}

TEST(DynamicLibTest, LastReferenceReleasesFixtureState) {
  for (int value : {23, 47}) {
    auto library = DynamicLib::open(STAR_DYNAMIC_LIB_FIXTURE);
    ASSERT_TRUE(library) << library.error().what();
    auto exchange = library->find<Transform>("asterbound_fixture_exchange");
    ASSERT_TRUE(exchange) << exchange.error().what();
    EXPECT_EQ((*exchange)(value), 0);
    EXPECT_EQ((*exchange)(value), value);
  }
}

TEST(DynamicLibTest, RejectsEmbeddedNulAndRecoversAfterLookupFailure) {
  String path(STAR_DYNAMIC_LIB_FIXTURE);
  auto invalid_path = path;
  invalid_path.append(std::string_view("\0tail", 5));
  EXPECT_FALSE(DynamicLib::open(invalid_path).has_value());
  EXPECT_FALSE(DynamicLib::open(path + ".missing").has_value());

  auto base = path.utf8();
  ASSERT_TRUE(base.ends_with(DynamicLib::extension()));
  base.resize(base.size() - DynamicLib::extension().size());
  auto library = DynamicLib::open_base(base);
  ASSERT_TRUE(library.has_value()) << library.error().what();
  String invalid_name(transform_name);
  invalid_name.append(std::string_view("\0tail", 5));
  EXPECT_FALSE(library->find<Transform>(invalid_name).has_value());
  EXPECT_FALSE(library->find<Transform>("asterbound_fixture_missing").has_value());
  auto symbol = library->find<Transform>(transform_name);
  ASSERT_TRUE(symbol.has_value()) << symbol.error().what();
  EXPECT_EQ((*symbol)(7), 22);
}

TEST(DynamicLibTest, CurrentExecutableRemainsAvailableAcrossHandles) {
  for (int i = 0; i != 2; ++i) {
    auto current = DynamicLib::open_current();
    ASSERT_TRUE(current) << current.error().what();
    auto symbol = current->find<int()>("asterbound_test_executable_symbol");
    ASSERT_TRUE(symbol) << symbol.error().what();
    EXPECT_EQ((*symbol)(), 37);
  }
}
} // namespace
