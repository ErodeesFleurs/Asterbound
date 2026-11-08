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
import star.list;
import star.format;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
import star.steam_dispatch;
#endif

import star.platform_services_pc;
import star.user_generated_content_service_pc_steam;
import star.logging;
import star.lexical_cast;

namespace Star {

SteamUserGeneratedContentService::SteamUserGeneratedContentService(std::shared_ptr<PcPlatformServicesState> state)
  : m_state(std::move(state)),
    m_callbackDownloadResult(m_state->steamDispatch, *this, &SteamUserGeneratedContentService::onDownloadResult),
    m_checkedUGC(false) {};

StringList SteamUserGeneratedContentService::subscribedContentIds() const {
  List<PublishedFileId_t> contentIds(SteamAPI_ISteamUGC_GetNumSubscribedItems(SteamAPI_SteamUGC(), false), {});
  SteamAPI_ISteamUGC_GetSubscribedItems(SteamAPI_SteamUGC(), contentIds.data(), contentIds.size(), false);
  return contentIds.transformed([](PublishedFileId_t id) {
      return String(Star::to_string(id));
    });
}

std::expected<std::optional<String>, StarException> SteamUserGeneratedContentService::contentDownloadDirectory(String const& contentId) const {
  auto parsed = lexical_cast<PublishedFileId_t>(contentId);
  if (!parsed)
    return std::unexpected(std::move(parsed.error()));
  PublishedFileId_t id = *parsed;
  uint32 itemState = SteamAPI_ISteamUGC_GetItemState(SteamAPI_SteamUGC(), id);
  if (itemState & k_EItemStateInstalled) {
    char path[4096];
    if (SteamAPI_ISteamUGC_GetItemInstallInfo(SteamAPI_SteamUGC(), id, nullptr, path, sizeof(path), nullptr))
      return std::optional<String>(String(path));
    return std::unexpected(ApplicationException::format("Steam could not obtain installation info for content {}", contentId));
  }
  return {};
}

std::expected<UserGeneratedContentService::UGCState, StarException> SteamUserGeneratedContentService::triggerContentDownload() {
  if (m_downloadError)
    return std::unexpected(*m_downloadError);
  List<PublishedFileId_t> contentIds(SteamAPI_ISteamUGC_GetNumSubscribedItems(SteamAPI_SteamUGC(), false), {});
  SteamAPI_ISteamUGC_GetSubscribedItems(SteamAPI_SteamUGC(), contentIds.data(), contentIds.size(), false);

  if (!m_checkedUGC) {
    bool contentNeedsUpdate = false;
    for (uint64 contentId : contentIds) {
      uint32 itemState = SteamAPI_ISteamUGC_GetItemState(SteamAPI_SteamUGC(), contentId);
      if (!(itemState & k_EItemStateInstalled) || itemState & k_EItemStateNeedsUpdate) {
        // Download is needed
        contentNeedsUpdate = true;
      }
    }
    m_checkedUGC = true;
    if (!contentNeedsUpdate) {
      // No download was needed
      return UserGeneratedContentService::UGCState::NoDownload;
    }
  }

  for (uint64 contentId : contentIds) {
    if (!m_currentDownloadState.contains(contentId)) {
      uint32 itemState = SteamAPI_ISteamUGC_GetItemState(SteamAPI_SteamUGC(), contentId);
      if (!(itemState & k_EItemStateInstalled) || itemState & k_EItemStateNeedsUpdate) {
        // DownloadItem has a return bool if it succeeds in attempting to download.
        if (SteamAPI_ISteamUGC_DownloadItem(SteamAPI_SteamUGC(), contentId, true)) {
          itemState = SteamAPI_ISteamUGC_GetItemState(SteamAPI_SteamUGC(), contentId);
          m_currentDownloadState[contentId] = (itemState & k_EItemStateInstalled)
              && !(itemState & (k_EItemStateNeedsUpdate | k_EItemStateDownloading));
        } else {
          m_downloadError = ApplicationException::format("Steam rejected download of content {}", contentId);
          return std::unexpected(*m_downloadError);
        }
      } else {
        m_currentDownloadState[contentId] = true;
      }
    }
  }

  bool allDownloaded = true;
  for (auto const& p : m_currentDownloadState) {
    if (!p.second)
      allDownloaded = false;
  }

  if (allDownloaded) {
    return UserGeneratedContentService::UGCState::Finished;
  } else {
    return UserGeneratedContentService::UGCState::InProgress;
  }
}

std::expected<void, StarException> SteamUserGeneratedContentService::onDownloadResult(DownloadItemResult_t& result) {
  PublishedFileId_t file = result.m_nPublishedFileId;
  if (result.m_eResult != k_EResultOK) {
    m_downloadError = ApplicationException::format("Steam download of content {} failed with code {}", file, (int)result.m_eResult);
    return {};
  }
  m_currentDownloadState[file] = true;
  return {};
}

}
