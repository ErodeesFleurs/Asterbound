include_guard(GLOBAL)
include(FetchContent)

set(lua_metadata "${CMAKE_CURRENT_LIST_DIR}/dependencies/lua.json")
set(lua_patch "${CMAKE_CURRENT_LIST_DIR}/patches/lua-openstarbound.patch")
set(lua_patch_script "${CMAKE_CURRENT_LIST_DIR}/StarPatchLua.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${lua_metadata}" "${lua_patch}" "${lua_patch_script}")
file(READ "${lua_metadata}" lua_metadata_json)
string(JSON lua_version GET "${lua_metadata_json}" version)
string(JSON lua_url GET "${lua_metadata_json}" url)
string(JSON lua_sha256 GET "${lua_metadata_json}" sha256)
set(lua_patch_state "${CMAKE_BINARY_DIR}/_deps/star_lua-${lua_sha256}-patch")

# Nix devShells export the standard override, while an explicit -D wins.
if(NOT DEFINED FETCHCONTENT_SOURCE_DIR_STAR_LUA AND
    DEFINED ENV{FETCHCONTENT_SOURCE_DIR_STAR_LUA})
  set(FETCHCONTENT_SOURCE_DIR_STAR_LUA "$ENV{FETCHCONTENT_SOURCE_DIR_STAR_LUA}"
    CACHE PATH "Prepatched Lua source root")
endif()

set(lua_patch_command)
if(NOT FETCHCONTENT_SOURCE_DIR_STAR_LUA)
  find_package(Git REQUIRED)
  set(lua_patch_command
    "${CMAKE_COMMAND}"
    "-DSTAR_LUA_SOURCE_DIR=<SOURCE_DIR>"
    "-DSTAR_LUA_PATCH=${lua_patch}"
    "-DSTAR_LUA_PATCH_STATE=${lua_patch_state}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    -P "${lua_patch_script}")
endif()
FetchContent_Declare(star_lua
  URL "${lua_url}"
  URL_HASH "SHA256=${lua_sha256}"
  TLS_VERIFY TRUE
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  PATCH_COMMAND ${lua_patch_command})
FetchContent_MakeAvailable(star_lua)

# FetchContent population stamps can survive a changed project patch. Check
# every configure; the helper replaces an earlier project patch atomically.
# A SOURCE_DIR override is already patched and may be an immutable Nix store.
if(NOT FETCHCONTENT_SOURCE_DIR_STAR_LUA)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DSTAR_LUA_SOURCE_DIR=${star_lua_SOURCE_DIR}"
    "-DSTAR_LUA_PATCH=${lua_patch}"
    "-DSTAR_LUA_PATCH_STATE=${lua_patch_state}"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    -P "${lua_patch_script}"
    RESULT_VARIABLE lua_patch_result)
  if(NOT lua_patch_result EQUAL 0)
    message(FATAL_ERROR "Applying the OpenStarbound Lua patch failed")
  endif()
endif()

set(lua_sources
  lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c
  ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c
  lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c
  lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c
  lutf8lib.c lvm.c lzio.c)
list(TRANSFORM lua_sources PREPEND "${star_lua_SOURCE_DIR}/")
add_library(star_lua STATIC ${lua_sources})
set_target_properties(star_lua PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(star_lua PUBLIC "${star_lua_SOURCE_DIR}")
# This retains the project's platform, profile and sanitizer policy for Lua C
# code without pulling its implementation into star_extern's object closure.
target_link_libraries(star_lua PRIVATE star_build_config)
if(UNIX)
  target_link_libraries(star_lua PUBLIC m ${CMAKE_DL_LIBS})
endif()
message(STATUS "Using Lua ${lua_version}: ${star_lua_SOURCE_DIR}")
