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
import star.json;
import star.string;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
import star.steam_dispatch;
#endif

import star.platform_services_pc;

namespace Star {

class SteamStatisticsService : public StatisticsService {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<SteamStatisticsService>, StarException> create(std::shared_ptr<PcPlatformServicesState> state);

  bool initialized() const override;
  std::optional<StarException> error() const override;

  [[nodiscard]] std::expected<bool, StarException> setStat(String const& name, String const& type, Json const& value) override;
  Json getStat(String const& name, String const& type, Json def = {}) const override;

  bool reportEvent(String const& name, Json const& fields) override;

  [[nodiscard]] std::expected<bool, StarException> unlockAchievement(String const& name) override;
  StringSet achievementsUnlocked() const override;

  [[nodiscard]] std::expected<void, StarException> refresh() override;
  [[nodiscard]] std::expected<void, StarException> flush() override;
  bool reset() override;

private:
  SteamStatisticsService(std::shared_ptr<PcPlatformServicesState> state);
  std::expected<void, StarException> onUserStatsStored(UserStatsStored_t& callback);
  std::expected<void, StarException> onAchievementStored(UserAchievementStored_t& callback);

  std::shared_ptr<PcPlatformServicesState> m_state;
  SteamCallback<SteamStatisticsService, UserStatsStored_t> m_callbackUserStatsStored;
  SteamCallback<SteamStatisticsService, UserAchievementStored_t> m_callbackAchievementStored;

  std::uint64_t m_appId;
  bool m_initialized = false;
  std::optional<StarException> m_error;
};

}

export module star.statistics_service_pc_steam;

export namespace Star {
  using ::Star::SteamStatisticsService;
}
