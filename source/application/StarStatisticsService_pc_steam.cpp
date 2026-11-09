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
import star.statistics_service_pc_steam;
import star.logging;

namespace Star {

SteamStatisticsService::SteamStatisticsService(std::shared_ptr<PcPlatformServicesState> state)
: m_state(std::move(state)),
  m_callbackUserStatsStored(m_state->steamDispatch, *this, &SteamStatisticsService::onUserStatsStored),
  m_callbackAchievementStored(m_state->steamDispatch, *this, &SteamStatisticsService::onAchievementStored) {
  m_appId = SteamAPI_ISteamUtils_GetAppID(SteamAPI_SteamUtils());
}

std::expected<std::shared_ptr<SteamStatisticsService>, StarException> SteamStatisticsService::create(std::shared_ptr<PcPlatformServicesState> state) {
  auto service = std::shared_ptr<SteamStatisticsService>(new SteamStatisticsService(std::move(state)));
  auto refreshed = service->refresh();
  if (!refreshed) {
    auto logged = service->m_state->logger.error("Steam statistics unavailable: {}", refreshed.error().what());
    if (!logged)
      return std::unexpected(std::move(logged.error()));
  }
  return service;
}

bool SteamStatisticsService::initialized() const {
  return m_initialized;
}

std::optional<StarException> SteamStatisticsService::error() const {
  return m_error;
}

std::expected<bool, StarException> SteamStatisticsService::setStat(String const& name, String const& type, Json const& value) {
  if (type == "int") {
    auto converted = value.toInt();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    return SteamAPI_ISteamUserStats_SetStatInt32(SteamAPI_SteamUserStats(), name.utf8_data(), (std::int32_t)*converted);
  }
  if (type == "float") {
    auto converted = value.toFloat();
    if (!converted)
      return std::unexpected(std::move(converted.error()));
    return SteamAPI_ISteamUserStats_SetStatFloat(SteamAPI_SteamUserStats(), name.utf8_data(), *converted);
  }
  return false;
}

Json SteamStatisticsService::getStat(String const& name, String const& type, Json def) const {
  if (type == "int") {
    std::int32_t intValue = 0;
    if (SteamAPI_ISteamUserStats_GetStatInt32(SteamAPI_SteamUserStats(), name.utf8_data(), &intValue))
      return Json(intValue);
  }

  if (type == "float") {
    float floatValue = 0.0f;
    if (SteamAPI_ISteamUserStats_GetStatFloat(SteamAPI_SteamUserStats(), name.utf8_data(), &floatValue))
      return Json(floatValue);
  }

  return def;
}

bool SteamStatisticsService::reportEvent(String const&, Json const&) {
  // Steam doesn't support events
  return false;
}

std::expected<bool, StarException> SteamStatisticsService::unlockAchievement(String const& name) {
  if (!SteamAPI_ISteamUserStats_SetAchievement(SteamAPI_SteamUserStats(), name.utf8_data())) {
    auto logged = m_state->logger.error("Cannot set Steam achievement {}", name);
    if (!logged)
      return std::unexpected(std::move(logged.error()));
    return false;
  }
  return true;
}

StringSet SteamStatisticsService::achievementsUnlocked() const {
  StringSet achievements;
  for (std::uint32_t i = 0; i < SteamAPI_ISteamUserStats_GetNumAchievements(SteamAPI_SteamUserStats()); ++i) {
    String achievement(std::string_view(SteamAPI_ISteamUserStats_GetAchievementName(SteamAPI_SteamUserStats(), i)));

    bool unlocked = false;
    if (SteamAPI_ISteamUserStats_GetAchievement(SteamAPI_SteamUserStats(), achievement.utf8_data(), &unlocked) && unlocked) {
      achievements.add(achievement);
    }
  }
  return achievements;
}

std::expected<void, StarException> SteamStatisticsService::refresh() {
  if (!SteamAPI_ISteamUser_BLoggedOn(SteamAPI_SteamUser())) {
    m_error = ApplicationException("Not logged in");
    return std::unexpected(*m_error);
  }
  // Steamworks 1.61+ preloads local stats and achievements before launch.
  m_initialized = true;
  m_error.reset();
  return {};
}

std::expected<void, StarException> SteamStatisticsService::flush() {
  if (!SteamAPI_ISteamUserStats_StoreStats(SteamAPI_SteamUserStats())) {
    m_error = ApplicationException("Steam StoreStats was rejected");
    return std::unexpected(*m_error);
  }
  return {};
}

bool SteamStatisticsService::reset() {
  return SteamAPI_ISteamUserStats_ResetAllStats(SteamAPI_SteamUserStats(), true);
}


std::expected<void, StarException> SteamStatisticsService::onUserStatsStored(UserStatsStored_t& callback) {
  if (callback.m_nGameID != m_appId)
    return {};

  if (callback.m_eResult == k_EResultOK) {
    return m_state->logger.debug("Steam StoreStats successful");
  }

  if (callback.m_eResult == k_EResultInvalidParam) {
    // A stat we set broke a constraint and was reverted on the service.
    return m_state->logger.info("Steam StoreStats: Some stats failed validation");
  }

  m_error = ApplicationException::format("Steam StoreStats failed with code {}", (int)callback.m_eResult);
  return {};
}

std::expected<void, StarException> SteamStatisticsService::onAchievementStored(UserAchievementStored_t& callback) {
  if (callback.m_nGameID != m_appId)
    return {};

  return m_state->logger.debug("Steam achievement {} stored successfully", callback.m_rgchAchievementName);
}

}
