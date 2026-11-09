{
  lib,
  stdenv,
  stdenvNoCC,
  fetchurl,
  callPackage,
  cmake,
  ninja,
  python3,
  pkg-config,
  autoPatchelfHook,
  zlib-ng,
  dependencies,
  libpng,
  freetype,
  libogg,
  libvorbis,
  zstd,
  libopus,
  sdl3,
  glew,
  libGL,
  # This repository's source tree; the CMake layout expects the repository root
  # (source/CMakeLists.txt is configured through `cmakeDir`).
  src,
  # Recorded into the built-in version string (shown in logs and the window
  # title); normally the git revision the tree was built from.
  sourceIdentifier ? "unknown",
  guiSupport ? true,
  steamSupport ? guiSupport && stdenv.hostPlatform.isx86_64,
  discordSupport ? guiSupport && stdenv.hostPlatform.isx86_64,
  qtSupport ? false,
  withTests ? true,
  withBenchmarks ? false,
  # Build with AddressSanitizer + UndefinedBehaviorSanitizer.  The checkPhase
  # then fails on the first report, so `nix build .#sanitized` is a hard gate
  # for undefined behaviour in the code the tests cover; the game_tests sweep
  # runs through `nix run .#sanitized-tests`.
  sanitizers ? false,
  # Build with the project's own assertions live.  RelWithAsserts leaves NDEBUG
  # undefined, so starAssert and the DebugEnabled code paths actually execute in
  # the checkPhase and in `nix run .#asserts-tests`; the shipped configurations
  # all define NDEBUG, so nothing else ever runs them.
  asserts ? false,
}:

