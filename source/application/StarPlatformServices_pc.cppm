module;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
#include "discord/discord.h"
#endif
import std;
import star.format;
import star.thread;
import star.input_event;
import star.application;
import star.string;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.logging;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
import star.steam_dispatch;
#endif


namespace Star {

class PcPlatformServices;

struct PcPlatformServicesState {
  [[nodiscard]] static std::expected<std::shared_ptr<PcPlatformServicesState>, StarException> create(Logger& logger);
  ~PcPlatformServicesState();
  Logger& logger;
  bool steamIsFlatpak = false;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  [[nodiscard]] std::expected<void, StarException> onGameOverlayActivated(GameOverlayActivated_t& callback);
  SteamDispatch steamDispatch;
  SteamCallback<PcPlatformServicesState, GameOverlayActivated_t> callbackGameOverlayActivated;

  bool steamAvailable = false;
#endif
  std::optional<StarException> steamError;

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  bool discordAvailable = false;
  std::optional<StarException> discordError;

  // Must lock discordMutex before accessing any of the managers when not inside
  // a discord callback.
  std::mutex discordMutex;

  std::unique_ptr<discord::Core> discordCore;
  
  std::optional<discord::User> discordCurrentUser;
  ThreadFunction<void> discordEventThread;
  std::stop_source discordEventStopSource{std::nostopstate};
#endif

  bool overlayActive = false;
private:
  explicit PcPlatformServicesState(Logger& logger);
  std::expected<void, StarException> initialize();
};


class PcPlatformServices {
public:
  // Any command line arguments that start with '+platform' will be stripped
  // out and passed here
  [[nodiscard]] static std::expected<std::unique_ptr<PcPlatformServices>, StarException> create(Logger& logger, String const& path, StringList platformArguments);

  std::shared_ptr<StatisticsService> statisticsService() const;
  std::shared_ptr<P2PNetworkingService> p2pNetworkingService() const;
  std::shared_ptr<UserGeneratedContentService> userGeneratedContentService() const;
  std::shared_ptr<DesktopService> desktopService() const;
  bool steamIsFlatpak() const;

  // Will return true if there is an in-game overlay active.  This is important
  // because the cursor must be visible when such an overlay is active,
  // regardless of the ApplicationController setting.
  bool overlayActive() const;

  [[nodiscard]] std::expected<void, StarException> update();

private:
  PcPlatformServices() = default;

  std::shared_ptr<PcPlatformServicesState> m_state;

  std::shared_ptr<StatisticsService> m_statisticsService;
  std::shared_ptr<P2PNetworkingService> m_p2pNetworkingService;
  std::shared_ptr<UserGeneratedContentService> m_userGeneratedContentService;
  std::shared_ptr<DesktopService> m_desktopService;
};

}

export module star.platform_services_pc;

export namespace Star {
  using ::Star::PcPlatformServices;
  using ::Star::PcPlatformServicesState;
}
