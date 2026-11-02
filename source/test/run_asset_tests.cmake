# Asset-backed tests must never inherit a user's storage or boot configuration.
if(NOT TEST_ASSETS)
  set(TEST_ASSETS "$ENV{OPENSTARBOUND_ASSETS}")
endif()
if(NOT TEST_ASSETS)
  message("SKIP: set STAR_TEST_ASSETS or OPENSTARBOUND_ASSETS to owned Starbound assets")
  return()
endif()
get_filename_component(TEST_ASSETS "${TEST_ASSETS}" ABSOLUTE)
if(NOT IS_DIRECTORY "${TEST_ASSETS}")
  message(FATAL_ERROR "Starbound assets directory does not exist: ${TEST_ASSETS}")
endif()
if(EXISTS "${TEST_ASSETS}/packed.pak")
  set(TEST_ASSETS "${TEST_ASSETS}/packed.pak")
endif()

string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef run_id)
set(runtime "${TEST_RUNTIME_ROOT}/${run_id}")
file(MAKE_DIRECTORY "${runtime}/storage" "${runtime}/logs")
file(CHMOD "${runtime}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

function(json_path path output)
  string(REPLACE "\\" "\\\\" escaped "${path}")
  string(REPLACE "\"" "\\\"" escaped "${escaped}")
  string(REPLACE "\n" "\\n" escaped "${escaped}")
  string(REPLACE "\r" "\\r" escaped "${escaped}")
  string(REPLACE "\t" "\\t" escaped "${escaped}")
  set(${output} "\"${escaped}\"" PARENT_SCOPE)
endfunction()

json_path("${TEST_ASSETS}" vanilla)
json_path("${TEST_BUNDLED_ASSETS}" bundled)
json_path("${runtime}/storage" storage)
json_path("${runtime}/logs" logs)
file(WRITE "${runtime}/sbinit.config"
  "{\n  \"assetSources\": [${vanilla}, ${bundled}],\n  \"storageDirectory\": ${storage},\n  \"logDirectory\": ${logs}\n}\n")
message(STATUS "Asset test runtime and reports: ${runtime}")
execute_process(
  COMMAND ${TEST_EMULATOR} "${TEST_EXECUTABLE}" -bootconfig sbinit.config
          "--gtest_output=json:${runtime}/results.json"
  WORKING_DIRECTORY "${runtime}"
  RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Asset tests failed (${result}); runtime: ${runtime}")
endif()
