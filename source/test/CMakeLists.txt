set (star_core_tests_SOURCES
      gtest/gtest-all.cc

      core_tests_main.cpp

      algorithm_test.cpp
      block_allocator_test.cpp
      blocks_along_line_test.cpp
      btree_database_test.cpp
      btree_test.cpp
      byte_array_test.cpp
      clock_test.cpp
      color_test.cpp
      container_test.cpp
      encode_test.cpp
      exception_test.cpp
      file_test.cpp
      hash_test.cpp
      host_address_test.cpp
      image_processing_test.cpp
      image_scaling_test.cpp
      ref_ptr_test.cpp
      json_test.cpp
      json_contract_test.cpp
      json_path_patch_test.cpp
      flat_hash_test.cpp
      formatted_json_test.cpp
      line_test.cpp
      lexical_cast_test.cpp
      lua_test.cpp
      lua_json_test.cpp
      lua_syntax_test.cpp
      math_test.cpp
      memory_test.cpp
      multi_table_test.cpp
      net_states_test.cpp
      ordered_map_test.cpp
      ordered_set_test.cpp
      periodic_test.cpp
      poly_test.cpp
      random_test.cpp
      rect_test.cpp
      rpc_promise_test.cpp
      serialization_test.cpp
      static_vector_test.cpp
      small_vector_test.cpp
      sha_test.cpp
      shell_parse.cpp
      string_test.cpp
      strong_typedef_test.cpp
      thread_test.cpp
      worker_pool_test.cpp
      variant_test.cpp
      vlq_test.cpp
    )
add_executable (core_tests
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core>
  ${star_core_tests_SOURCES})
if (STAR_ENABLE_STEAM_INTEGRATION)
  target_sources (core_tests PRIVATE steam_dispatch_test.cpp)
  target_include_directories (core_tests SYSTEM PRIVATE ${STEAM_API_INCLUDE_DIR})
  target_link_libraries (core_tests PRIVATE star_steam_dispatch_modules)
endif ()
# gtest-all.cc includes its implementation files relative to the test directory.
target_include_directories (core_tests PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_GAME_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
# The star libraries are referenced by their objects, which carries no usage
# requirements, so the module target has to be linked in explicitly wherever a
# source imports it.
target_link_libraries (core_tests PRIVATE star_base_modules star_build_config star_vorbis_dependencies star_lua
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
set_target_properties (core_tests PROPERTIES CXX_SCAN_FOR_MODULES ON)
add_test (NAME core_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:core_tests> COMMAND core_tests)

set (star_game_tests_SOURCES
      gtest/gtest-all.cc

      game_tests_main.cpp

      StarTestUniverse.cpp
      StarGameTest.cpp
      assets_test.cpp
      biome_placement_test.cpp
      damage_database_test.cpp
      function_test.cpp
      humanoid_render_test.cpp
      lighting_test.cpp
      item_test.cpp
      liquid_types_test.cpp
      lua_component_test.cpp
      material_types_test.cpp
      movement_controller_test.cpp
      physics_object_test.cpp
      root_test.cpp
      server_test.cpp
      spawn_test.cpp
      species_test.cpp
      stat_test.cpp
      tile_array_test.cpp
      world_geometry_test.cpp
      world_snapshot_test.cpp
      universe_connection_test.cpp
      weather_domain_test.cpp
      wire_processor_test.cpp
      weather_test.cpp
    )
add_executable (game_tests
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>
  ${star_game_tests_SOURCES})
target_sources (game_tests PRIVATE FILE_SET CXX_MODULES FILES StarTestUniverse.cppm StarGameTest.cppm)
target_include_directories (game_tests PRIVATE
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_GAME_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
target_link_libraries (game_tests PRIVATE star_base_modules star_game_modules star_build_config
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions (game_tests PRIVATE STAR_TEST_WRAP_PTHREAD_CREATE=1)
  target_link_options (game_tests PRIVATE "LINKER:--wrap=pthread_create")
endif ()
set_target_properties (game_tests PROPERTIES CXX_SCAN_FOR_MODULES ON)
add_test (NAME game_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:game_tests> COMMAND game_tests)

set_tests_properties(core_tests PROPERTIES
                     LABELS "NoAssets")


if(STAR_BUILD_GUI)
  add_executable(frontend_tests
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>
    gtest/gtest-all.cc game_tests_main.cpp StarTestUniverse.cpp StarGameTest.cpp frontend_test.cpp)
  target_sources(frontend_tests PRIVATE FILE_SET CXX_MODULES FILES StarTestUniverse.cppm StarGameTest.cppm)
  target_include_directories(frontend_tests PRIVATE
    ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES} ${STAR_APPLICATION_INCLUDES}
    ${STAR_RENDERING_INCLUDES} ${STAR_WINDOWING_INCLUDES} ${STAR_FRONTEND_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(frontend_tests PRIVATE
    star_base_modules star_frontend_modules star_application_modules star_build_config
    $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>
    $<LINK_ONLY:star_application_dependencies> $<LINK_ONLY:star_voice_dependencies>)
  set_target_properties(frontend_tests PROPERTIES CXX_SCAN_FOR_MODULES ON)
endif()
