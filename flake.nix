{
  description = "Asterbound — build the client, server and asset tools with Nix";

  # Pinned to the revision the C++ reference build was validated against.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/c8d3b8cdb3959a35bb0af0de111b7d13caf79896";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system:
          f (
            import nixpkgs {
              inherit system;
              # The Steam and Discord game SDKs vendored in lib/linux are unfree;
              # without this, `packages.full`, `devShells.full` and therefore
              # `nix flake check` cannot be evaluated at all.  Nothing else is
              # allowed to be unfree.
              config.allowUnfreePredicate = pkg: (pkg.pname or pkg.name or "") == "openstarbound-sdks";
            }
          )
        );

      # Ends up in the version string the binaries report and log.
      sourceIdentifier = self.shortRev or self.dirtyShortRev or "dirty";

      packagesFor =
        pkgs:
        let
          dependencies = import ./nix/dependencies.nix { inherit pkgs; };
          sanitizedBaseDependencies = import ./nix/dependencies.nix {
            inherit pkgs;
            sanitizers = true;
          };
          sanitizerRuntime = pkgs.callPackage ./nix/asan-runtime.nix {
            stdenv = dependencies.stdenv;
            bundledRuntime = dependencies.cc.bundledRuntime;
          };
          sanitizedCompiler = pkgs.callPackage ./nix/compiler.nix {
            sanitizers = true;
            inherit sanitizerRuntime;
          };
          # Only ASan's global-retention metadata changes. The runtime build
          # verifies its config-site against the bundled ASan container ABI, so
          # existing instrumented dependency archives remain compatible.
          sanitizedDependencies = sanitizedBaseDependencies // {
            cc = sanitizedCompiler;
            stdenv = pkgs.overrideCC sanitizedBaseDependencies.stdenv sanitizedCompiler;
          };
          asterbound = pkgs.callPackage ./nix/asterbound.nix {
            inherit dependencies;
            stdenv = dependencies.stdenv;
            # Build inputs include version.json, source/, assets/, scripts/, cmake/ and lib/.
            # Keep the flake's own files out of the source tree so that editing
            # them does not force a rebuild of the ten minute C++ build, and
            # keep build outputs out: a hand build leaves a `build/` tree with a
            # CMake cache that names the checkout it was configured from, and
            # copying that into the store shipped 14 GB of object files into
            # every build and let stale absolute paths into the sandbox.
            src = nixpkgs.lib.cleanSourceWith {
              src = ./.;
              name = "asterbound-source";
              filter =
                path: type:
                let
                  name = baseNameOf path;
                in
                !builtins.elem name [
                  "flake.nix"
                  "flake.lock"
                  "nix"
                ]
                && !(
                  type == "directory"
                  && builtins.elem name [
                    "build"
                    "build_linux"
                    "dist"
                    "dev"
                    "out"
                    "output"
                    "Output"
                    ".cache"
                  ]
                )
                && !(type == "symlink" && builtins.match "result.*" name != null);
            };
            inherit sourceIdentifier;
          };
        in
        {
          inherit asterbound;
          default = asterbound;
          # Headless server: no SDL3/OpenGL/GLEW, no client.
          server = asterbound.override { guiSupport = false; };
          # Steam SDK and Qt tools. Discord requires an explicit override with
          # the distributor's own discordApplicationId.
          full = asterbound.override {
            steamSupport = true;
            qtSupport = true;
          };
          # ASan + UBSan build: the checkPhase fails on the first sanitizer
          # report, and `nix run .#sanitized-tests` sweeps game_tests with the
          # sanitizers halting on the first error (and a 900s per-case timeout,
          # world generation is much slower under ASan).
          # Note: `nix build .#sanitized` overwrites ./result -- pass
          # `-o result-sanitized` to keep the default package linked there.
          sanitized = asterbound.override {
            sanitizers = true;
            dependencies = sanitizedDependencies;
            stdenv = sanitizedDependencies.stdenv;
          };
          # RelWithAsserts build: no -DNDEBUG, so the project's own invariants
          # (starAssert) and DebugEnabled paths are live while the tests run.
          asserts = asterbound.override { asserts = true; };
          benchmarks = asterbound.override {
            guiSupport = false;
            withBenchmarks = true;
          };
          benchmarks-gui = asterbound.override { withBenchmarks = true; };
        };
    in
    {
      packages = forAllSystems packagesFor;

      # `nix run .#game-tests -- --gtest_filter=...` runs the asset-dependent
      # game_tests against a local copy of Starbound's assets and writes a
      # report; those assets are gitignored, so they cannot be a build input.
      apps = forAllSystems (pkgs: {
        game-tests = {
          type = "app";
          program = "${(packagesFor pkgs).default}/bin/run-game-tests";
          meta.description = "Run Asterbound's game_tests against your Starbound assets";
        };
        # Same sweep with ASan/UBSan halting on the first error and a 900s per
        # case timeout. LSan sees intercepted system allocations, not individual
        # mimalloc private-heap blocks (which use MI_TRACK_ASAN annotations).
        sanitized-tests = {
          type = "app";
          program = "${pkgs.writeShellScript "asterbound-sanitized-tests" ''
            export ASAN_OPTIONS="detect_leaks=1''${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
            export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1''${UBSAN_OPTIONS:+:$UBSAN_OPTIONS}"
            export ASTERBOUND_TEST_TIMEOUT="''${ASTERBOUND_TEST_TIMEOUT:-900}"
            exec ${(packagesFor pkgs).sanitized}/bin/run-game-tests "$@"
          ''}";
          meta.description = "Run Asterbound's game_tests with ASan/UBSan and system-allocation leak checks";
        };
        # Same sweep against the assert-enabled build, so a violated invariant
        # fails the run instead of being compiled out.
        asserts-tests = {
          type = "app";
          program = "${pkgs.writeShellScript "asterbound-asserts-tests" ''
            export ASTERBOUND_TEST_TIMEOUT="''${ASTERBOUND_TEST_TIMEOUT:-900}"
            exec ${(packagesFor pkgs).asserts}/bin/run-game-tests "$@"
          ''}";
          meta.description = "Run Asterbound's game_tests with the project's own assertions enabled";
        };
      });

      # Both shells use the same compiler and static C++ dependency closure as
      # their packages. CMake's build RPATH keeps hand-built binaries runnable
      # outside the shell; no GNU compiler or shared C++ runtime alternative.
      devShells = forAllSystems (
        pkgs:
        let
          packages = packagesFor pkgs;
          shellFor =
            package:
            (pkgs.mkShell.override {
              stdenv = package.dependencies.stdenv;
            })
              {
                inputsFrom = [ package ];
                buildInputs = [ package.dependencies.gbenchmark ];
                XCLANG_ROOT = package.dependencies.cc.bundledRuntime;
                ASTERBOUND_BUILD_RPATH = nixpkgs.lib.makeLibraryPath package.runtimeLibraries;
                # Manual CMake invocations can pass $cmakeFlags, or use the
                # exported source path as the standard FetchContent cache option.
                FETCHCONTENT_SOURCE_DIR_STAR_LUA = "${package.luaSource}";
                cmakeFlags = [
                  (nixpkgs.lib.cmakeFeature "FETCHCONTENT_SOURCE_DIR_STAR_LUA" "${package.luaSource}")
                ];
                # The package patches literal ImGui Lua format strings; the shell
                # builds the working tree without applying packaging patches.
                hardeningDisable = [ "format" ];
              };
        in
        {
          default = shellFor packages.default;
          full = shellFor packages.full;
        }
      );

      checks = forAllSystems (pkgs: {
        default = (packagesFor pkgs).default;
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
