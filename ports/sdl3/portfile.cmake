# Reuse the fixed registry recipe with corrected options and import metadata.
vcpkg_from_github(
    OUT_SOURCE_PATH upstream
    REPO microsoft/vcpkg
    REF d015e31e90838a4c9dfa3eed45979bc70d9357fc
    SHA512 7304d9591cb925a71e92bf9c783fefd75b78c1aa31f4eb409f4a2d2169e2983f04af0e919cb444832044408c4e77bfb79836d5107c284e78b7dcec5271eadacb
    PATCHES remove-obsolete-console-option.patch
)
set(ASTERBOUND_SDL_IMPLIB_PATCH "${CURRENT_PORT_DIR}/mingw-import-library.patch")
set(overlay_port_dir "${CURRENT_PORT_DIR}")
set(CURRENT_PORT_DIR "${upstream}/ports/sdl3")
include("${CURRENT_PORT_DIR}/portfile.cmake")
set(CURRENT_PORT_DIR "${overlay_port_dir}")