let
  inherit (dependencies)
    mimalloc
    libcpr
    re2
    cpptrace
    qt5
    sdkRuntime
    ;
  # Version lived in source/core/StarVersion.cpp.in.  It must match the
  # "version" field in assets/opensb/_metadata or Root refuses to load assets.
  version = "0.1.15.1";
  sdkExtension = stdenv.hostPlatform.extensions.sharedLibrary;
  opusCmake = callPackage ./opus-cmake-config.nix { };
  luaMetadata = builtins.fromJSON (builtins.readFile ../cmake/dependencies/lua.json);
  # Isolate the patch's store identity from unrelated edits to the flake source.
  luaPatch = builtins.path {
    path = ../cmake/patches/lua-openstarbound.patch;
    name = "openstarbound-lua.patch";
  };
  # Fetch and patch sources only: the project's compiler builds the Lua target,
  # including sanitizer instrumentation, rather than linking a separate package.
  luaSource = stdenvNoCC.mkDerivation {
    pname = "openstarbound-lua-source";
    version = luaMetadata.version;
    src = fetchurl {
      inherit (luaMetadata) url sha256;
    };
    patches = [ luaPatch ];
    strictDeps = true;
    dontConfigure = true;
    dontBuild = true;
    dontFixup = true;
    installPhase = ''
      runHook preInstall
      mkdir -p "$out"
      cp -R . "$out/"
      runHook postInstall
    '';
  };
  # SDL 3.4.14's ALSA backend omits two matching frees on audio shutdown.
  # Keep the normal ALSA driver; patch its ownership rather than hiding leaks
  # by selecting the dummy audio driver.
  patchedSdl3 = sdl3.overrideAttrs (old: {
    patches = (old.patches or [ ]) ++ [ ./patches/sdl3-alsa-cleanup.patch ];
  });
  imgui = dependencies.instrument (
    callPackage ./imgui.nix {
      inherit stdenv guiSupport;
      sdl3 = patchedSdl3;
    }
  );

  # Optional prebuilt Steam/Discord SDKs, vendored under lib/linux in the
  # repository.  They are unfree redistribution-wise and x86_64 only, hence the
  # asserts below; they need patchelf'ing because they are prebuilt binaries.
  sdks = stdenvNoCC.mkDerivation {
    pname = "openstarbound-sdks";
    inherit version;
    dontUnpack = true;
    dontConfigure = true;
    dontBuild = true;
    strictDeps = true;
    nativeBuildInputs = [ autoPatchelfHook ];
    # Only Discord's vendor ELF needs the system libgcc_s runtime. The SDKs
    # expose C APIs; this is not a compiler or a C++ build dependency.
    buildInputs = lib.optional discordSupport sdkRuntime;
    installPhase = ''
      runHook preInstall
      mkdir -p "$out/lib"
    ''
    + lib.optionalString steamSupport ''
      install -m755 ${src}/lib/linux/libsteam_api${sdkExtension} "$out/lib/"
      mkdir -p "$out/include"
      cp -r ${src}/lib/linux/include/steam "$out/include/"
    ''
    + lib.optionalString discordSupport ''
      install -m755 ${src}/lib/linux/libdiscord_game_sdk${sdkExtension} "$out/lib/"
    ''
    + ''
      runHook postInstall
    '';
    meta = {
      description = "Steam and Discord game SDKs vendored in the OpenStarbound repository";
      license = lib.licenses.unfree;
      platforms = [ "x86_64-linux" ];
    };
  };

  # Everything upstream drops into dist/.  starbound/starbound_server need
  # assets, so they are launched through nix/launcher.sh instead of being
  # exposed directly.
  wrapped = [ "starbound_server" ] ++ lib.optional guiSupport "starbound";
  plain = [
    "asset_packer"
    "asset_unpacker"
    "btree_repacker"
    "dump_versioned_json"
    "make_versioned_json"
  ]
  ++ lib.optionals withTests [
    "core_tests"
    "game_tests"
  ]
  ++ lib.optional (withTests && guiSupport) "frontend_tests"
  ++ lib.optionals withBenchmarks [
    "core_benchmarks"
    "json_benchmarks"
    "file_benchmarks"
    "array_benchmarks"
    "hash_benchmarks"
    "list_benchmarks"
    "vector_benchmarks"
    "set_benchmarks"
    "string_benchmarks"
    "format_benchmarks"
    "error_benchmarks"
    "unicode_benchmarks"
    "byte_array_benchmarks"
    "bytes_benchmarks"
    "line_benchmarks"
    "game_benchmarks"
    "ownership_benchmarks"
    "performance_benchmarks"
    "worker_benchmarks"
    "allocator_benchmarks"
  ]
  ++ lib.optionals (withBenchmarks && guiSupport) [
    "frontend_benchmarks"
    "tile_performance_benchmarks"
  ];
  qtTools =
    lib.optional qtSupport "json_tool" ++ lib.optional (qtSupport && steamSupport) "mod_uploader";
  distBinaries = plain ++ wrapped ++ qtTools;
  # C ABI dependencies may remain shared. Give copied build-tree executables
  # a complete store RPATH, including indirect C-library dependencies, rather
  # than relying on a development shell's LD_LIBRARY_PATH.
  # Static C++ archives forward C-library dependencies through their dev
  # outputs. Select the real runtime output explicitly before making RPATHs.
  runtimeLibraries = map (package: package.lib or package.out) (
    lib.closePropagation (
      [
        stdenv.cc.libc
        zlib-ng
        libcpr
        libpng
        freetype
        libogg
        libvorbis
        zstd
        re2
        cpptrace
      ]
      ++ lib.optionals guiSupport [
        patchedSdl3
        glew
        libGL
        libopus
      ]
      ++ lib.optional (steamSupport || discordSupport) sdks
      ++ lib.optional discordSupport sdkRuntime
      ++ lib.optional qtSupport qt5.qtbase
    )
  );
in
assert lib.assertMsg (
  stdenv.hostPlatform.isLinux && stdenv.hostPlatform.is64bit
) "This flake only supports x86_64/aarch64 Linux.";
assert lib.assertMsg (
  stdenv.buildPlatform == stdenv.hostPlatform
) "OpenStarbound's build system does not support cross compilation here.";
assert lib.assertMsg (
  !(steamSupport || discordSupport) || stdenv.hostPlatform.isx86_64
) "The vendored Linux Steam/Discord SDK binaries are x86_64 only.";

