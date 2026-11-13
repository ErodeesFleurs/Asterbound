# Root version.json owns product, bundled asset and base-game versions.
get_filename_component(asterbound_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(asterbound_version_file "${asterbound_root}/version.json")
file(READ "${asterbound_version_file}" asterbound_metadata)
string(JSON ASTERBOUND_VERSION GET "${asterbound_metadata}" version)
string(JSON ASTERBOUND_ASSET_VERSION GET "${asterbound_metadata}" assetVersion)
string(JSON STARBOUND_VERSION GET "${asterbound_metadata}" starboundVersion)

if(NOT CMAKE_SCRIPT_MODE_FILE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${asterbound_version_file}")
endif()

foreach(bundle asterbound asterbound-compat)
  set(asset_metadata_file "${asterbound_root}/assets/${bundle}/_metadata")
  file(READ "${asset_metadata_file}" asset_metadata)
  string(JSON asset_version GET "${asset_metadata}" version)
  if(NOT asset_version STREQUAL ASTERBOUND_ASSET_VERSION)
    if(ASTERBOUND_SYNC_ASSET_VERSIONS)
      # Keep JSON comments, whitespace and unrelated metadata intact.
      string(REGEX REPLACE "(\"version\"[ \t]*:[ \t]*)\"[^\"]*\""
        "\\1\"${ASTERBOUND_ASSET_VERSION}\"" asset_metadata "${asset_metadata}")
      file(WRITE "${asset_metadata_file}" "${asset_metadata}")
      message(STATUS "Updated ${bundle} assets: ${asset_version} -> ${ASTERBOUND_ASSET_VERSION}")
    else()
      message(FATAL_ERROR
        "${asset_metadata_file} has version ${asset_version}; version.json requires ${ASTERBOUND_ASSET_VERSION}.\n"
        "Run cmake -DASTERBOUND_SYNC_ASSET_VERSIONS=ON -P cmake/AsterboundVersion.cmake from the repository root.")
    endif()
  endif()
  if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${asset_metadata_file}")
  endif()
endforeach()
