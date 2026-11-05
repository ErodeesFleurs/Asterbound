add_library (star_application_module_interfaces OBJECT)
target_sources (star_application_module_interfaces PUBLIC FILE_SET CXX_MODULES FILES
    StarApplication.cppm
    StarApplicationController.cppm
    StarFixedScheduler.cppm
    StarMainApplication.cppm
    StarMainThreadDispatcher.cppm
    StarMainThreadServices.cppm
    StarNativeUiContext.cppm
    StarRenderer.cppm
    StarThreadedRenderer.cppm
    StarTextureAtlas.cppm
    StarRenderer_opengl.cppm
    StarP2PNetworkingService_pc.cppm
    StarPlatformServices_pc.cppm
  )

add_library (star_application_modules STATIC
    StarApplication.cpp
    StarRenderer.cpp
    StarThreadedRenderer.cpp
    StarMainApplication_sdl.cpp
    StarP2PNetworkingService_pc.cpp
    StarPlatformServices_pc.cpp
    StarRenderer_opengl.cpp
  )

if (STAR_ENABLE_STEAM_INTEGRATION)
  add_library (star_steam_dispatch_modules STATIC)
  target_sources (star_steam_dispatch_modules PUBLIC FILE_SET CXX_MODULES FILES
      StarSteamDispatch.cppm)
  target_include_directories (star_steam_dispatch_modules PRIVATE
      ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES})
  target_link_libraries (star_steam_dispatch_modules PUBLIC star_modules
      PRIVATE star_build_config star_steam_dependencies)
  set_target_properties (star_steam_dispatch_modules PROPERTIES CXX_SCAN_FOR_MODULES ON)
  target_link_libraries (star_application_module_interfaces PUBLIC star_steam_dispatch_modules)
  target_sources (star_application_module_interfaces PUBLIC FILE_SET CXX_MODULES FILES
      StarDesktopService_pc_steam.cppm
      StarStatisticsService_pc_steam.cppm
      StarUserGeneratedContentService_pc_steam.cppm
    )
  target_sources (star_application_modules PRIVATE
      StarDesktopService_pc_steam.cpp
      StarStatisticsService_pc_steam.cpp
      StarUserGeneratedContentService_pc_steam.cpp
    )
endif ()

if (STAR_ENABLE_DISCORD_INTEGRATION)
  set (star_application_DISCORD_SOURCES
      discord/core.cpp
      discord/image_manager.cpp
      discord/lobby_manager.cpp
      discord/network_manager.cpp
      discord/overlay_manager.cpp
      discord/relationship_manager.cpp
      discord/storage_manager.cpp
      discord/store_manager.cpp
      discord/types.cpp
      discord/user_manager.cpp
      discord/voice_manager.cpp
      discord/achievement_manager.cpp
      discord/activity_manager.cpp
      discord/application_manager.cpp
    )
endif ()

foreach (target star_application_module_interfaces star_application_modules)
  target_include_directories (${target} PRIVATE
      ${STAR_EXTERN_INCLUDES}
      ${STAR_CORE_INCLUDES}
      ${STAR_PLATFORM_INCLUDES}
      ${STAR_APPLICATION_INCLUDES}
    )
  target_link_libraries (${target} PRIVATE star_build_config star_application_dependencies)
  set_target_properties (${target} PROPERTIES CXX_SCAN_FOR_MODULES ON)
endforeach ()
target_link_libraries (star_application_module_interfaces PUBLIC star_modules star_platform_modules)
target_link_libraries (star_application_modules PUBLIC star_application_module_interfaces)

if (STAR_ENABLE_DISCORD_INTEGRATION)
  add_library (star_application OBJECT ${star_application_DISCORD_SOURCES})
  target_include_directories (star_application PRIVATE
      ${STAR_EXTERN_INCLUDES}
      ${STAR_CORE_INCLUDES}
      ${STAR_PLATFORM_INCLUDES}
      ${STAR_APPLICATION_INCLUDES}
    )
  target_link_libraries (star_application PRIVATE star_modules star_build_config star_application_dependencies)
  set_target_properties (star_application PROPERTIES CXX_SCAN_FOR_MODULES ON)
endif ()