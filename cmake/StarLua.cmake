include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/StarLuaSource.cmake")

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
