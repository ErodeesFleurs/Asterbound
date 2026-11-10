#ifdef STAR_ENABLE_STEAM_INTEGRATION
#include "steam/steam_api.h"
#include "steam/steam_api_flat.h"
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
#include "discord/discord.h"
#endif
import std;
import star.format;
import star.logging;
import star.thread;
import star.string;
import star.file;
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

import star.strong_typedef;
import star.rpc_promise;
import star.p2p_networking_service_pc;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
import star.statistics_service_pc_steam;

import star.user_generated_content_service_pc_steam;

import star.desktop_service_pc_steam;
#endif

namespace Star {

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
std::uint64_t const DiscordClientId = STAR_DISCORD_APPLICATION_ID;
#endif

PcPlatformServicesState::PcPlatformServicesState(Logger& logger)
  : logger(logger)
#ifdef STAR_ENABLE_STEAM_INTEGRATION
  , callbackGameOverlayActivated(steamDispatch, *this, &PcPlatformServicesState::onGameOverlayActivated)
#endif
{
}

std::expected<std::shared_ptr<PcPlatformServicesState>, StarException> PcPlatformServicesState::create(Logger& logger) {
  auto state = std::shared_ptr<PcPlatformServicesState>(new PcPlatformServicesState(logger));
  auto initialized = state->initialize();
  if (!initialized)
    return std::unexpected(std::move(initialized.error()));
  return state;
}

std::expected<void, StarException> PcPlatformServicesState::initialize() {

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  bool shouldLoadSteam = true;

  #ifdef STAR_SYSTEM_LINUX
  // Flatpak doesn't create ~/.steam by default, which prevents
  // SteamAPI from initializing...
  //
  // Flatpak also causes crashes should the user rectify this by
  // symlinking.
  //
  // We'll determine if the user is using Flatpak for Steam here,
  // so we can warn them about such issues.
  shouldLoadSteam = true;

  bool steamFlatpakInstalled = false;

  auto flatpakDirectory = File::is_directory("/var/lib/flatpak/app/com.valvesoftware.Steam");
  if (!flatpakDirectory)
    return std::unexpected(std::move(flatpakDirectory.error()));
  if (*flatpakDirectory) {
    if (auto logged = logger.info("Steam flatpak is installed"); !logged)
      return std::unexpected(std::move(logged.error()));
    steamFlatpakInstalled = true;
  }

  auto steamRoot = String::join_with("", String(std::string_view(std::getenv("HOME"))), "/.steam");
  if (!steamRoot)
    return std::unexpected(std::move(steamRoot.error()));
  String steamRootPath = std::move(*steamRoot);
  auto steamDirectory = File::is_directory(steamRootPath);
  if (!steamDirectory)
    return std::unexpected(std::move(steamDirectory.error()));
  if (!*steamDirectory) {
    if (steamFlatpakInstalled) {
      steamIsFlatpak = true;
    } else {
      if (auto logged = logger.info("Could not find ~/.steam; Steam platform services will be unavailable!"); !logged)
        return std::unexpected(std::move(logged.error()));
      shouldLoadSteam = false;
    }
  } else {
    // Account for the user having Steam installed as both a flatpak
    // and normal.
    if (steamFlatpakInstalled) {
      // Check if ~/.steam points to a flatpak installation
      auto steamAbsRootPath = File::full_path(steamRootPath);
      if (!steamAbsRootPath)
        return std::unexpected(std::move(steamAbsRootPath.error()));
      auto isFlatpak = steamAbsRootPath->contains("com.valvesoftware.Steam");
      if (!isFlatpak)
        return std::unexpected(std::move(isFlatpak.error()));
      if (*isFlatpak) {
        if (auto logged = logger.info("~/.steam points towards the installed flatpak"); !logged)
          return std::unexpected(std::move(logged.error()));
        steamIsFlatpak = true;
        shouldLoadSteam = false;
      } else {
        if (auto logged = logger.info("~/.steam does not point towards the installed flatpak"); !logged)
          return std::unexpected(std::move(logged.error()));
      }
    }
  }
  #endif

  if (shouldLoadSteam) {
    SteamErrMsg errMsg;
    if (SteamAPI_InitEx(&errMsg) == k_ESteamAPIInitResult_OK) {
      steamAvailable = true;
      auto initialized = steamDispatch.initialize();
      if (!initialized) {
        steamError = std::move(initialized.error());
        SteamAPI_Shutdown();
        steamAvailable = false;
        if (auto logged = logger.error("Failed to initialize Steam dispatch: {}", steamError->what()); !logged)
          return std::unexpected(std::move(logged.error()));
      } else {
        if (auto logged = logger.info("Initialized Steam platform services"); !logged)
          return std::unexpected(std::move(logged.error()));
      }
    } else {
      if (auto logged = logger.info("Failed to initialize Steam platform services: {}", errMsg); !logged)
        return std::unexpected(std::move(logged.error()));
      steamError = ApplicationException::format("Failed to initialize Steam platform services: {}", errMsg);
    }
  } else {
    if (auto logged = logger.info("Skipping Steam platform initialization"); !logged)
      return std::unexpected(std::move(logged.error()));
  }
#endif

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  static std::int64_t const DiscordEventSleep = 3;

  discord::Core* discordCorePtr = nullptr;
  discord::Result res = discord::Core::Create(DiscordClientId, DiscordCreateFlags_NoRequireDiscord, &discordCorePtr);
  if (res == discord::Result::Ok && discordCorePtr) {
    discordCore.reset(discordCorePtr);
    discordAvailable = true;

    discordCore->UserManager().OnCurrentUserUpdate.Connect([this](){
        discord::User user;
        auto res = discordCore->UserManager().GetCurrentUser(&user);
        if (res != discord::Result::Ok) {
          discordError = ApplicationException::format("Could not get current Discord user (err {})", (int)res);
          if (auto logged = this->logger.error("{}", discordError->what()); !logged)
            discordError = std::move(logged.error());
        } else {
          discordCurrentUser = user;
        }
      });

  } else {
    if (auto logged = logger.error("Failed to instantiate Discord core (err {})", (int)res); !logged)
      return std::unexpected(std::move(logged.error()));
    discordError = ApplicationException::format("Failed to instantiate Discord core (err {})", (int)res);
  }

  if (discordAvailable) {
    std::lock_guard locker(discordMutex);
    discordCore->SetLogHook(discord::LogLevel::Info, [&state = *this](discord::LogLevel level, char const* msg) {
      std::expected<void, StarException> logged;
      if (level == discord::LogLevel::Debug)
        logged = state.logger.debug("[Discord]: {}", msg);
      else if (level == discord::LogLevel::Error)
        logged = state.logger.error("[Discord]: {}", msg);
      else if (level == discord::LogLevel::Info)
        logged = state.logger.info("[Discord]: {}", msg);
      else if (level == discord::LogLevel::Warn)
        logged = state.logger.warn("[Discord]: {}", msg);
      if (!logged)
        state.discordError = std::move(logged.error());
    });
    discordEventStopSource = std::stop_source{};
    auto eventThread = Thread::invoke("PcPlatformServices::discordEventThread", [&state = *this, stopToken = discordEventStopSource.get_token()]() -> std::expected<void, StarException> {
        while (!stopToken.stop_requested()) {
          {
            std::lock_guard locker(state.discordMutex);
            auto result = state.discordCore->RunCallbacks();
            if (result != discord::Result::Ok) {
              state.discordError = ApplicationException::format("Discord callback dispatch failed (err {})", (int)result);
              if (auto logged = state.logger.error("{}", state.discordError->what()); !logged) {
                state.discordError = logged.error();
                return std::unexpected(std::move(logged.error()));
              }
            }
            result = state.discordCore->LobbyManager().FlushNetwork();
            if (result != discord::Result::Ok) {
              state.discordError = ApplicationException::format("Discord network flush failed (err {})", (int)result);
              if (auto logged = state.logger.error("{}", state.discordError->what()); !logged) {
                state.discordError = logged.error();
                return std::unexpected(std::move(logged.error()));
              }
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(DiscordEventSleep));
        }
        return {};
      });
    if (!eventThread)
      return std::unexpected(std::move(eventThread.error()));
    discordEventThread = std::move(*eventThread);

    if (auto logged = logger.info("Initialized Discord platform services"); !logged)
      return std::unexpected(std::move(logged.error()));
  } else {
    if (auto logged = logger.info("Was not able to authenticate with Discord and create all components, Discord services will be unavailable"); !logged)
      return std::unexpected(std::move(logged.error()));
  }
#endif
  return {};
}

PcPlatformServicesState::~PcPlatformServicesState() {
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  if (discordEventThread) {
    discordEventStopSource.request_stop();
    auto finished = discordEventThread.finish();
    if (!finished)
      EmergencyLog::error("Could not finish Discord event thread: {}", finished.error().what());
  }
  if (discordAvailable && discordError)
    EmergencyLog::error("Unobserved Discord failure during shutdown: {}", discordError->what());
#endif
#ifdef STAR_ENABLE_STEAM_INTEGRATION
  if (steamAvailable)
    SteamAPI_Shutdown();
#endif
}

#ifdef STAR_ENABLE_STEAM_INTEGRATION
std::expected<void, StarException> PcPlatformServicesState::onGameOverlayActivated(GameOverlayActivated_t& callback) {
  overlayActive = callback.m_bActive;
  return {};
}
#endif

std::expected<std::unique_ptr<PcPlatformServices>, StarException> PcPlatformServices::create(Logger& logger, [[maybe_unused]] String const& path, StringList platformArguments) {
  auto services = std::unique_ptr<PcPlatformServices>(new PcPlatformServices);

  auto state = PcPlatformServicesState::create(logger);
  if (!state)
    return std::unexpected(std::move(state.error()));
  services->m_state = std::move(*state);

  bool provideP2PNetworking = false;

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  provideP2PNetworking |= services->m_state->steamAvailable;
#endif

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  provideP2PNetworking |= services->m_state->discordAvailable;
#endif

  if (provideP2PNetworking) {
    auto p2pNetworkingService = std::make_shared<PcP2PNetworkingService>(services->m_state);
    for (auto& argument : platformArguments) {
      auto connectArgument = argument.starts_with("+platform:connect:");
      if (!connectArgument)
        return std::unexpected(std::move(connectArgument.error()));
      if (*connectArgument) {
        if (auto logged = logger.info("PC platform services joining from command line argument '{}'", argument); !logged)
          return std::unexpected(std::move(logged.error()));
        auto joined = p2pNetworkingService->addPendingJoin(std::move(argument));
        if (!joined)
          return std::unexpected(std::move(joined.error()));
      } else {
        return std::unexpected(ApplicationException::format("Unrecognized PC platform services command line argument '{}'", argument));
      }
    }

    services->m_p2pNetworkingService = p2pNetworkingService;
  }

#ifdef STAR_ENABLE_STEAM_INTEGRATION
  if (services->m_state->steamAvailable) {
    auto statistics = SteamStatisticsService::create(services->m_state);
    if (!statistics)
      return std::unexpected(std::move(statistics.error()));
    services->m_statisticsService = std::move(*statistics);
    services->m_userGeneratedContentService = std::make_shared<SteamUserGeneratedContentService>(services->m_state);
    services->m_desktopService = std::make_shared<SteamDesktopService>(services->m_state);
  }
#endif

#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(services->m_state->discordMutex);
  if (services->m_state->discordAvailable) {
    if (auto logged = logger.debug("Registering Asterbound to Discord at path: {}", path); !logged)
      return std::unexpected(std::move(logged.error()));
    auto registered = services->m_state->discordCore->ActivityManager().RegisterCommand(path.utf8_data());
    if (registered != discord::Result::Ok)
      return std::unexpected(ApplicationException::format("Could not register Discord launch command (err {})", (int)registered));
  }
#endif

  return services;
}

std::shared_ptr<StatisticsService> PcPlatformServices::statisticsService() const {
  return m_statisticsService;
}

std::shared_ptr<P2PNetworkingService> PcPlatformServices::p2pNetworkingService() const {
  return m_p2pNetworkingService;
}

std::shared_ptr<UserGeneratedContentService> PcPlatformServices::userGeneratedContentService() const {
  return m_userGeneratedContentService;
}

std::shared_ptr<DesktopService> PcPlatformServices::desktopService() const {
  return m_desktopService;
}

bool PcPlatformServices::steamIsFlatpak() const {
  return m_state->steamIsFlatpak;
}

bool PcPlatformServices::overlayActive() const {
  return m_state->overlayActive;
}

std::expected<void, StarException> PcPlatformServices::update() {
#ifdef STAR_ENABLE_STEAM_INTEGRATION
  if (m_state->steamAvailable) {
    auto dispatched = m_state->steamDispatch.runCallbacks();
    if (!dispatched)
      return std::unexpected(std::move(dispatched.error()));
  }
#endif
#ifdef STAR_ENABLE_DISCORD_INTEGRATION
  std::lock_guard discordLocker(m_state->discordMutex);
  if (m_state->discordAvailable && m_state->discordError)
    return std::unexpected(std::move(*std::exchange(m_state->discordError, {})));
#endif
  return {};
}

}
