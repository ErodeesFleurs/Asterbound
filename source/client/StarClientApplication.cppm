module;
import std;
import star.json;
import star.string;
import star.list;
import star.thread;
import star.vector;
import star.byte_array;
import star.application;
import star.input_event;
import star.host_address;
import star.p2p_networking_service;
import star.world_render_data;
import star.key_bindings;
import star.root;
import star.listener;
import star.logging;
import star.format;
import star.world_camera;

namespace Star {

class Input;
class Voice;
class MainMixer;
class GuiContext;
class Cinematic;
class ErrorScreen;
class PlayerStorage;
class Statistics;
class UniverseClient;
class TitleScreen;
class Player;
class WorldPainter;
class MainInterface;
class UniverseServer;
class LuaHttpSession;
class LuaCallbacks;
class Configuration;
class WorldClient;

class ClientApplication : public Application {
public:
  Logger& logger() const override { return m_root->logger(); }
  ~ClientApplication() override;
  [[nodiscard]] std::expected<void, StarException> setPostProcessLayerPasses(String const& layer, unsigned const& passes);
  [[nodiscard]] std::expected<void, StarException> setPostProcessGroupEnabled(String const& group, bool const& enabled, std::optional<bool> const& save);
  [[nodiscard]] std::expected<bool, StarException> postProcessGroupEnabled(String const& group);
  [[nodiscard]] std::expected<Json, StarException> postProcessGroups();
  unsigned framesSkipped() const override;

protected:
  [[nodiscard]] std::expected<void, StarException> startup(StringList const& cmdLineArgs) override;
  [[nodiscard]] std::expected<void, StarException> shutdown() override;

  [[nodiscard]] std::expected<void, StarException> applicationInit(std::shared_ptr<ApplicationController> appController) override;
  [[nodiscard]] std::expected<void, StarException> renderInit(std::shared_ptr<Renderer> renderer) override;

  [[nodiscard]] std::expected<void, StarException> windowChanged(WindowMode windowMode, Vec2U screenSize) override;

  [[nodiscard]] std::expected<void, StarException> processInput(InputEvent const& event) override;

  [[nodiscard]] std::expected<void, StarException> update() override;
  [[nodiscard]] std::expected<void, StarException> render() override;
  [[nodiscard]] std::expected<void, StarException> framePresented() override;

  [[nodiscard]] std::expected<void, StarException> getAudioData(std::span<std::int16_t> stream, std::size_t len) override;

private:
  enum class MainAppState {
    Quit,
    Startup,
    SteamFlatpakWarning,
    Mods,
    ModsWarning,
    Splash,
    Error,
    Title,
    SinglePlayer,
    MultiPlayer
  };

  struct PendingMultiPlayerConnection {
    std::variant<P2PNetworkingPeerId, HostAddressWithPort> server;
    String account;
    String password;
    bool forceLegacy;
  };
  
  struct PostProcessGroup {
    bool enabled;
  };
  
  struct PostProcessLayer {
    List<String> effects;
    unsigned passes;
    std::optional<std::reference_wrapper<PostProcessGroup>> group;
  };

  [[nodiscard]] std::expected<void, StarException> renderReload();
  [[nodiscard]] std::expected<void, StarException> applyRenderSettings(Configuration const& configuration);

  [[nodiscard]] std::expected<void, StarException> changeState(MainAppState newState);
  [[nodiscard]] std::expected<void, StarException> prepareStateCinematic(MainAppState newState);
  [[nodiscard]] std::expected<void, StarException> teardownTransition(MainAppState oldState, MainAppState newState);
  [[nodiscard]] std::expected<void, StarException> prepareTitleServices();
  void installPlayerReloadCallbacks();
  [[nodiscard]] std::expected<void, StarException> prepareTitleForm();
  [[nodiscard]] std::expected<void, StarException> startSession(MainAppState newState);
  // False means setError completed recovery to the title, not a ready session.
  [[nodiscard]] std::expected<bool, StarException> activateSessionPlayer();
  [[nodiscard]] std::expected<bool, StarException> connectMultiPlayer();
  [[nodiscard]] std::expected<bool, StarException> connectSinglePlayer();
  [[nodiscard]] std::expected<void, StarException> prepareSessionInterface();
  [[nodiscard]] std::expected<void, StarException>
  installLuaCallbacks(String const& name, std::expected<LuaCallbacks, StarException> callbacks);
  [[nodiscard]] std::expected<void, StarException> recoverToTitle(std::expected<void, StarException> logged,
                                                                  std::expected<void, StarException> displayed);
  [[nodiscard]] std::expected<void, StarException> setError(String const& error);
  [[nodiscard]] std::expected<void, StarException> setError(String const& error, ExceptionDiagnostic const& e);

