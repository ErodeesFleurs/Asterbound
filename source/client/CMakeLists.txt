set (star_client_SOURCES
      StarClientApplication.cpp
      StarRenderingLuaBindings.cpp
    )

if (STAR_SYSTEM_WINDOWS)
  set (star_client_RESOURCES
      starbound.rc
      starbound.exe.manifest
    )
endif ()

add_executable (starbound WIN32
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>
  ${star_client_SOURCES} ${star_client_RESOURCES})
if (STAR_ENABLE_DISCORD_INTEGRATION)
  target_sources (starbound PRIVATE $<TARGET_OBJECTS:star_application>)
endif ()
target_include_directories (starbound PRIVATE
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
add_library (star_client_module_interfaces OBJECT)
target_sources (star_client_module_interfaces PUBLIC FILE_SET CXX_MODULES FILES
    StarClientApplication.cppm
    StarRenderingLuaBindings.cppm
  )
target_link_libraries (star_client_module_interfaces PUBLIC
  star_base_modules star_frontend_modules star_application_modules
  PRIVATE star_build_config star_client_dependencies)

if(STAR_SYSTEM_MACOS)
  target_link_options (starbound PRIVATE "LINKER:-rpath,@executable_path")
elseif (UNIX)
  target_link_options (starbound PRIVATE "LINKER:--export-dynamic" "LINKER:-rpath,$ORIGIN")
endif()

if (STAR_SYSTEM_MACOS)
  set_target_properties (starbound PROPERTIES XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES)
endif()

target_link_libraries (starbound PRIVATE
  star_client_module_interfaces
  star_base_modules star_frontend_modules star_application_modules star_build_config star_client_dependencies
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>
  $<LINK_ONLY:star_application_dependencies> $<LINK_ONLY:star_voice_dependencies>)
set_target_properties (starbound PROPERTIES
  CXX_SCAN_FOR_MODULES ON
  OUTPUT_NAME asterbound
  PDB_NAME asterbound
  COMPILE_PDB_NAME asterbound)

