module;
import std;
import star.string;
import star.vector;
import star.list;
import star.drawable;
import star.warping;
import star.format;

namespace Star {

struct TestUniverseExceptionTag {
  static constexpr std::string_view name() { return "TestUniverseException"; }
};
using TestUniverseException = StarError<TestUniverseExceptionTag, StarException>;
struct TestUniverseTimeoutExceptionTag {
  static constexpr std::string_view name() { return "TestUniverseTimeoutException"; }
};
using TestUniverseTimeoutException = StarError<TestUniverseTimeoutExceptionTag, TestUniverseException>;

class UniverseServer;
class UniverseClient;
class Player;
class Root;
class WorldClient;

class TestUniverse {
public:
  struct ReadyWorld {
    List<Drawable> drawables;
    std::uint64_t firstServerStep;
    std::uint64_t readyServerStep;
    std::size_t observedScriptedNpcs;
    std::size_t observedCoordinators;
  };

  // Publishes only a fully connected fixture; failed setup tears down its
  // partially created client, server, and temporary storage.
  [[nodiscard]] static std::expected<std::unique_ptr<TestUniverse>, StarException> create(Root& root, Vec2U clientWindowSize);
  ~TestUniverse();

  [[nodiscard]] std::expected<void, StarException> warpPlayer(WorldId worldId);
  WorldId currentPlayerWorld() const;

  [[nodiscard]] std::expected<void, StarException> update(unsigned times = 1);
  std::shared_ptr<WorldClient> worldClient() const;

  [[nodiscard]] std::expected<List<Drawable>, StarException> currentClientDrawables();
  // Wait for active world sectors and observed NPC/coordinator script schedules,
  // then acknowledge a real post-update world message before rendering.
  [[nodiscard]] std::expected<ReadyWorld, StarException> waitForWorldReady(std::int64_t timeoutMilliseconds = 60000);
  // Call before releasing the fixture to assert teardown failures. Destruction
  // still completes cleanup, reporting otherwise unobserved errors urgently.
  [[nodiscard]] std::expected<void, StarException> close();

private:
  explicit TestUniverse(Vec2U clientWindowSize);
  Vec2U m_clientWindowSize;
  String m_storagePath;
  std::shared_ptr<UniverseServer> m_server;
  std::shared_ptr<UniverseClient> m_client;
  std::shared_ptr<Player> m_mainPlayer;
};

}


export module star.test_universe;

export namespace Star {
  using ::Star::TestUniverse;
  using ::Star::TestUniverseException;
  using ::Star::TestUniverseTimeoutException;
}
