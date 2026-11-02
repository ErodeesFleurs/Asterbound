{
  lib,
  stdenv,
  fetchurl,
  cmake,
  ninja,
  python3,
  bundledRuntime,
}:
let
  triple = stdenv.hostPlatform.config;
in
stdenv.mkDerivation {
  pname = "xclang-asan-libcxx";
  version = "23.1.3.6";

  src = fetchurl {
    url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.3/llvm-project-23.1.3.src.tar.xz";
    sha256 = "c44186a7762ed28954be72e5ff6df9808e0779d4f1bf014ecc4e7e211d31ee34";
  };

  nativeBuildInputs = [ cmake ninja python3 ];
  enableParallelBuilding = true;
  # Keep the archive's ASan instrumentation and metadata intact.
  dontStrip = true;

  cmakeDir = "../runtimes";
  cmakeBuildType = "Release";
  preConfigure = ''
    # This changes ASan's ELF grouping, not its global registration, redzones,
    # ODR checks or libc++ container annotations. Do not instrument with UBSan:
    # the original xclang runtime uses LLVM_USE_SANITIZER=Address only.
    export CXXFLAGS="''${CXXFLAGS-} -fno-sanitize-address-globals-dead-stripping"
  '';
  # Use the caller's non-sanitized wrapped xclang and Nix glibc, never the
  # release archive's glibc 2.17 sysroot or a self-dependent ASan compiler.
  # As with the project's custom install, omit generic CMake-hook install-dir
  # and deprecated registry options that the runtimes build does not consume.
  configurePhase = ''
    runHook preConfigure
    mkdir -p build
    cd build
    flagsArray=()
    concatTo flagsArray cmakeFlags cmakeFlagsArray
    cmake "$cmakeDir" -GNinja \
      -DCMAKE_BUILD_TYPE="$cmakeBuildType" \
      -DCMAKE_INSTALL_PREFIX="$TMPDIR/runtime-install" \
      '-DCMAKE_EXE_LINKER_FLAGS=--rtlib=compiler-rt --unwindlib=none -nostdlib++' \
      -DCMAKE_C_COMPILER="$(command -v "$CC")" \
      -DCMAKE_CXX_COMPILER="$(command -v "$CXX")" \
      -DCMAKE_AR="$(command -v "$AR")" \
      -DCMAKE_RANLIB="$(command -v "$RANLIB")" \
      -DCMAKE_STRIP="$(command -v "$STRIP")" \
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
      "''${flagsArray[@]}"
    export CMAKE_BUILD_PARALLEL_LEVEL="$NIX_BUILD_CORES"
    runHook postConfigure
  '';

  # Match xclang's pinned cmake/caches/cxx.cmake and Linux cxxAsan recipe.
  # libunwind remains in the build graph because libc++abi requires its target;
  # its output is not installed here or substituted for the bundled unwinder.
  cmakeFlags = [
    (lib.cmakeBool "LLVM_INCLUDE_TESTS" false)
    (lib.cmakeBool "LLVM_ENABLE_PER_TARGET_RUNTIME_DIR" false)
    (lib.cmakeBool "CMAKE_POSITION_INDEPENDENT_CODE" true)
    (lib.cmakeBool "CMAKE_DISABLE_PRECOMPILE_HEADERS" true)
    (lib.cmakeFeature "LLVM_ENABLE_RUNTIMES" "libunwind;libcxxabi;libcxx")
    (lib.cmakeFeature "LLVM_DEFAULT_TARGET_TRIPLE" triple)
    (lib.cmakeFeature "LLVM_USE_SANITIZER" "Address")

    (lib.cmakeBool "LIBUNWIND_ENABLE_SHARED" false)
    (lib.cmakeBool "LIBUNWIND_ENABLE_STATIC" true)
    (lib.cmakeBool "LIBUNWIND_USE_COMPILER_RT" true)
    (lib.cmakeBool "LIBUNWIND_HIDE_SYMBOLS" true)
    (lib.cmakeBool "LIBUNWIND_INCLUDE_TESTS" false)

    (lib.cmakeBool "LIBCXXABI_ENABLE_SHARED" false)
    (lib.cmakeBool "LIBCXXABI_ENABLE_STATIC" true)
    (lib.cmakeBool "LIBCXXABI_USE_COMPILER_RT" true)
    (lib.cmakeBool "LIBCXXABI_HERMETIC_STATIC_LIBRARY" true)
    (lib.cmakeBool "LIBCXXABI_INCLUDE_TESTS" false)
    (lib.cmakeBool "LIBCXXABI_USE_LLVM_UNWINDER" true)
    (lib.cmakeBool "LIBCXXABI_HAS_CXA_THREAD_ATEXIT_IMPL" false)
    (lib.cmakeBool "LIBCXXABI_ENABLE_EXCEPTIONS" true)

    (lib.cmakeBool "LIBCXX_ENABLE_SHARED" false)
    (lib.cmakeBool "LIBCXX_ENABLE_STATIC" true)
    (lib.cmakeFeature "LIBCXX_CXX_ABI" "libcxxabi")
    (lib.cmakeBool "LIBCXX_ENABLE_STATIC_ABI_LIBRARY" true)
    (lib.cmakeBool "LIBCXX_HERMETIC_STATIC_LIBRARY" true)
    (lib.cmakeBool "LIBCXX_USE_COMPILER_RT" true)
    (lib.cmakeFeature "LIBCXX_HARDENING_MODE" "none")
    (lib.cmakeBool "LIBCXX_INSTALL_MODULES" true)
    (lib.cmakeBool "LIBCXX_ENABLE_ABI_LINKER_SCRIPT" false)
    (lib.cmakeBool "LIBCXX_INCLUDE_TESTS" false)
    (lib.cmakeBool "LIBCXX_INCLUDE_BENCHMARKS" false)
    (lib.cmakeBool "LIBCXX_ENABLE_RTTI" true)
    (lib.cmakeBool "LIBCXX_ENABLE_EXCEPTIONS" true)
  ];

  installPhase = ''
    runHook preInstall
    cmake --install .
    configSite="$TMPDIR/runtime-install/include/c++/v1/__config_site"
    # Fail rather than silently selecting different libc++ feature defaults.
    cmp "$configSite" '${bundledRuntime}/${triple}/usr/lib/asan/include/__config_site'
    grep -Fx '#define _LIBCPP_INSTRUMENTED_WITH_ASAN 1' "$configSite"
    mkdir -p "$out/lib" "$out/include"
    cp "$TMPDIR/runtime-install/lib/libc++.a" "$out/lib/"
    cp "$TMPDIR/runtime-install/lib/libc++experimental.a" "$out/lib/"
    cp "$configSite" "$out/include/__config_site"
    runHook postInstall
  '';

  meta = {
    description = "Pinned xclang ASan libc++ with independent global registration";
    license = lib.licenses.asl20;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
  };
}
