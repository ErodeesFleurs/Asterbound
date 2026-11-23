add_library (star_frontend_module_interfaces OBJECT)
target_sources (star_frontend_module_interfaces PUBLIC FILE_SET CXX_MODULES FILES
    StarSimpleTooltip.cppm
    StarHttpTrustDialog.cppm
    StarItemTooltip.cppm
    StarClipboardLuaBindings.cppm
    StarVoiceLuaBindings.cppm
    StarInterfaceLuaBindings.cppm
    StarBindingsMenu.cppm
    StarConfirmationDialog.cppm
    StarErrorScreen.cppm
    StarGraphicsMenu.cppm
    StarJoinRequestDialog.cppm
    StarModsMenu.cppm
    StarOptionsMenu.cppm
    StarShadersMenu.cppm
    StarTeleportDialog.cppm
    StarVoiceSettingsMenu.cppm
    StarCharCreation.cppm
    StarCharSelection.cppm
    StarAiInterface.cppm
    StarBaseScriptPane.cppm
    StarChatBubbleSeparation.cppm
    StarChatBubbleManager.cppm
    StarContainerInterface.cppm
    StarContainerInteractor.cppm
    StarCraftingInterface.cppm
    StarInventory.cppm
    StarInterfaceCursor.cppm
    StarMainInterfaceTypes.cppm
    StarMainMixer.cppm
    StarMerchantInterface.cppm
    StarNameplatePainter.cppm
    StarQuestIndicatorPainter.cppm
    StarScriptPane.cppm
    StarWireInterface.cppm
    StarCinematic.cppm
    StarClientCommandProcessor.cppm
    StarMainInterface.cppm
    StarTitleScreen.cppm
    StarVoice.cppm
  )
target_include_directories (star_frontend_module_interfaces PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_GAME_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_APPLICATION_INCLUDES}
    ${STAR_RENDERING_INCLUDES}
    ${STAR_WINDOWING_INCLUDES}
    ${STAR_FRONTEND_INCLUDES}
  )
target_link_libraries (star_frontend_module_interfaces PUBLIC star_game_module_interfaces star_application_module_interfaces star_rendering_module_interfaces star_windowing_module_interfaces PRIVATE star_build_config)

add_library (star_frontend_modules STATIC
    StarInterfaceLuaBindings.cpp
    StarHttpTrustDialog.cpp
    StarVoiceLuaBindings.cpp
    StarClipboardLuaBindings.cpp
    StarBindingsMenu.cpp
    StarConfirmationDialog.cpp
    StarErrorScreen.cpp
    StarGraphicsMenu.cpp
    StarJoinRequestDialog.cpp
    StarModsMenu.cpp
    StarOptionsMenu.cpp
    StarShadersMenu.cpp
    StarTeleportDialog.cpp
    StarVoiceSettingsMenu.cpp
    StarCharCreation.cpp
    StarCharSelection.cpp
    StarAiInterface.cpp
    StarBaseScriptPane.cpp
    StarChatBubbleSeparation.cpp
    StarChatBubbleManager.cpp
    StarContainerInterface.cpp
    StarContainerInteractor.cpp
    StarCraftingInterface.cpp
    StarInventory.cpp
    StarInterfaceCursor.cpp
    StarMainInterfaceTypes.cpp
    StarMainMixer.cpp
    StarMerchantInterface.cpp
    StarNameplatePainter.cpp
    StarQuestIndicatorPainter.cpp
    StarScriptPane.cpp
    StarWireInterface.cpp
    StarCinematic.cpp
    StarClientCommandProcessor.cpp
    StarMainInterface.cpp
    StarTitleScreen.cpp
    StarVoice.cpp
  )
target_include_directories (star_frontend_modules PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_GAME_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_APPLICATION_INCLUDES}
    ${STAR_RENDERING_INCLUDES}
    ${STAR_WINDOWING_INCLUDES}
    ${STAR_FRONTEND_INCLUDES}
  )
target_link_libraries (star_frontend_modules PRIVATE
    $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)
target_link_libraries (star_frontend_modules PUBLIC star_frontend_module_interfaces star_game_modules star_rendering_modules star_windowing_modules star_application_modules PRIVATE star_build_config star_voice_dependencies)
set_target_properties (star_frontend_modules PROPERTIES CXX_SCAN_FOR_MODULES ON)

