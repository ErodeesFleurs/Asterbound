#include "gtest/gtest.h"
import std;
import star.format;
import star.game_test;
import star.root;
import star.configuration;
import star.logging;

namespace Star {
namespace {

class ErrorLogSink : public LogSink {
public:
  ErrorLogSink() { set_level(LogLevel::Error); }
  [[nodiscard]] std::expected<void, StarException> log(std::string_view message, LogLevel) override {
    ADD_FAILURE() << "Error was logged: " << message;
    return {};
  }
};

std::vector<std::reference_wrapper<GameTestRegistration const>>& gameTestRegistrations() {
  static std::vector<std::reference_wrapper<GameTestRegistration const>> registrations;
  return registrations;
}

}

GameTestEnvironment::GameTestEnvironment(Root::Settings settings)
  : m_settings(std::move(settings)) {}

void GameTestEnvironment::SetUp() {
  auto root = Root::create(m_settings);
  ASSERT_TRUE(root) << root.error().what();
  m_root = std::move(*root);
  m_root->logger().add_sink(std::make_shared<ErrorLogSink>());
  auto configuration = m_root->configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  auto clearUniverse = (*configuration)->set("clearUniverseFiles", true);
  ASSERT_TRUE(clearUniverse) << clearUniverse.error().what();
  auto clearPlayer = (*configuration)->set("clearPlayerFiles", true);
  ASSERT_TRUE(clearPlayer) << clearPlayer.error().what();
  m_ready = true;
}

void GameTestEnvironment::TearDown() {
  m_root.reset();
  m_ready = false;
}

GameTest::GameTest(GameTestEnvironment& environment, Body body)
  : m_environment(environment), m_body(body) {}

Root& GameTest::root() const {
  return *m_environment.m_root;
}

Root::Settings const& GameTest::rootSettings() const {
  return m_environment.m_settings;
}

void GameTest::TestBody() {
  if (!m_environment.m_ready)
    GTEST_SKIP() << "Game test environment setup failed";
  m_body(*this);
}

GameTestRegistration::GameTestRegistration(std::string_view suite, std::string_view name,
    GameTest::Body body, std::source_location location)
  : m_suite(suite), m_name(name), m_body(body), m_location(location) {
  gameTestRegistrations().push_back(std::cref(*this));
}

void GameTestRegistration::bind(GameTestEnvironment& environment) const {
  testing::RegisterTest(m_suite.data(), m_name.data(), nullptr, nullptr, m_location.file_name(),
      static_cast<int>(m_location.line()), [&environment, body = m_body]() {
        return new GameTest(environment, body);
      });
}

void registerGameTests(GameTestEnvironment& environment) {
  for (auto const& registration : gameTestRegistrations())
    registration.get().bind(environment);
}

}
