module;
#include "gtest/gtest.h"
import std;
import star.root;

namespace Star {

class GameTest;

class GameTestEnvironment : public testing::Environment {
public:
  explicit GameTestEnvironment(Root::Settings settings);
  void SetUp() override;
  void TearDown() override;

private:
  friend class GameTest;
  Root::Settings m_settings;
  std::unique_ptr<Root> m_root;
  bool m_ready = false;
};

class GameTest : public testing::Test {
public:
  using Body = void (*)(GameTest&);
  GameTest(GameTestEnvironment& environment, Body body);
  Root& root() const;
  Root::Settings const& rootSettings() const;
  static bool check(bool passed, std::string_view message, std::source_location at = std::source_location::current()) {
    if (!passed)
      testing::internal::AssertHelper(testing::TestPartResult::kNonFatalFailure, at.file_name(), static_cast<int>(at.line()), "")
          = testing::Message() << message;
    return passed;
  }

private:
  void TestBody() override;
  GameTestEnvironment& m_environment;
  Body m_body;
};

// Static descriptors contain test metadata only. The executable explicitly binds
// its environment owner to each gtest factory before running any tests.
class GameTestRegistration {
public:
  GameTestRegistration(std::string_view suite, std::string_view name, GameTest::Body body,
      std::source_location location = std::source_location::current());
  void bind(GameTestEnvironment& environment) const;

private:
  std::string_view m_suite;
  std::string_view m_name;
  GameTest::Body m_body;
  std::source_location m_location;
};

void registerGameTests(GameTestEnvironment& environment);

}

export module star.game_test;
export namespace Star {
  using ::Star::GameTest;
  using ::Star::GameTestEnvironment;
  using ::Star::GameTestRegistration;
  using ::Star::registerGameTests;
}
