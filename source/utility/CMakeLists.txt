function(star_add_utility name with_game)
  add_executable(${name} ${name}.cpp ${ARGN}
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core>)
  target_include_directories(${name} PRIVATE
    ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES})
  target_link_libraries(${name} PRIVATE star_base_modules star_build_config
    $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
  if(with_game)
    target_sources(${name} PRIVATE $<TARGET_OBJECTS:star_game>)
    target_link_libraries(${name} PRIVATE star_game_modules)
  endif()
  set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
endfunction()

foreach(name IN ITEMS asset_packer asset_unpacker btree_repacker)
  star_add_utility(${name} FALSE)
endforeach()

foreach(name IN ITEMS dump_versioned_json make_versioned_json)
  star_add_utility(${name} TRUE)
endforeach()

# Maintenance utilities are kept outside release builds.
if(STAR_BUILD_DEV_TOOLS)
  star_add_utility(update_tilesets TRUE tileset_updater.cpp)
  target_sources(update_tilesets PRIVATE FILE_SET CXX_MODULES FILES tileset_updater.cppm)
  star_add_utility(fix_embedded_tilesets TRUE)
  star_add_utility(map_grep TRUE)
endif()
