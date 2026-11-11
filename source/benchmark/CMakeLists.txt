find_package(benchmark 1.9.5 CONFIG REQUIRED GLOBAL)
get_target_property(benchmark_library_type benchmark::benchmark TYPE)
if(NOT benchmark_library_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "Benchmarks require a static Google Benchmark built with the pinned xclang/runtime")
endif()

function(star_add_core_benchmark name)
  add_executable(${name} ${name}.cpp
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core>)
  target_include_directories(${name} PRIVATE
    ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES})
  target_link_libraries(${name} PRIVATE
    benchmark::benchmark star_base_modules star_build_config star_vorbis_dependencies star_lua
    $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
  set_target_properties(${name} PROPERTIES CXX_SCAN_FOR_MODULES ON)
endfunction()

star_add_core_benchmark(core_benchmarks)
star_add_core_benchmark(json_benchmarks)
star_add_core_benchmark(worker_benchmarks)
star_add_core_benchmark(file_benchmarks)
star_add_core_benchmark(io_benchmarks)
star_add_core_benchmark(logging_benchmarks)
star_add_core_benchmark(array_benchmarks)
star_add_core_benchmark(hash_benchmarks)
star_add_core_benchmark(flat_hash_benchmarks)
star_add_core_benchmark(list_benchmarks)
star_add_core_benchmark(vector_benchmarks)
star_add_core_benchmark(set_benchmarks)
star_add_core_benchmark(string_benchmarks)
star_add_core_benchmark(format_benchmarks)
star_add_core_benchmark(error_benchmarks)
star_add_core_benchmark(unicode_benchmarks)
star_add_core_benchmark(byte_array_benchmarks)
star_add_core_benchmark(bytes_benchmarks)
star_add_core_benchmark(line_benchmarks)
star_add_core_benchmark(image_benchmarks)
star_add_core_benchmark(lru_benchmarks)
target_link_libraries(byte_array_benchmarks PRIVATE
  zlib-ng::zlib
  $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>)
target_sources(file_benchmarks PRIVATE
  file_asset_benchmarks.cpp file_asset_batch_benchmarks.cpp file_commit_benchmarks.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
  star_add_core_benchmark(allocator_benchmarks)
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(allocator_size_code j)
  elseif(MINGW)
    set(allocator_size_code y)
  else()
    set(allocator_size_code m)
  endif()
  # Observe native C++ allocation requests without replacing the allocator.
  foreach(operation IN ITEMS _Znw _Zna)
    target_link_options(allocator_benchmarks PRIVATE "LINKER:--wrap=${operation}${allocator_size_code}")
    foreach(suffix IN ITEMS St11align_val_t RKSt9nothrow_t St11align_val_tRKSt9nothrow_t)
      target_link_options(allocator_benchmarks PRIVATE "LINKER:--wrap=${operation}${allocator_size_code}${suffix}")
    endforeach()
  endforeach()
  foreach(operation IN ITEMS _ZdlPv _ZdaPv)
    target_link_options(allocator_benchmarks PRIVATE "LINKER:--wrap=${operation}")
    foreach(suffix IN ITEMS ${allocator_size_code} St11align_val_t ${allocator_size_code}St11align_val_t
        RKSt9nothrow_t St11align_val_tRKSt9nothrow_t)
      target_link_options(allocator_benchmarks PRIVATE "LINKER:--wrap=${operation}${suffix}")
    endforeach()
  endforeach()
endif()

add_executable(game_benchmarks game_benchmarks.cpp player_storage_benchmarks.cpp
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>)
target_include_directories(game_benchmarks PRIVATE
  ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
  ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES})
target_link_libraries(game_benchmarks PRIVATE
  benchmark::benchmark star_base_modules star_game_modules star_build_config
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
set_target_properties(game_benchmarks PROPERTIES CXX_SCAN_FOR_MODULES ON)

add_executable(ownership_benchmarks ownership_benchmarks.cpp
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>)
target_include_directories(ownership_benchmarks PRIVATE
  ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
  ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES})
target_link_libraries(ownership_benchmarks PRIVATE
  benchmark::benchmark star_base_modules star_game_modules star_build_config
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
set_target_properties(ownership_benchmarks PROPERTIES CXX_SCAN_FOR_MODULES ON)

add_library(star_performance_benchmark_support STATIC)
target_sources(star_performance_benchmark_support PUBLIC FILE_SET CXX_MODULES
  FILES StarPerformanceBenchmarks.cppm)
target_link_libraries(star_performance_benchmark_support PUBLIC
  benchmark::benchmark star_game_modules star_base_modules star_build_config)
set_target_properties(star_performance_benchmark_support PROPERTIES CXX_SCAN_FOR_MODULES ON)

add_executable(performance_benchmarks performance_benchmarks.cpp
  collision_performance.cpp lighting_performance.cpp network_performance.cpp
  liquid_performance.cpp assets_performance.cpp
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>)
target_include_directories(performance_benchmarks PRIVATE
  ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
  ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES})
target_link_libraries(performance_benchmarks PRIVATE
  star_performance_benchmark_support star_build_config
  $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>)
set_target_properties(performance_benchmarks PROPERTIES CXX_SCAN_FOR_MODULES ON)

if(STAR_BUILD_GUI)
  add_executable(frontend_benchmarks frontend_benchmarks.cpp
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>)
  target_include_directories(frontend_benchmarks PRIVATE
    ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES} ${STAR_APPLICATION_INCLUDES}
    ${STAR_RENDERING_INCLUDES} ${STAR_WINDOWING_INCLUDES} ${STAR_FRONTEND_INCLUDES})
  target_link_libraries(frontend_benchmarks PRIVATE
    benchmark::benchmark star_base_modules star_frontend_modules star_application_modules star_build_config
    $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>
    $<LINK_ONLY:star_application_dependencies> $<LINK_ONLY:star_voice_dependencies>)
  set_target_properties(frontend_benchmarks PROPERTIES CXX_SCAN_FOR_MODULES ON)

  add_executable(tile_performance_benchmarks tile_performance.cpp
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_game>)
  if(STAR_ENABLE_DISCORD_INTEGRATION)
    target_sources(tile_performance_benchmarks PRIVATE $<TARGET_OBJECTS:star_application>)
  endif()
  target_include_directories(tile_performance_benchmarks PRIVATE
    ${STAR_EXTERN_INCLUDES} ${STAR_CORE_INCLUDES} ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES} ${STAR_GAME_INCLUDES} ${STAR_APPLICATION_INCLUDES}
    ${STAR_RENDERING_INCLUDES} ${STAR_WINDOWING_INCLUDES} ${STAR_FRONTEND_INCLUDES})
  target_link_libraries(tile_performance_benchmarks PRIVATE
    star_performance_benchmark_support star_rendering_modules star_application_modules star_build_config
    $<LINK_ONLY:star_core_dependencies> $<LINK_ONLY:star_extern_dependencies>
    $<LINK_ONLY:star_application_dependencies> $<LINK_ONLY:star_voice_dependencies>)
  set_target_properties(tile_performance_benchmarks PROPERTIES CXX_SCAN_FOR_MODULES ON)
endif()
