cmake_minimum_required(VERSION 3.28)
# Remember command-line configuration flags before project() gives cache entries
# their normal types. These values take precedence over project defaults.
foreach(language C CXX)
  foreach(config DEBUG RELWITHASSERTS RELWITHDEBINFO RELEASE)
    set(flag_variable CMAKE_${language}_FLAGS_${config})
    if(NOT DEFINED ${flag_variable})
      unset(STAR_EXPLICIT_${language}_FLAGS_${config} CACHE)
    endif()
    get_property(flag_type CACHE ${flag_variable} PROPERTY TYPE)
    get_property(flag_help CACHE ${flag_variable} PROPERTY HELPSTRING)
    if(flag_type STREQUAL "UNINITIALIZED" OR flag_help MATCHES "specified on the command line")
      set(STAR_EXPLICIT_${language}_FLAGS_${config} TRUE CACHE INTERNAL "Caller-selected configuration flags")
    endif()
  endforeach()
endforeach()

if(NOT DEFINED CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug RelWithAsserts RelWithDebInfo Release CACHE STRING "Available build configurations")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compiler commands")
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
  find_program(STAR_XCLANG_C_COMPILER NAMES clang REQUIRED)
  set(CMAKE_C_COMPILER "${STAR_XCLANG_C_COMPILER}" CACHE FILEPATH "xclang C compiler")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
  find_program(STAR_XCLANG_CXX_COMPILER NAMES clang++ REQUIRED)
  set(CMAKE_CXX_COMPILER "${STAR_XCLANG_CXX_COMPILER}" CACHE FILEPATH "xclang C++ compiler")
endif()
if(NOT DEFINED CMAKE_BUILD_RPATH AND DEFINED ENV{ASTERBOUND_BUILD_RPATH})
  set(CMAKE_BUILD_RPATH "$ENV{ASTERBOUND_BUILD_RPATH}" CACHE STRING "Dependency runtime search paths")
endif()
project(starbound)

set(asterbound_version_file "${PROJECT_SOURCE_DIR}/../version.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${asterbound_version_file}")
file(READ "${asterbound_version_file}" asterbound_metadata)
string(JSON ASTERBOUND_VERSION GET "${asterbound_metadata}" version)
string(JSON ASTERBOUND_ASSET_VERSION GET "${asterbound_metadata}" assetVersion)
string(JSON STARBOUND_VERSION GET "${asterbound_metadata}" starboundVersion)
string(REPLACE "." "," ASTERBOUND_VERSION_COMMA "${ASTERBOUND_VERSION}")
configure_file("${PROJECT_SOURCE_DIR}/../cmake/AsterboundVersion.h.in"
  "${PROJECT_BINARY_DIR}/AsterboundVersion.h" @ONLY)


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../cmake")

# Every platform uses xclang's C++23 frontend and bundled libc++.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <version>
#ifndef _LIBCPP_VERSION
#error Asterbound requires xclang's bundled libc++
#endif
int main() { return 0; }" STAR_USES_LIBCXX)
if(NOT STAR_USES_LIBCXX)
  message(FATAL_ERROR "Asterbound requires xclang's bundled libc++; use nix develop or the xclang presets.")
endif()
# Only the targets that import modules opt into scanning; module file sets are
# scanned regardless. Avoid scanning third-party translation units.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)


# Discover all the relevant environment / system information and place the
# result in STAR_* cmake variables.

# STAR_SOURCE_IDENTIFIER may be set to any string value
if(NOT DEFINED STAR_SOURCE_IDENTIFIER)
  include(GetGitRevisionDescription)
  get_git_head_revision(STAR_GIT_REFSPEC STAR_GIT_HASHVAR)
  set(STAR_SOURCE_IDENTIFIER "${STAR_GIT_HASHVAR}")
endif()

# Architecture identifier, like i386, x86_64 or ppc
if(NOT DEFINED STAR_ARCHITECTURE)
  include(TargetArch)
  target_architecture(STAR_ARCHITECTURE)
endif()

