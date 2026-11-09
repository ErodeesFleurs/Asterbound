#ifdef STAR_ENABLE_STEAM_INTEGRATION
#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
#include "discord/discord.h"
#endif
import std;
import star.thread;
import star.input_event;
import star.application;
import star.string;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;


import star.platform_services_pc;
import star.desktop_service_pc_steam;

namespace Star {

SteamDesktopService::SteamDesktopService(std::shared_ptr<PcPlatformServicesState> state)
  : m_state(std::move(state)) {}

void SteamDesktopService::openUrl(String const& url) {
  SteamAPI_ISteamFriends_ActivateGameOverlayToWebPage(SteamAPI_SteamFriends(), url.utf8_data(),
      k_EActivateGameOverlayToWebPageMode_Default);
}

}