stdenv.mkDerivation {
  pname = if sanitizers then "openstarbound-sanitized" else "openstarbound";
  inherit version src;

  # Deviation from upstream, taken from the verified reference build: the Lua
  # binding passed Lua-supplied strings to ImGui as printf format strings.
  patches = [ ./patches/imgui-lua-literal-text.patch ];

  strictDeps = true;
  # The compiler wrapper supplies its bundled lld; no separate LLVM release.
  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
  ]
  ++ lib.optional qtSupport qt5.qttools;
  buildInputs = [
    mimalloc
    zlib-ng
    libcpr
    libpng
    freetype
    libogg
    libvorbis
    zstd
    imgui
    re2
    cpptrace
  ]
  ++ lib.optionals guiSupport [
    patchedSdl3
    glew
    libGL
    libopus
    opusCmake
  ]
  ++ lib.optional (steamSupport || discordSupport) sdks
  ++ lib.optional qtSupport qt5.qtbase
  ++ lib.optional withBenchmarks dependencies.gbenchmark;

  # Append rather than replace the caller's compiler/linker flags. These are
  # compiler-driver options: NIX_LDFLAGS would pass them directly to ld.
  env.XCLANG_ROOT = stdenv.cc.bundledRuntime;
  preConfigure = lib.optionalString sanitizers ''
    sanitizerFlags="-fsanitize=address,undefined"
    export CFLAGS="''${CFLAGS-} $sanitizerFlags"
    export CXXFLAGS="''${CXXFLAGS-} $sanitizerFlags"
    export LDFLAGS="''${LDFLAGS-} $sanitizerFlags"
  '';

  cmakeDir = "../source";
  # RelWithAsserts keeps optimization but omits NDEBUG, so starAssert and
  # DebugEnabled remain live.
  cmakeBuildType = if asserts then "RelWithAsserts" else "RelWithDebInfo";
  # Installation is handled below, not by CMake's GNUInstallDirs/export rules.
  # Keep configure hooks and package isolation without passing unused install
  # directory or obsolete export-registry options from the generic CMake hook.
  configurePhase = ''
    runHook preConfigure
    mkdir -p build
    cd build
    flagsArray=()
    concatTo flagsArray cmakeFlags cmakeFlagsArray
    cmake "$cmakeDir" -GNinja \
      -DCMAKE_BUILD_TYPE="$cmakeBuildType" \
      -DCMAKE_INSTALL_PREFIX="$out" \
      -DCMAKE_INSTALL_NAME_DIR="$out/lib" \
      -DCMAKE_AR="$(command -v "$AR")" \
      -DCMAKE_RANLIB="$(command -v "$RANLIB")" \
      -DCMAKE_STRIP="$(command -v "$STRIP")" \
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
      "''${flagsArray[@]}"
    export CMAKE_BUILD_PARALLEL_LEVEL="$NIX_BUILD_CORES"
    runHook postConfigure
  '';
  cmakeFlags = [
    (lib.cmakeFeature "STAR_SOURCE_IDENTIFIER" sourceIdentifier)
    (lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_STAR_LUA" "${luaSource}")
    (lib.cmakeBool "STAR_BUILD_GUI" guiSupport)
    # Include maintenance utilities (map_grep and tileset tools) in package builds.
    (lib.cmakeBool "STAR_BUILD_DEV_TOOLS" true)
    (lib.cmakeBool "BUILD_TESTING" withTests)
    (lib.cmakeFeature "STAR_STD_MODULE_MANIFEST" "${stdenv.cc.bundledRuntime}/${stdenv.hostPlatform.config}/usr/lib/libc++.modules.json")
    (lib.cmakeFeature "CMAKE_C_COMPILER" "${stdenv.cc}/bin/clang")
    (lib.cmakeFeature "CMAKE_CXX_COMPILER" "${stdenv.cc}/bin/clang++")
    (lib.cmakeBool "STAR_ENABLE_STEAM_INTEGRATION" steamSupport)
    (lib.cmakeBool "STAR_ENABLE_DISCORD_INTEGRATION" discordSupport)
    (lib.cmakeBool "STAR_BUILD_QT_TOOLS" qtSupport)
    # These binaries are copied from the build tree instead of CMake-installed.
    (lib.cmakeBool "CMAKE_SKIP_BUILD_RPATH" false)
    (lib.cmakeBool "CMAKE_BUILD_WITH_INSTALL_RPATH" true)
    (lib.cmakeBool "CMAKE_INSTALL_RPATH_USE_LINK_PATH" true)
    (lib.cmakeFeature "CMAKE_INSTALL_RPATH" (lib.makeLibraryPath runtimeLibraries))
    # Line tables retain file:line diagnostics without enormous test binaries.
    (lib.cmakeFeature "STAR_DEBUG_SYMBOL_LEVEL" (
      if sanitizers || asserts then "minimal" else "default"
    ))
  ]
  ++ lib.optional withBenchmarks (lib.cmakeBool "STAR_BUILD_BENCHMARKS" true)
  ++ lib.optionals steamSupport [
    (lib.cmakeFeature "STEAM_API_INCLUDE_DIR" "${sdks}/include")
    (lib.cmakeFeature "STEAM_API_LIBRARY" "${sdks}/lib/libsteam_api${sdkExtension}")
  ]
  ++ lib.optionals discordSupport [
    (lib.cmakeFeature "DISCORD_API_LIBRARY" "${sdks}/lib/libdiscord_game_sdk${sdkExtension}")
  ];

  # core_tests carries the "NoAssets" label and runs without any game assets.
  # game_tests needs a user's proprietary assets and writable storage, so it is
  # built and installed but never run here: the package ships
  # bin/run-game-tests (nix run .#game-tests) for that.
  #
  # ASan annotates mimalloc storage; debug checks validate pointers and padding.
  # LSan sees system allocations, not individual mimalloc private-heap blocks.
  doCheck = withTests;
  checkPhase = ''
    runHook preCheck
    ${lib.optionalString sanitizers "ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 "}ctest --output-on-failure -L NoAssets
    runHook postCheck
  '';

  dontWrapQtApps = true;
  installPhase = ''
    runHook preInstall
    mkdir -p "$out/bin" "$out/libexec" "$out/share/openstarbound/assets"
    install -Dm644 ../doc/OPENSOURCE.md "$out/share/doc/openstarbound/OPENSOURCE.md"

    for binary in ${lib.escapeShellArgs distBinaries}; do
      install -m755 "../dist/$binary" "$out/libexec/$binary"
    done
    for binary in ${lib.escapeShellArgs plain}; do
      ln -s "../libexec/$binary" "$out/bin/$binary"
    done
    ${lib.concatMapStrings (binary: ''
      install -m755 ${./launcher.sh} "$out/bin/${binary}"
      substituteInPlace "$out/bin/${binary}" \
        --replace-fail '@binary@' "$out/libexec/${binary}" \
        --replace-fail '@bundledAssets@' "$out/share/openstarbound/assets"
    '') wrapped}
    ${lib.optionalString qtSupport ''
      for binary in ${lib.escapeShellArgs qtTools}; do
        install -m755 "../dist/$binary" "$out/bin/$binary"
      done
    ''}
    ${lib.optionalString withTests ''
      # Runs game_tests against assets from the local filesystem; they cannot be
      # a build input because they are gitignored and not redistributable.
      install -m755 ${./run-game-tests.py} "$out/bin/run-game-tests"
      substituteInPlace "$out/bin/run-game-tests" \
        --replace-fail '@python@' '${python3}/bin/python3' \
        --replace-fail '@gameTests@' "$out/libexec/game_tests" \
        --replace-fail '@bundledAssets@' "$out/share/openstarbound/assets"
    ''}
    cp -r ../assets/opensb "$out/share/openstarbound/assets/opensb"
    cp -r ../assets/opensb-compat "$out/share/openstarbound/assets/opensb-compat"
    install -m644 ../scripts/packing.config "$out/share/openstarbound/packing.config"
    install -m644 ../scripts/ci/linux/sbinit.config \
      "$out/share/openstarbound/sbinit.config.example"
    runHook postInstall
  '';

  # Qt libraries and plugins are static, so no dynamic Qt plugin wrapper is
  # needed (nor may the ordinary shared Qt/Wayland scope enter the closure).

  passthru = {
    inherit
      imgui
      luaSource
      dependencies
      runtimeLibraries
      ;
    inherit
      guiSupport
      steamSupport
      discordSupport
      qtSupport
      withTests
      distBinaries
      ;
  };

  meta = {
    description = "Starbound client and server, the OpenStarbound fork";
    homepage = "https://github.com/OpenStarbound/OpenStarbound";
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
    mainProgram = if guiSupport then "starbound" else "starbound_server";
    # Upstream carries no top-level license grant; do not invent one.
  };
}
