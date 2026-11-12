cmake_minimum_required(VERSION 3.28)
project(AsterboundLuaPreflight NONE)

include("${CMAKE_CURRENT_LIST_DIR}/../../../cmake/StarLuaSource.cmake")
if(NOT EXISTS "${star_lua_SOURCE_DIR}/lua.h")
  message(FATAL_ERROR "Incomplete prepatched Lua source: ${star_lua_SOURCE_DIR}")
endif()
# The engine reuses these prepatched sources instead of downloading them again.
if(DEFINED ENV{GITHUB_ENV})
  file(APPEND "$ENV{GITHUB_ENV}"
    "FETCHCONTENT_SOURCE_DIR_STAR_LUA=${star_lua_SOURCE_DIR}\n")
endif()
message(STATUS "Prepared Lua ${lua_version}: ${star_lua_SOURCE_DIR}")
