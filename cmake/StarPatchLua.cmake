cmake_minimum_required(VERSION 3.28)

foreach(required STAR_LUA_SOURCE_DIR STAR_LUA_PATCH STAR_LUA_PATCH_STATE GIT_EXECUTABLE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Lua patch helper requires ${required}")
  endif()
endforeach()
if(NOT EXISTS "${STAR_LUA_SOURCE_DIR}/lua.h")
  message(FATAL_ERROR "Not a complete Lua source root: ${STAR_LUA_SOURCE_DIR}")
endif()
file(MAKE_DIRECTORY "${STAR_LUA_PATCH_STATE}")
set(previous_patch "${STAR_LUA_PATCH_STATE}/applied.patch")
get_filename_component(source_parent "${STAR_LUA_SOURCE_DIR}" DIRECTORY)

function(lua_git_apply result diagnostic)
  # Do not discover a surrounding OpenStarbound Git checkout when the fetched
  # source lives inside its build directory. git apply needs no repository.
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    --unset=GIT_DIR --unset=GIT_WORK_TREE
    "GIT_CEILING_DIRECTORIES=${source_parent}"
    "${GIT_EXECUTABLE}" apply --whitespace=nowarn ${ARGN}
    WORKING_DIRECTORY "${STAR_LUA_SOURCE_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(${result} "${status}" PARENT_SCOPE)
  set(${diagnostic} "${output}${error}" PARENT_SCOPE)
endfunction()

# A reverse check of a smaller patch also succeeds against a larger old patch.
# Compare the recorded patch first so removed hunks cannot survive an update.
set(removed_previous FALSE)
if(EXISTS "${previous_patch}")
  file(SHA256 "${previous_patch}" previous_hash)
  file(SHA256 "${STAR_LUA_PATCH}" current_hash)
  if(NOT previous_hash STREQUAL current_hash)
    lua_git_apply(check_result check_error --reverse --check "${previous_patch}")
    if(NOT check_result EQUAL 0)
      message(FATAL_ERROR "Cannot replace the recorded Lua patch:\n${check_error}")
    endif()
    lua_git_apply(remove_result remove_error --reverse "${previous_patch}")
    if(NOT remove_result EQUAL 0)
      message(FATAL_ERROR "Removing the previous Lua patch failed:\n${remove_error}")
    endif()
    set(removed_previous TRUE)
  endif()
endif()

lua_git_apply(forward_result forward_error --check "${STAR_LUA_PATCH}")
if(forward_result EQUAL 0)
  lua_git_apply(apply_result apply_error "${STAR_LUA_PATCH}")
elseif(removed_previous)
  set(apply_result "${forward_result}")
  set(apply_error "${forward_error}")
else()
  lua_git_apply(apply_result apply_error --reverse --check "${STAR_LUA_PATCH}")
endif()
if(NOT apply_result EQUAL 0)
  if(removed_previous)
    lua_git_apply(restore_result restore_error "${previous_patch}")
    if(NOT restore_result EQUAL 0)
      message(FATAL_ERROR "New Lua patch failed and restoring the previous patch also failed:\n${apply_error}\n${restore_error}")
    endif()
    message(FATAL_ERROR "New Lua patch failed; restored the previous patch:\n${apply_error}")
  endif()
  message(FATAL_ERROR "Lua patch conflicts with fetched sources:\n${forward_error}\n${apply_error}")
endif()
file(COPY_FILE "${STAR_LUA_PATCH}" "${previous_patch}" ONLY_IF_DIFFERENT)