# System name, like windows, macos, linux, freebsd, or (generic) unix
if(NOT DEFINED STAR_SYSTEM)
  if(WIN32)
    set(STAR_SYSTEM "windows")
  elseif(APPLE AND ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(STAR_SYSTEM "macos")
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(STAR_SYSTEM "linux")
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(STAR_SYSTEM "freebsd")
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "NetBSD")
    set(STAR_SYSTEM "netbsd")
  elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Haiku")
    set(STAR_SYSTEM "haiku")
  elseif(UNIX)
    set(STAR_SYSTEM "unix")
  else()
    set(STAR_SYSTEM "unknown")
  endif()
endif()

if(NOT DEFINED STAR_SYSTEM_FAMILY)
  if(WIN32)
    set(STAR_SYSTEM_FAMILY "windows")
  elseif(UNIX)
    set(STAR_SYSTEM_FAMILY "unix")
  else()
    set(STAR_SYSTEM_FAMILY "unknown")
  endif()
endif()

foreach(language C CXX)
  if(NOT CMAKE_${language}_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Asterbound requires xclang for both C and C++.")
  endif()
  execute_process(COMMAND "${CMAKE_${language}_COMPILER}" --version
    OUTPUT_VARIABLE compiler_version RESULT_VARIABLE compiler_version_result)
  if(NOT compiler_version_result EQUAL 0 OR NOT compiler_version MATCHES "^clang version 23\\.1\\.3 [(]https://github[.]com/llvm/llvm-project 0d261d1ca552c95a8f007e061c787ac7132fbcbc[)]")
    message(FATAL_ERROR "The ${language} compiler does not match the pinned xclang LLVM revision; install the pinned toolchain or use nix develop.")
  endif()
endforeach()
get_filename_component(compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(STAR_XCLANG_COMMAND NAMES xclang
  HINTS "$ENV{XCLANG_ROOT}/bin" "${compiler_directory}" NO_DEFAULT_PATH REQUIRED)
execute_process(COMMAND "${STAR_XCLANG_COMMAND}" --version
  OUTPUT_VARIABLE xclang_version RESULT_VARIABLE xclang_version_result)
if(NOT xclang_version_result EQUAL 0 OR NOT xclang_version MATCHES "^xclang 23\\.1\\.3\\.6[\r\n]")
  message(FATAL_ERROR "Asterbound requires the pinned xclang release 23.1.3.6.")
endif()
set(STAR_COMPILER "clang")

# Enable OPTIONs based on the discovered system / environment...

option(STAR_BUILD_GUI "Build GUI utilities and Client" ON)

if(STAR_BUILD_GUI)
  option(STAR_BUILD_QT_TOOLS "Build GUI utilities that require Qt" OFF)
  option(STAR_ENABLE_STEAM_INTEGRATION "Use Steam platform services" OFF)
  option(STAR_ENABLE_DISCORD_INTEGRATION "Use Discord platform services" OFF)
endif()

set(STAR_DISCORD_APPLICATION_ID "" CACHE STRING "Your registered Discord application ID")
if(STAR_ENABLE_DISCORD_INTEGRATION)
  string(LENGTH "${STAR_DISCORD_APPLICATION_ID}" discord_id_length)
  if(NOT STAR_DISCORD_APPLICATION_ID MATCHES "^[1-9][0-9]*$"
      OR discord_id_length GREATER 19
      OR (discord_id_length EQUAL 19 AND STAR_DISCORD_APPLICATION_ID STRGREATER "9223372036854775807"))
    message(FATAL_ERROR "Discord integration requires your own positive int64 STAR_DISCORD_APPLICATION_ID")
  endif()
endif()

option(STAR_BUILD_DEV_TOOLS "Build the maintenance utilities (map_grep, tileset tools)" OFF)

option(STAR_LUA_APICHECK "Use lua api checks" OFF)

option(STAR_ENABLE_THINLTO "Enable ThinLTO for optimized configurations" ON)

# "default" preserves Clang's line-table-only RelWithDebInfo behavior; "full"
# requests full symbols and "minimal" is useful for sanitizer/assert packages.
set(STAR_DEBUG_SYMBOL_LEVEL "default" CACHE STRING "Debug symbols: default, minimal, or full")
set_property(CACHE STAR_DEBUG_SYMBOL_LEVEL PROPERTY STRINGS default minimal full)
if(NOT STAR_DEBUG_SYMBOL_LEVEL MATCHES "^(default|minimal|full)$")
  message(FATAL_ERROR "STAR_DEBUG_SYMBOL_LEVEL must be default, minimal, or full")
endif()

add_library(star_build_config INTERFACE)
target_compile_definitions(star_build_config INTERFACE "$<$<PLATFORM_ID:Windows>:NOMINMAX>")
target_include_directories(star_build_config INTERFACE "${PROJECT_BINARY_DIR}")
if(STAR_ENABLE_DISCORD_INTEGRATION)
  target_compile_definitions(star_build_config INTERFACE
    "STAR_DISCORD_APPLICATION_ID=${STAR_DISCORD_APPLICATION_ID}ULL")
endif()
add_library(star_core_dependencies INTERFACE)
add_library(star_module_dependencies INTERFACE)
add_library(star_extern_dependencies INTERFACE)
add_library(star_vorbis_dependencies INTERFACE)

# Report all the discovered system / environment settings and all options.

message(STATUS "Source ID: ${STAR_SOURCE_IDENTIFIER}")
message(STATUS "Architecture: ${STAR_ARCHITECTURE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "System: ${STAR_SYSTEM}")
message(STATUS "System family: ${STAR_SYSTEM_FAMILY}")
message(STATUS "C/C++ compiler: ${STAR_COMPILER}")


message(STATUS "Building GUI: ${STAR_BUILD_GUI}")

if(DEFINED STAR_BUILD_QT_TOOLS)
  message(STATUS "Building Qt tools: ${STAR_BUILD_QT_TOOLS}")
endif()

if(DEFINED STAR_ENABLE_STEAM_INTEGRATION)
  message(STATUS "Using Steam platform services: ${STAR_ENABLE_STEAM_INTEGRATION}")
endif()

if(DEFINED STAR_ENABLE_DISCORD_INTEGRATION)
  message(STATUS "Using Discord platform services: ${STAR_ENABLE_DISCORD_INTEGRATION}")
endif()

message(STATUS "Using Lua API checks: ${STAR_LUA_APICHECK}")
message(STATUS "Allocator: mimalloc (static)")

# Set C defines and cmake variables based on the build settings we have now
# determined...

# Set a cmake variable to true and define a corresponding C/C++ definition
function(SET_FLAG flagValue)
  set(${flagValue} TRUE PARENT_SCOPE)
  target_compile_definitions(star_build_config INTERFACE ${flagValue})
endfunction()

if(STAR_ARCHITECTURE STREQUAL "i386")
  set_flag(STAR_ARCHITECTURE_I386)
elseif(STAR_ARCHITECTURE STREQUAL "x86_64")
  set_flag(STAR_ARCHITECTURE_X86_64)
endif()

if(STAR_SYSTEM STREQUAL "windows")
  set_flag(STAR_SYSTEM_WINDOWS)
elseif(STAR_SYSTEM STREQUAL "macos")
  set_flag(STAR_SYSTEM_MACOS)
elseif(STAR_SYSTEM STREQUAL "linux")
  set_flag(STAR_SYSTEM_LINUX)
elseif(STAR_SYSTEM STREQUAL "freebsd")
  set_flag(STAR_SYSTEM_FREEBSD)
elseif(STAR_SYSTEM STREQUAL "netbsd")
  set_flag(STAR_SYSTEM_NETBSD)
elseif(STAR_SYSTEM STREQUAL "haiku")
  set_flag(STAR_SYSTEM_HAIKU)
endif()

if(STAR_SYSTEM_FAMILY STREQUAL "windows")
  set_flag(STAR_SYSTEM_FAMILY_WINDOWS)
elseif(STAR_SYSTEM_FAMILY STREQUAL "unix")
  set(STAR_SYSTEM_FAMILY_UNIX TRUE)
endif()

set_flag(STAR_COMPILER_CLANG)

if(STAR_LUA_APICHECK)
  target_compile_definitions(star_build_config INTERFACE LUA_USE_APICHECK)
endif()

if(STAR_SYSTEM_WINDOWS)
  # LUA_USE_WINDOWS is automatically defined in luaconf if _WIN32 is defined
elseif(STAR_SYSTEM_MACOS)
  target_compile_definitions(star_build_config INTERFACE LUA_USE_MACOSX)
elseif(STAR_SYSTEM_LINUX)
  target_compile_definitions(star_build_config INTERFACE LUA_USE_LINUX)
elseif(STAR_SYSTEM_FAMILY_UNIX)
  target_compile_definitions(star_build_config INTERFACE LUA_USE_POSIX)
endif()

if(STAR_ENABLE_STEAM_INTEGRATION)
  target_compile_definitions(star_build_config INTERFACE STAR_ENABLE_STEAM_INTEGRATION)
endif()

if(STAR_ENABLE_DISCORD_INTEGRATION)
  target_compile_definitions(star_build_config INTERFACE STAR_ENABLE_DISCORD_INTEGRATION)
endif()


# Compiler/configuration defaults belong only to in-tree compiled targets.
include(StarBuildOptions)
if(STAR_SYSTEM_LINUX)
  target_include_directories(star_build_config SYSTEM BEFORE INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/../cmake/standard_headers>")
endif()
include(StarStd)
include(StarLua)
target_link_libraries(star_core_dependencies INTERFACE star_lua)
target_link_libraries(star_module_dependencies INTERFACE star_lua)
target_link_libraries(star_extern_dependencies INTERFACE star_lua)
if(NOT DEFINED CMAKE_SKIP_BUILD_RPATH)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
endif()

# External usage requirements belong to their implementation layer, not to
# star_build_config. Object expressions do not forward them; executable targets
# explicitly link the relevant groups with LINK_ONLY for their object closure.
if(STAR_SYSTEM_WINDOWS)
  enable_language(RC)
  target_link_libraries(star_core_dependencies INTERFACE ws2_32 iphlpapi shlwapi dbghelp)
  target_link_libraries(star_module_dependencies INTERFACE ws2_32 iphlpapi dbghelp)
elseif(STAR_SYSTEM_LINUX)
  target_link_libraries(star_core_dependencies INTERFACE pthread ${CMAKE_DL_LIBS} rt)
elseif(STAR_SYSTEM_FREEBSD)
  target_link_libraries(star_core_dependencies INTERFACE pthread rt)
elseif(STAR_SYSTEM_NETBSD)
  target_link_libraries(star_core_dependencies INTERFACE pthread rt execinfo)
elseif(STAR_SYSTEM_HAIKU)
  target_link_libraries(star_core_dependencies INTERFACE pthread execinfo network)
  target_link_libraries(star_module_dependencies INTERFACE network)
endif()
target_link_libraries(star_module_dependencies INTERFACE ${CMAKE_DL_LIBS})
target_link_libraries(star_extern_dependencies INTERFACE ${CMAKE_DL_LIBS})

find_package(mimalloc CONFIG REQUIRED)
target_link_libraries(star_core_dependencies INTERFACE mimalloc-static)

find_package(zlib-ng CONFIG REQUIRED)
# Keep project-wide definitions identical across module providers and importers.
target_compile_definitions(star_build_config INTERFACE WITH_GZFILEOP)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(OggVorbis REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
if(NOT STAR_SYSTEM_HAIKU)
  find_package(re2 CONFIG REQUIRED)
endif()

target_include_directories(star_vorbis_dependencies SYSTEM INTERFACE
  ${VORBIS_INCLUDE_DIR} ${OGG_INCLUDE_DIR})
target_link_libraries(star_vorbis_dependencies INTERFACE
  ${VORBISFILE_LIBRARY} ${VORBIS_LIBRARY} ${OGG_LIBRARY})

target_include_directories(star_core_dependencies SYSTEM INTERFACE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(star_core_dependencies INTERFACE
  PNG::PNG
  $<IF:$<TARGET_EXISTS:Freetype::Freetype>,Freetype::Freetype,freetype>
  star_vorbis_dependencies
  $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
  $<IF:$<TARGET_EXISTS:re2::re2>,re2::re2,re2>)
target_link_libraries(star_module_dependencies INTERFACE
  zlib-ng::zlib cpr::cpr star_vorbis_dependencies)
# Lua's ImGui bindings are available in headless builds too.
target_link_libraries(star_extern_dependencies INTERFACE imgui::imgui)

if(STAR_SYSTEM_FAMILY STREQUAL "unix" AND NOT STAR_SYSTEM_HAIKU)
  set_flag(STAR_USE_CPPTRACE)
  find_package(cpptrace CONFIG REQUIRED)
  target_link_libraries(star_core_dependencies INTERFACE cpptrace::cpptrace)
  message(STATUS "Using cpptrace")
endif()

if(STAR_BUILD_GUI)
  add_library(star_application_dependencies INTERFACE)
  add_library(star_voice_dependencies INTERFACE)
  add_library(star_client_dependencies INTERFACE)

  find_package(SDL3 CONFIG REQUIRED)
  set(STAR_SDL_TARGET $<IF:$<TARGET_EXISTS:SDL3::SDL3>,SDL3::SDL3,SDL3::SDL3-static>)
  find_package(OpenGL REQUIRED)
  find_package(GLEW REQUIRED)
  if(NOT STAR_SYSTEM_HAIKU)
    find_package(Opus CONFIG REQUIRED)
  endif()

  target_link_libraries(star_application_dependencies INTERFACE
    ${STAR_SDL_TARGET}
    OpenGL::GL
    $<IF:$<TARGET_EXISTS:GLEW::glew_s>,GLEW::glew_s,GLEW::GLEW>
    $<IF:$<TARGET_EXISTS:imgui::imgui_backends>,imgui::imgui_backends,imgui::imgui>)
  target_link_libraries(star_voice_dependencies INTERFACE
    ${STAR_SDL_TARGET}
    $<IF:$<TARGET_EXISTS:Opus::opus>,Opus::opus,opus>)
  target_include_directories(star_client_dependencies SYSTEM INTERFACE ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(star_client_dependencies INTERFACE
    imgui::imgui
    $<IF:$<TARGET_EXISTS:Freetype::Freetype>,Freetype::Freetype,freetype>
    $<TARGET_NAME_IF_EXISTS:SDL3::SDL3main>)

  if(STAR_SYSTEM_MACOS)
    target_link_libraries(star_application_dependencies INTERFACE "-framework CoreAudio")
  endif()

  if(STAR_ENABLE_STEAM_INTEGRATION)
    find_package(SteamApi REQUIRED)
    add_library(star_steam_dependencies INTERFACE)
    target_include_directories(star_steam_dependencies SYSTEM INTERFACE ${STEAM_API_INCLUDE_DIR})
    target_link_libraries(star_steam_dependencies INTERFACE ${STEAM_API_LIBRARY})
    target_link_libraries(star_application_dependencies INTERFACE star_steam_dependencies)
  endif()

  if(STAR_ENABLE_DISCORD_INTEGRATION)
    find_package(DiscordApi REQUIRED)
    target_link_libraries(star_application_dependencies INTERFACE ${DISCORD_API_LIBRARY})
  endif()
endif()

# Set basic build flags, include all the relevant source directories, based on
# build settings...

set(BUILD_SHARED_LIBS false)

# First set output dir for the generic no-config case (e.g. macos / linux)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../dist)

# Second, set output dir for multi-config builds (e.g. msvc)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${PROJECT_SOURCE_DIR}/../dist)
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

configure_file("${PROJECT_SOURCE_DIR}/../doc/OPENSOURCE.md"
  "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/OPENSOURCE.md" COPYONLY)

if(STAR_SYSTEM_WINDOWS AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  # vcpkg's Windows app-local helper requires a Windows PowerShell host.
  file(GLOB STAR_WINDOWS_RUNTIME_DLLS
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll"
    "${PROJECT_SOURCE_DIR}/../lib/windows/*.dll")
  file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  foreach(runtime_dll IN LISTS STAR_WINDOWS_RUNTIME_DLLS)
    get_filename_component(runtime_dll_name "${runtime_dll}" NAME)
    file(COPY_FILE "${runtime_dll}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${runtime_dll_name}" ONLY_IF_DIFFERENT)
  endforeach()
endif()

# External code included with starbound source, which core depends on
set(STAR_EXTERN_INCLUDES ${PROJECT_SOURCE_DIR}/extern)
add_subdirectory(extern)

# Core support code, not specific to starbound.
set(STAR_CORE_INCLUDES
    ${PROJECT_SOURCE_DIR}/core
    ${PROJECT_SOURCE_DIR}/core/scripting
)
add_subdirectory(core)

# Less general purpose code than core that is available to both the game and
# application modules.
set(STAR_BASE_INCLUDES
    ${PROJECT_SOURCE_DIR}/base
    ${PROJECT_SOURCE_DIR}/base/scripting
)
add_subdirectory(base)

# Platform APIs that are implemented by the application module
set(STAR_PLATFORM_INCLUDES ${PROJECT_SOURCE_DIR}/platform)
add_subdirectory(platform)

# Core game logic used by both server and client.
set(STAR_GAME_INCLUDES
    ${PROJECT_SOURCE_DIR}/game
    ${PROJECT_SOURCE_DIR}/game/interfaces
    ${PROJECT_SOURCE_DIR}/game/items
    ${PROJECT_SOURCE_DIR}/game/objects
    ${PROJECT_SOURCE_DIR}/game/scripting
    ${PROJECT_SOURCE_DIR}/game/terrain
  )
add_subdirectory(game)

# Googletest based tests
option(BUILD_TESTING "Build test projects" OFF)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

option(STAR_BUILD_BENCHMARKS "Build standalone performance benchmarks" OFF)
if(STAR_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

# Starbound stand-alone server.
add_subdirectory(server)

# cmdline utilities
add_subdirectory(utility)

if(STAR_BUILD_GUI)
  # Handles creating windows, keyboard / mouse / joystick input, and the 2d
  # rendering model.
  set(STAR_APPLICATION_INCLUDES ${PROJECT_SOURCE_DIR}/application)
  add_subdirectory(application)

  # Rendering code not dependent on widget system
  set(STAR_RENDERING_INCLUDES ${PROJECT_SOURCE_DIR}/rendering)
  add_subdirectory(rendering)

  # Panes and Widgets
  set(STAR_WINDOWING_INCLUDES ${PROJECT_SOURCE_DIR}/windowing)
  add_subdirectory(windowing)

  # Client interface code
  set(STAR_FRONTEND_INCLUDES ${PROJECT_SOURCE_DIR}/frontend)
  add_subdirectory(frontend)

  # Starbound game / client
  add_subdirectory(client)

  # Qt GUI tools
  if(STAR_BUILD_QT_TOOLS)
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    get_target_property(qt_core_type Qt5::Core TYPE)
    if(NOT qt_core_type STREQUAL "STATIC_LIBRARY")
      message(FATAL_ERROR "Qt tools require static Qt built with xclang's bundled libc++.")
    endif()
    if(STAR_SYSTEM_WINDOWS)
      set(STAR_QT_PLATFORM_PLUGIN Qt5::QWindowsIntegrationPlugin)
    elseif(STAR_SYSTEM_MACOS)
      set(STAR_QT_PLATFORM_PLUGIN Qt5::QCocoaIntegrationPlugin)
    else()
      set(STAR_QT_PLATFORM_PLUGIN Qt5::QXcbIntegrationPlugin)
    endif()
    function(star_import_qt_plugins target)
      qt5_import_plugins(${target} INCLUDE ${STAR_QT_PLATFORM_PLUGIN})
    endfunction()
    function(star_module_moc target interface output)
      set(moc_includes)
      foreach(qt_target Qt5::Core Qt5::Gui Qt5::Widgets)
        get_target_property(qt_includes ${qt_target} INTERFACE_INCLUDE_DIRECTORIES)
        foreach(directory IN LISTS qt_includes)
          list(APPEND moc_includes "-I${directory}")
        endforeach()
      endforeach()
      set(moc_source "${CMAKE_CURRENT_SOURCE_DIR}/${interface}")
      set(moc_output "${CMAKE_CURRENT_BINARY_DIR}/${output}")
      add_custom_command(OUTPUT "${moc_output}"
        COMMAND Qt5::moc -i ${moc_includes}
          -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB
          "${moc_source}" -o "${moc_output}"
        DEPENDS "${moc_source}" Qt5::moc
        VERBATIM)
      set_source_files_properties("${moc_output}" PROPERTIES HEADER_FILE_ONLY TRUE)
      target_sources(${target} PRIVATE "${moc_output}")
    endfunction()
    add_subdirectory(json_tool)

    if(STAR_ENABLE_STEAM_INTEGRATION)
      add_subdirectory(mod_uploader)
    endif()
  endif()
endif()

# Check the complete target graph, including optional tools and tests, only once
# all subdirectories have declared their source and usage requirements.
cmake_language(DEFER CALL star_check_language_directory "${PROJECT_SOURCE_DIR}")


