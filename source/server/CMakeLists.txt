if (STAR_SYSTEM_WINDOWS)
  set (star_server_RESOURCES
      starbound_server.rc
      starbound_server.exe.manifest
    )
endif ()

add_library (star_server_modules STATIC)
target_sources (star_server_modules PUBLIC FILE_SET CXX_MODULES FILES
    StarServerQueryThread.cppm
    StarServerRconThread.cppm)
target_sources(star_server_modules PRIVATE
    StarServerQueryThread.cpp
    StarServerRconThread.cpp)
set_target_properties(star_server_modules PROPERTIES CXX_SCAN_FOR_MODULES ON)
target_include_directories (star_server_modules PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_GAME_INCLUDES}
  )
target_link_libraries (star_server_modules PUBLIC star_modules PRIVATE star_game_module_interfaces star_build_config)

add_executable (starbound_server
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>
  main.cpp ${star_server_RESOURCES})
target_include_directories (starbound_server PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_GAME_INCLUDES}
  )
set_target_properties (starbound_server PROPERTIES
  CXX_SCAN_FOR_MODULES ON
  OUTPUT_NAME asterbound_server
  PDB_NAME asterbound_server
  COMPILE_PDB_NAME asterbound_server)

if(STAR_SYSTEM_MACOS)
  target_link_options (starbound_server PRIVATE "LINKER:-rpath,@executable_path")
elseif (UNIX)
  target_link_options (starbound_server PRIVATE "LINKER:--export-dynamic" "LINKER:-rpath,$ORIGIN")
endif()

target_link_libraries (starbound_server PRIVATE star_base_modules star_server_modules star_game_modules star_build_config
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