  [[nodiscard]] std::expected<void, StarException> loadMods();
  [[nodiscard]] std::expected<void, StarException> updateSteamFlatpakWarning(float dt);
  [[nodiscard]] std::expected<void, StarException> updateMods(float dt);
  [[nodiscard]] std::expected<void, StarException> updateModsWarning(float dt);
  [[nodiscard]] std::expected<void, StarException> updateSplash(float dt);
  [[nodiscard]] std::expected<void, StarException> updateError(float dt);
  [[nodiscard]] std::expected<void, StarException> updateTitle(float dt);
  [[nodiscard]] std::expected<void, StarException> updateRunning(float dt);

  bool isHeldActionTaken(InterfaceAction action) const;

  [[nodiscard]] std::expected<void, StarException> updateCamera(float dt);
  void restorePresentedCamera();

  std::unique_ptr<Root> m_root;
  ThreadFunction<void> m_rootLoader;
  std::shared_ptr<CallbackListener> m_reloadListener;
  ListenerSubscription m_reloadSubscription;

  MainAppState m_state = MainAppState::Startup;

  // Valid after applicationInit is called
  std::unique_ptr<MainMixer> m_mainMixer;
  std::shared_ptr<GuiContext> m_guiContext;
  std::shared_ptr<Input> m_input;
  std::shared_ptr<Voice> m_voice;
  std::shared_ptr<LuaHttpSession> m_httpSession;

  // Valid after renderInit is called the first time
  std::shared_ptr<Cinematic> m_cinematicOverlay;
  std::shared_ptr<ErrorScreen> m_errorScreen;

  // Valid if main app state >= Title
  std::shared_ptr<PlayerStorage> m_playerStorage;
  std::shared_ptr<Statistics> m_statistics;
  std::shared_ptr<UniverseClient> m_universeClient;
  std::shared_ptr<TitleScreen> m_titleScreen;

  // Valid if main app state > Title
  std::shared_ptr<Player> m_player;
  std::shared_ptr<WorldPainter> m_worldPainter;
  WorldRenderData m_renderData;
  std::unique_ptr<MainInterface> m_mainInterface;
  
  StringMap<PostProcessGroup> m_postProcessGroups;
  List<PostProcessLayer> m_postProcessLayers;
  StringMap<std::size_t> m_labelledPostProcessLayers;

  // Valid if main app state == SinglePlayer
  std::shared_ptr<UniverseServer> m_universeServer;

  float m_cameraXOffset = 0.0f;
  float m_cameraYOffset = 0.0f;
  bool m_snapBackCameraOffset = false;
  float m_cameraOffsetDownTime = 0.f;
  Vec2F m_cameraPositionSmoother;
  Vec2F m_cameraSmoothDelta;
  int m_cameraZoomDirection = 0;
  std::weak_ptr<WorldClient> m_presentedWorld;
  std::weak_ptr<Player> m_presentedPlayer;
  std::uint64_t m_presentedPlayerEpoch = 0;
  std::uint64_t m_presentedRenderEpoch = 0;
  struct PresentedCamera {
    WorldCamera camera;
    std::weak_ptr<WorldClient> world;
    std::weak_ptr<Player> player;
    std::uint64_t playerEpoch;
    std::uint64_t renderEpoch;
  };
  // Main acknowledges presentation; only the application owner touches these.
  std::optional<PresentedCamera> m_preparedCamera;
  std::optional<PresentedCamera> m_inputCamera;
  float m_appliedRenderFpsLimit = -1.0f;
  bool m_appliedVSync = true;

  unsigned m_framesSkipped = 0;
  float m_minInterfaceScale = 2;
  float m_maxInterfaceScale = 3;
  Vec2F m_crossoverRes;

  bool m_controllerInput;
  Vec2F m_controllerLeftStick;
  Vec2F m_controllerRightStick;
  List<KeyDownEvent> m_heldKeyEvents;
  List<KeyDownEvent> m_edgeKeyEvents;

  std::optional<PendingMultiPlayerConnection> m_pendingMultiPlayerConnection;
  std::optional<HostAddressWithPort> m_currentRemoteJoin;
  std::int64_t m_timeSinceJoin = 0;

  ByteArray m_immediateFont;

  bool m_loggedUGCCheck;
};

}


export module star.client_application;

export namespace Star {
  using ::Star::ClientApplication;
}
