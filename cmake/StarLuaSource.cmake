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
  TIMEOUT 120
  INACTIVITY_TIMEOUT 30
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
