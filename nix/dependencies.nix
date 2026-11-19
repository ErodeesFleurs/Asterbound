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

  # Runtime shader compilation crosses shaderc's C API, but its implementation
  # and every transitive C++ archive still use our private libc++/ASan ABI.
  spirv-tools = instrument (
    (pkgs.spirv-tools.override { inherit stdenv; }).overrideAttrs (old: {
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        (lib.cmakeBool "BUILD_SHARED_LIBS" false)
        (lib.cmakeBool "SPIRV_TOOLS_BUILD_STATIC" true)
        (lib.cmakeBool "SPIRV_SKIP_TESTS" true)
        (lib.cmakeBool "SPIRV_SKIP_EXECUTABLES" true)
        "-UBUILD_TESTING"
      ];
    })
  );
  glslang = instrument (
    (pkgs.glslang.override { inherit stdenv spirv-tools gtest; }).overrideAttrs (old: {
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        (lib.cmakeBool "BUILD_SHARED_LIBS" false)
        "-UBUILD_TESTING"
      ];
    })
  );
  shaderc = instrument (
    (pkgs.shaderc.override { inherit stdenv glslang spirv-tools; }).overrideAttrs (old: {
      propagatedBuildInputs = (old.propagatedBuildInputs or [ ]) ++ [ spirv-tools ];
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        (lib.cmakeBool "BUILD_SHARED_LIBS" false)
        "-UBUILD_TESTING"
      ];
      # The unvendored Nix build must use the installed namespaced glslang
      # targets and explicitly select SPIRV-Tools' static target.
      postPatch = (old.postPatch or "") + ''
        substituteInPlace libshaderc/CMakeLists.txt \
          --replace-fail '  glslang ''${CMAKE_THREAD_LIBS_INIT}' \
            '  glslang::glslang ''${CMAKE_THREAD_LIBS_INIT}' \
          --replace-fail '  SPIRV # from glslang' '  glslang::SPIRV # from glslang' \
          --replace-fail $'  SPIRV-Tools\n' $'  SPIRV-Tools-static\n'
        substituteInPlace libshaderc_util/CMakeLists.txt \
          --replace-fail '  glslang SPIRV' '  glslang::glslang glslang::SPIRV'
        # Imported static targets have no TARGET_OBJECTS; keep those archives
        # external and let the application link the exported dependency graph.
        substituteInPlace cmake/utils.cmake \
          --replace-fail 'if ("''${libtype}" STREQUAL "STATIC_LIBRARY")' \
            'get_target_property(is_imported ''${target} IMPORTED)
    if ("''${libtype}" STREQUAL "STATIC_LIBRARY" AND NOT is_imported)'
      '';
    })
  );

  # Qt's public APIs and plugin interfaces are C++, so Qt libraries AND its
  # platform/image/style plugins must be linked statically into each tool.
  # qtbase supplies moc/uic/rcc; neither tool uses Linguist or Qt translations.
  doubleConversion = instrument (
    (pkgs.double-conversion.override { inherit stdenv; }).overrideAttrs (old: {
      cmakeFlags = (old.cmakeFlags or [ ]) ++ [
        (lib.cmakeBool "BUILD_SHARED_LIBS" false)
      ];
    })
  );
  qt6 = (pkgs.qt6.override { inherit stdenv; }).overrideScope (
    _final: prev: {
      qtbase = instrument (
        (prev.qtbase.override {
          inherit stdenv icu;
          double-conversion = doubleConversion;
          qttranslations = null;
          withGtk3 = false;
          withWayland = false;
          systemdSupport = false;
          cups = null;
          libpq = null;
        }).overrideAttrs (old: {
          dontDisableStatic = true;
          # Static Qt still installs dynamically linked C-ABI host tools.
          # Qt clears their install RPATH; resolve it in the package, not via
          # LD_LIBRARY_PATH in each downstream moc invocation.
          nativeBuildInputs = (old.nativeBuildInputs or [ ]) ++ [ pkgs.autoPatchelfHook ];
          buildInputs = lib.subtractLists [ pkgs.libmysqlclient ] (old.buildInputs or [ ]);
          # Only the Widgets/XCB dependency closure is needed. In particular,
          # do not propagate unused SQL drivers, QtNetwork's proxy or Vulkan.
          propagatedBuildInputs = with pkgs; [
            zlib libGL harfbuzz libjpeg libpng pcre2 zstd libb2 md4c
            dbus glib fontconfig freetype
            libx11 libxext libxi libxrender libxcb libxkbcommon
            libxcb-util libxcb-image libxcb-keysyms libxcb-render-util
            libxcb-wm libxdmcp libxtst libxcb-cursor
          ] ++ [ icu doubleConversion ];
          cmakeFlags = lib.filter (
            flag: !(lib.any (prefix: lib.hasPrefix prefix flag) [
              "-DQT_FEATURE_libproxy=" "-DQT_FEATURE_system_sqlite="
              "-DQT_FEATURE_openssl_linked=" "-DQT_FEATURE_vulkan="
              "-DQT_FEATURE_sctp=" "-DQT_FEATURE_journald="
            ])
          ) (old.cmakeFlags or [ ]) ++ [
            (lib.cmakeBool "BUILD_SHARED_LIBS" false)
            (lib.cmakeBool "QT_BUILD_EXAMPLES" false)
            (lib.cmakeBool "QT_BUILD_TESTS" false)
            "-DQT_FEATURE_network=OFF"
            "-DQT_FEATURE_sql=OFF"
            "-DQT_FEATURE_printsupport=OFF"
            "-DQT_FEATURE_testlib=OFF"
            "-DQT_FEATURE_xml=OFF"
            "-DQT_FEATURE_vulkan=OFF"
          ];
          # Preserve split-output metadata fixups, not upstream's patchelf
          # operations on shared QtGui and the unused MySQL plugin.
          postFixup = lib.concatStringsSep "\n" (
            lib.filter (line:
              !(lib.hasInfix "libqsqlmysql.so" line || lib.hasInfix "$out/lib/libQt6Gui.so" line)
            ) (lib.splitString "\n" old.postFixup)
          );
        })
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
    spirv-tools
    glslang
    shaderc
    qt6
    sdkRuntime
    ;
}
