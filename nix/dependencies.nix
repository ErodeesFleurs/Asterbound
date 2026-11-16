{
  pkgs,
  sanitizers ? false,
}:

let
  inherit (pkgs) lib;
  cc = pkgs.callPackage ./compiler.nix { inherit sanitizers; };
  stdenv = pkgs.overrideCC pkgs.stdenv cc;

  # ASan's instrumented libc++ is selected by the compiler wrapper. All C++
  # code in the executable must be instrumented with the same container ABI.
  instrument =
    package:
    package.overrideAttrs (old: {
      env = (old.env or { }) // {
        XCLANG_ROOT = cc.bundledRuntime;
      };
      # A fresh Nix build has no stale cache. Omit variables marked for removal
      # before CMake sees them, including the hook's obsolete registry option.
      cmakeFlags = lib.filter (flag: !(lib.hasPrefix "-U" flag)) (old.cmakeFlags or [ ]) ++ [
        (lib.cmakeBool "CMAKE_SKIP_BUILD_RPATH" false)
        (lib.cmakeFeature "CMAKE_BUILD_RPATH" (
          lib.makeLibraryPath ((old.buildInputs or [ ]) ++ (old.propagatedBuildInputs or [ ]))
        ))
      ];
      preConfigure =
        (import ./filter-cmake-variables.nix) (
          [ "CMAKE_EXPORT_NO_PACKAGE_REGISTRY" ]
          ++ map (lib.removePrefix "-U")
            (lib.filter (lib.hasPrefix "-U") (old.cmakeFlags or [ ]))
        )
        + lib.optionalString sanitizers ''
          export CFLAGS="''${CFLAGS-} -fsanitize=address,undefined"
          export CXXFLAGS="''${CXXFLAGS-} -fsanitize=address,undefined"
          export LDFLAGS="''${LDFLAGS-} -fsanitize=address,undefined"
        ''
        + (old.preConfigure or "");
    });

  # nixpkgs pins mimalloc 3.4.5 and exports the mimalloc-static CMake target.
  # Keep it in the same xclang/runtime closure, without C malloc interposition
  # or a second set of global C++ operators (StarMemory owns those).
  mimalloc = instrument (
    (pkgs.mimalloc.override { inherit stdenv; }).overrideAttrs (old: {
      dontDisableStatic = true;
      # Keep optimization and all ASan pointer/padding checks, but do not mix
      # MI_DEBUG with Release's contradictory NDEBUG definition.
      cmakeBuildType = if sanitizers then "RelWithAsserts" else "Release";
      cmakeFlags =
        (old.cmakeFlags or [ ])
        ++ [
          # This is a C-only build with no package lookup.
          "-UCMAKE_CXX_COMPILER"
          "-UCMAKE_FIND_USE_PACKAGE_REGISTRY"
          "-UCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY"
          (lib.cmakeBool "MI_BUILD_STATIC" true)
          (lib.cmakeBool "MI_BUILD_SHARED" false)
          (lib.cmakeBool "MI_BUILD_OBJECT" false)
          (lib.cmakeBool "MI_OVERRIDE" false)
          (lib.cmakeBool "MI_USE_CXX" false)
          (lib.cmakeBool "MI_TRACK_ASAN" sanitizers)
        ]
        # Tracking enables padding; validate pointers before reading that metadata.
        # Without MI_DEBUG, upstream's invalid-realloc API test corrupts free lists.
        ++ lib.optionals sanitizers [
          (lib.cmakeBool "MI_DEBUG" true)
          (lib.cmakeFeature "CMAKE_C_FLAGS_RELWITHASSERTS" "-O3")
        ];
      # Upstream otherwise silently drops tracking if ASan headers are absent.
      # Poison/unpoison annotations cover access errors, not LSan registration
      # of individual allocations in mimalloc's private heaps.
      postPatch =
        (old.postPatch or "")
        + lib.optionalString sanitizers ''
          substituteInPlace CMakeLists.txt \
            --replace-fail \
              "message(WARNING \"Cannot find the 'sanitizer/asan_interface.h' -- install address sanitizer support first\")" \
              'message(FATAL_ERROR "Asterbound requires mimalloc ASan tracking; sanitizer/asan_interface.h is missing")'
        '';
    })
  );

  # Test helpers have C++ interfaces too; using nixpkgs' shared gtest here
  # would introduce a second, incompatible C++ runtime into dependency tests.
  gtest = instrument (
    (pkgs.gtest.override {
      inherit stdenv;
      static = true;
    }).overrideAttrs (old: {
      # GoogleTest uses gtest_build_tests, not CTest's BUILD_TESTING.
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [ "-UBUILD_TESTING" ];
    })
  );
  gbenchmark = instrument (
    (pkgs.gbenchmark.override { inherit stdenv gtest; }).overrideAttrs (old: {
      # The structured environment is sourced before LOCALE_ARCHIVE is set.
      # Enter the build in C locale and select the test locale after setup.
      env = (old.env or { }) // {
        LANG = "C";
        LC_ALL = "C";
      };
      preCheck = (old.preCheck or "") + lib.optionalString stdenv.hostPlatform.isGnu ''
        export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
      '';
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        "-UCMAKE_C_COMPILER"
        (lib.cmakeBool "BUILD_SHARED_LIBS" false)
        # Remove compiler-instruction snapshots; retain all behavioral tests.
        (lib.cmakeBool "BENCHMARK_ENABLE_ASSEMBLY_TESTS" false)
      ];
    })
  );
  abseil-cpp = instrument (
    (pkgs.abseil-cpp_202601.override {
      inherit stdenv gtest;
      static = true;
      cxxStandard = "20";
    }).overrideAttrs (old: {
      patches = (old.patches or [ ]) ++ [ ./patches/abseil-lifetime-capture-this.patch ];
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [ "-UCMAKE_C_COMPILER" ];
    })
  );

  # ICU exposes C++ APIs used by RE2. Keep both its libraries and its data
  # archive static, without rebuilding unrelated C-ABI libraries in nixpkgs.
  # Do not use withStatic: that splits archives into a separate output while
  # retaining shared libraries, which find_package(ICU) would prefer.
  icu = instrument (
    (pkgs.icu.override { inherit stdenv; }).overrideAttrs (old: {
      dontDisableStatic = true;
      configureFlags = (old.configureFlags or [ ]) ++ [
        "--enable-static"
        "--disable-shared"
      ];
      postInstall = lib.replaceStrings [ "--replace '" ] [ "--replace-fail '" ] old.postInstall;
    })
  );
  re2 = instrument (
    (pkgs.re2.override {
      inherit
        stdenv
        abseil-cpp
        icu
        gtest
        gbenchmark
        ;
    }).overrideAttrs
      (old: {
        cmakeFlags = (old.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "BUILD_SHARED_LIBS" false)
          "-UCMAKE_C_COMPILER"
        ];
        # FindICU's uc target does not carry the static data archive. Export the
        # data component as well, so downstream CMake consumers link it too.
        postPatch = (old.postPatch or "") + ''
          substituteInPlace CMakeLists.txt re2Config.cmake.in \
            --replace-fail "ICU REQUIRED COMPONENTS uc)" "ICU REQUIRED COMPONENTS uc data)"
          substituteInPlace CMakeLists.txt \
            --replace-fail "if(NOT TARGET ICU::uc)" "if(NOT TARGET ICU::uc OR NOT TARGET ICU::data)" \
            --replace-fail "target_link_libraries(re2 PUBLIC ICU::uc)" \
              "target_link_libraries(re2 PUBLIC ICU::uc ICU::data)"
          # Abseil's lowercase locking APIs and reference-taking guards have
          # identical synchronization semantics to the retired pointer APIs.
          substituteInPlace re2/dfa.cc \
            --replace-fail 'absl::MutexLock l(&mutex_);' 'absl::MutexLock l(mutex_);' \
            --replace-fail 'absl::MutexLock l(&dfa_->mutex_);' 'absl::MutexLock l(dfa_->mutex_);' \
            --replace-fail 'absl::MutexLock lock(&mutex_);' 'absl::MutexLock lock(mutex_);' \
            --replace-fail 'mu_->ReaderLock()' 'mu_->lock_shared()' \
            --replace-fail 'mu_->ReaderUnlock()' 'mu_->unlock_shared()' \
            --replace-fail 'mu_->WriterLock()' 'mu_->lock()' \
            --replace-fail 'mu_->WriterUnlock()' 'mu_->unlock()'
          substituteInPlace re2/regexp.cc \
            --replace-fail 'absl::MutexLock l(ref_mutex());' 'absl::MutexLock l(*ref_mutex());'
          substituteInPlace re2/testing/regexp_benchmark.cc \
            --replace-fail 'absl::MutexLock lock(&mutex);' 'absl::MutexLock lock(mutex);'
        '';
      })
  );
  libcpr = instrument (
    (pkgs.libcpr.override {
      inherit stdenv;
      staticOnly = true;
    }).overrideAttrs (old: {
      # CPR has its own CPR_BUILD_TESTS option and only enables C++.
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        "-UBUILD_TESTING"
        "-UCMAKE_C_COMPILER"
      ];
    })
  );
  cpptrace = instrument (
    (pkgs.cpptrace.override {
      inherit stdenv gtest;
      static = true;
    }).overrideAttrs
      (old: {
        # Use the compiler's private unwinder, not a second shared libunwind.
        buildInputs = [ (lib.getDev pkgs.libdwarf) ];
        propagatedBuildInputs = [
          pkgs.zstd
          pkgs.libdwarf
        ];
        # Keep the upstream semantic test suite, but do not build unconsumed
        # demos (notably signal_demo's deliberately unchecked signal I/O).
        postPatch = (old.postPatch or "") + ''
          substituteInPlace test/CMakeLists.txt \
            --replace-fail 'add_executable(demo demo.cpp)' 'add_executable(demo EXCLUDE_FROM_ALL demo.cpp)' \
            --replace-fail 'add_executable(c_demo ctrace_demo.c)' 'add_executable(c_demo EXCLUDE_FROM_ALL ctrace_demo.c)' \
            --replace-fail 'add_executable(signal_demo signal_demo.cpp)' 'add_executable(signal_demo EXCLUDE_FROM_ALL signal_demo.cpp)' \
            --replace-fail 'add_executable(signal_tracer signal_tracer.cpp)' 'add_executable(signal_tracer EXCLUDE_FROM_ALL signal_tracer.cpp)'
        '';
        cmakeFlags = (old.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "CPPTRACE_UNWIND_WITH_LIBUNWIND" false)
          (lib.cmakeBool "CPPTRACE_UNWIND_WITH_UNWIND" true)
          # Upstream excludes instruction-byte bounds under instrumentation;
          # the semantic tracing tests still run with ASan/UBSan enabled.
          (lib.cmakeBool "CPPTRACE_SANITIZER_BUILD" sanitizers)
        ];
      })
  );

  # Qt's public APIs and plugin interfaces are C++, so Qt libraries AND its
  # platform/image/style plugins must be linked statically into each tool.
  # Qtbase contains moc/uic/rcc. Only Linguist's lupdate/lrelease family from
  # qttools is needed; QDoc would add nixpkgs' incompatible shared LLVM C++ API.
  qt5 = (pkgs.qt5.override { inherit stdenv; }).overrideScope (
    final: prev: {
      qtbase = instrument (
        (prev.qtbase.override {
          inherit icu;
          # Avoid the translations -> qttools -> qtbase bootstrap cycle; the
          # Asterbound widgets do not install or load Qt translation catalogs.
          qttranslations = null;
        }).overrideAttrs
          (old: {
            propagatedBuildInputs = (old.propagatedBuildInputs or [ ]) ++ [ pkgs.zstd ];
            patches = (old.patches or [ ]) ++ [ ../cmake/patches/qtbase-libcxx.patch ];
            # Qt's shared-library RPATH feature is invalid for a static Qt build.
            configureFlags =
              lib.filter (flag: flag != "-shared" && flag != "-rpath") (old.configureFlags or [ ])
              ++ [
                "-static"
                "-no-rpath"
                "-platform"
                "linux-clang"
              ];
            # Host tools still load C-ABI dependencies through the bundled lld.
            preConfigure = (old.preConfigure or "") + ''
              export NIX_LDFLAGS="''${NIX_LDFLAGS-} -rpath ${
                lib.makeLibraryPath (
                  (old.buildInputs or [ ]) ++ (old.propagatedBuildInputs or [ ]) ++ [ pkgs.zstd ]
                )
              }"
            '';
            # Static plugin metadata is already installed in dev/mkspecs/modules.
            # Merge its files before the upstream one-level mkspecs directory move.
            postInstall = ''
              moveToOutput "mkspecs/modules/*" "$dev"
            ''
            + (old.postInstall or "");
            # qtbase does not load its own setup hook. Supply its plugin prefix to
            # the inherited fixup so static consumers find the binary-output archives.
            postFixup = ''
              qtPluginPrefix=${lib.escapeShellArg prev.qtbase.qtPluginPrefix}
            ''
            + (old.postFixup or "");
          })
      );
      qttools = instrument (
        final.qtModule {
          pname = "qttools";
          propagatedBuildInputs = [ final.qtbase ];
          outputs = [
            "out"
            "dev"
          ];
          patches = [
            (pkgs.replaceVars
              "${pkgs.path}/pkgs/development/libraries/qt-5/modules/qttools-QT_HOST_DATA-refs.patch"
              {
                qtbaseDev = lib.getDev final.qtbase;
              }
            )
          ];
          postPatch = ''
            # No QDoc/LLVM, Designer plugins or QML module is used by these builds.
            printf '%s\n' \
              'TEMPLATE = subdirs' \
              'SUBDIRS = uiplugin uitools linguist' \
              'uiplugin.subdir = designer/src/uiplugin' \
              'uitools.subdir = designer/src/uitools' \
              'uitools.depends = uiplugin' \
              'linguist.depends = uitools' > src/src.pro
          '';
          devTools = [
            "bin/linguist"
            "bin/lconvert"
            "bin/lrelease"
            "bin/lrelease-pro"
            "bin/lupdate"
            "bin/lupdate-pro"
            "bin/lprodump"
          ];
          setupHook = "${pkgs.path}/pkgs/development/libraries/qt-5/hooks/qttools-setup-hook.sh";
        }
      );
    }
  );

  # Vendor SDKs only cross C interfaces. Discord's prebuilt ELF needs
  # libgcc_s.so.1 (not libstdc++.so.6); this runtime-only output is deliberately
  # separate from the selected compiler and never used to build C++ code.
  sdkRuntime = pkgs.gcc.cc.lib;
in
{
  inherit
    cc
    stdenv
    instrument
    mimalloc
    gtest
    gbenchmark
    abseil-cpp
    icu
    re2
    libcpr
    cpptrace
    qt5
    sdkRuntime
    ;
}
