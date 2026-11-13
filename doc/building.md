# Building and running Asterbound

[Project overview](../README.md) · [Tests and benchmarks](testing.md) · [Development guide](development.md)

Run commands from the repository root unless a step changes directories.

- [Requirements](#requirements)
- [Linux without Nix](#linux-without-nix)
- [Nix on Linux](#nix-on-linux)
- [Runtime paths and data import](#runtime-paths-and-data-import)
- [Native CMake presets](#native-cmake-presets)
- [Packaging and version metadata](#packaging-and-version-metadata)
- [Continuous integration](#continuous-integration)
- [ThinLTO](#thinlto)
- [Steam and Discord SDKs](#steam-and-discord-sdks)
- [Windows cross-builds](#windows-cross-builds)

## Requirements

| Component | Requirement |
|---|---|
| Language | C++23 with named modules and `import std` |
| Compiler | [xclang 23.1.3.6](https://github.com/clice-io/xclang/releases/tag/23.1.3.6), LLVM 23.1.3, bundled libc++ |
| Build tools | CMake 3.28+, Ninja 1.11+ |
| Dependencies | Nix, or vcpkg with this repository's overlay ports/triplets |
| Game assets | `packed.pak` from an owned Starbound installation; never redistributed |

GCC, Apple Clang and MSVC builds are unsupported. Windows uses MinGW. Dependencies must use the same C++ runtime; all configurations use static mimalloc with `MI_OVERRIDE=OFF` and must not mix allocator heaps.

Lua is fetched from the official [`lua/lua`](https://github.com/lua/lua) GitHub repository at the Lua 5.5.1 commit pinned by the [source URL and archive hash](../cmake/dependencies/lua.json), then receives the [project patch](../cmake/patches/lua-openstarbound.patch). Git is required unless using Nix or supplying prepatched Lua sources. Both CMake and Nix use the same lock and patch; prepatched source overrides must expose `lua.h` and the C sources directly at their root.

## Linux without Nix

This is the native build route for conventional Linux distributions; it does not
require Nix. Ubuntu 24.04 is the CI baseline on x86_64 and ARM64. Other
distributions need equivalent development packages and the requirements above;
their package names and runtime compatibility may differ.

### System dependencies

On Ubuntu 24.04, install the bootstrap tools and native libraries used by the
[Linux CI job](../.github/workflows/build.yml):

```sh
sudo apt-get update
sudo apt-get install -y build-essential git curl zip unzip tar python3 cmake ninja-build \
  libc6-dev pkgconf libxmu-dev libgl-dev libglu1-mesa-dev libasound2-dev libpulse-dev \
  libaudio-dev libjack-dev libsndio-dev libx11-dev libxext-dev \
  libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev \
  libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev \
  libegl1-mesa-dev libdbus-1-dev libibus-1.0-dev libudev-dev libpipewire-0.3-dev \
  libwayland-dev libdecor-0-dev liburing-dev '^libxcb.*-dev' libx11-xcb-dev \
  libxrender-dev libxkbcommon-x11-dev autoconf autoconf-archive automake \
  libtool libltdl-dev patchelf tarlz
```

The system compiler supplied by `build-essential` is for dependency bootstrap;
the engine itself must use the pinned xclang toolchain, not GCC.

### Configure, build and test

From the Asterbound repository root, bootstrap vcpkg outside the source tree.
If you already have a bootstrapped checkout, set `VCPKG_ROOT` to it and skip the
clone/bootstrap commands:

```sh
export VCPKG_ROOT="$HOME/vcpkg"
git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

export VCPKG_MAX_CONCURRENCY=4
cmake -DXCLANG_INSTALL_DIR="$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
export XCLANG_ROOT="$PWD/build/compiler-sdk/xclang"
cd source
cmake --preset linux-release
cmake --build --preset linux-release --parallel 4
ctest --preset linux-release
cd ..
```

On ARM64, replace `linux-release` in all three commands with
`linux-arm-release`. Adjust both concurrency settings for available memory.
The presets select the repository's vcpkg overlays/triplets and run asset-free
tests; see [asset-backed tests](testing.md#native-ctest-suites) for owned assets.

The compiler installer downloads the pinned xclang release; configuration
downloads/builds vcpkg dependencies and Lua. First builds require network access.
Outputs go to `dist/`, but this alone is not a runnable installation:
follow [packaging](#packaging-and-version-metadata) to assemble the distribution,
then add your owned `packed.pak`. `ASTERBOUND_ASSETS` configures the Nix launcher,
not a bare native executable.

## Nix on Linux

### NixOS

Use the flake rather than the host-package/vcpkg instructions above. On NixOS,
add this setting to your existing system configuration if the features are not
already enabled, and rebuild that configuration using your normal workflow:

```nix
nix.settings.experimental-features = [ "nix-command" "flakes" ];
```

Alternatively, enable them for one invocation without changing the system:

```sh
nix --extra-experimental-features 'nix-command flakes' build
```

The commands below work on NixOS and on other Linux distributions with Nix
installed. They do not require `nixos-rebuild`, a system package installation or
an independently installed xclang/vcpkg toolchain.

### Build and run

Supports x86_64 and ARM64:

```sh
nix build
ASTERBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/asterbound
```

`ASTERBOUND_ASSETS` points to a directory containing the owned `packed.pak`, not to the file itself. Bundled Asterbound resources are supplied separately by the package.

| Output | Purpose |
|---|---|
| `.#default` / `.#asterbound` | Client, server and asset tools; Steam enabled on x86_64 |
| `.#server` | Headless server without GUI/vendor SDK dependencies |
| `.#full` | Steam and Qt tools; requires x86_64 |
| `.#asserts` | Engine assertions enabled |
| `.#sanitized` | ASan/UBSan instrumentation |
| `.#benchmarks` / `.#benchmarks-gui` | Opt-in benchmark targets |

Discord is disabled in all default variants. ARM64's default package is SDK-free. See the [testing guide](testing.md) for asset-backed runners and benchmark usage.

Build variants into different result links when retaining more than one:

```sh
nix build .#server -o result-server
ASTERBOUND_ASSETS=/path/to/Starbound/assets ./result-server/bin/asterbound_server
```

Use `nix develop` for the default development environment or `nix develop .#full` for Qt tools. Limit `--cores` and `--max-jobs` to suit the machine; ThinLTO can use additional memory during linking.

## Runtime paths and data import

### Nix paths

The launcher creates a writable runtime under `$XDG_DATA_HOME/asterbound`, falling back to `~/.local/share/asterbound`.

| Environment variable | Default |
|---|---|
| `ASTERBOUND_RUNTIME_DIR` | The runtime root above |
| `ASTERBOUND_ASSETS` | `<runtime>/assets` |
| `ASTERBOUND_MODS` | `<runtime>/mods` |
| `ASTERBOUND_STORAGE` | `<runtime>/storage` |
| `ASTERBOUND_LOGS` | `<runtime>/logs` |

The launcher writes `<runtime>/sbinit.config`. An explicit `-bootconfig` bypasses this generated path configuration. The engine accepts single-dash long options, such as `-version`, `-help` and `-bootconfig`; old `OPENSTARBOUND_*` environment variables are not aliases.

### Import OpenStarbound data

Stop both games and servers, keep a backup, then copy into an absent or completely empty target:

```sh
./result/bin/asterbound-import-openstarbound \
  "$HOME/.local/share/openstarbound" "$HOME/.local/share/asterbound"
```

The target argument is optional and defaults to the Asterbound runtime root. The tool:

- Copies `assets/`, `mods/` and `storage/`, including user configuration in storage, without modifying the source.
- Copies linked files as independent data; it does not share writable saves.
- Excludes `sbinit.config`, logs and log rotations.
- Refuses populated targets, same/nested source and target paths, and directory-link cycles.
- Leaves a partial target after a failed copy rather than automatically deleting data.

Move old bundled `opensb.pak` / `opensb-compat.pak` files or corresponding resource directories out of scanned asset directories before launch. Their old metadata identities do not replace the new Asterbound sources and can cause duplicate patches. Keep the owned `packed.pak` and your local content; migrate Mod dependencies as described in [resource names and Mod compatibility](modding.md#bundled-mod-compatibility).

### Portable distributions and manual builds

Install into a separate directory and explicitly copy the saves/mods you want to import. A built executable alone is not an assembled distribution: it still needs assets, a boot configuration and any enabled vendor runtimes. Do not point two running installations at the same writable storage.

## Native CMake presets

Bootstrap [vcpkg](https://github.com/microsoft/vcpkg#quick-start) outside this repository, set `VCPKG_ROOT`, and retain the repository's overlay ports/triplets.

| Platform | Preset | Additional requirements |
|---|---|---|
| Linux x86_64 | `linux-release` | Native C SDK (`libc6-dev`) and packages listed in [CI](../.github/workflows/build.yml) |
| Linux ARM64 | `linux-arm-release` | Same; Steam/Discord disabled |
| Windows x64 | `windows-release` | MSYS2 UCRT64 `make`; set `MSYS2_ROOT` and add `ucrt64/bin` to `PATH` |
| macOS Apple Silicon | `macos-arm-release` | Xcode Command Line Tools, macOS 14-capable SDK, Homebrew `autoconf autoconf-archive automake libtool tarlz` |

### macOS

Export `SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"` and use `macos-arm-release`:

```sh
export VCPKG_ROOT=/absolute/path/to/vcpkg
export VCPKG_MAX_CONCURRENCY=16
cmake -DXCLANG_INSTALL_DIR="$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
export XCLANG_ROOT="$PWD/build/compiler-sdk/xclang"
cd source
cmake --preset macos-arm-release
cmake --build --preset macos-arm-release --parallel 16
ctest --preset macos-arm-release
```

### Windows PowerShell

With `VCPKG_ROOT` and `MSYS2_ROOT` configured:

```powershell
$env:VCPKG_MAX_CONCURRENCY = "16"
cmake "-DXCLANG_INSTALL_DIR=$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
$env:XCLANG_ROOT = "$PWD/build/compiler-sdk/xclang"
cd source
cmake --preset windows-release
cmake --build --preset windows-release --parallel 16
ctest --preset windows-release
```

Both examples run asset-free tests through the preset. Run [asset-backed tests](testing.md#native-ctest-suites) separately. Adjust concurrency for available memory.

Native Linux uses the host C SDK and xclang's static C++ runtime. CI targets Ubuntu 24.04/glibc 2.39, **not** glibc 2.17. Offline builds can set `FETCHCONTENT_SOURCE_DIR_STAR_LUA` to prepatched sources containing `lua.h` and C sources at their root, with `FETCHCONTENT_FULLY_DISCONNECTED=ON`.

## Packaging and version metadata

Binaries go to `dist/`: `asterbound`, `asterbound_server` and enabled tools (`.exe` on Windows). Assemble the distribution with the platform's [Linux](../scripts/ci/linux/assemble.sh), [Windows](../scripts/ci/windows/assemble.bat) or [macOS](../scripts/ci/macos/assemble.sh) script. Commercial `packed.pak` is never included. macOS tests also require `libsteam_api.dylib` beside the executable.

Windows CI publishes `Asterbound-Windows-Client.zip`, `Asterbound-Windows-Server.zip` and an installer. ZIPs are created with CMake. Matching `asterbound.pdb` and `asterbound_server.pdb` files accompany their executables; keep them together for symbolized stack traces.

Root [`version.json`](../version.json) is the shared version source:

| Field | Contract |
|---|---|
| `version` | Four-part Asterbound product version; consumed by C++, Nix, Windows resources and the installer |
| `assetVersion` | Shared version of both bundled asset sources and packs; also the engine's required `asterbound_base` version |
| `starboundVersion` | Base-game compatibility identifier, not the product version |

A product-version change alone does not require a wire or asset-version change. See [protocol and resource compatibility](modding.md#asterbound-compatibility-identity).

Edit `assetVersion` only in `version.json`, then synchronize both checked-in `_metadata` files:

```sh
cmake -DASTERBOUND_SYNC_ASSET_VERSIONS=ON -P cmake/AsterboundVersion.cmake
```

The command preserves metadata comments and formatting. Both `assets/asterbound` (`asterbound_base`) and `assets/asterbound-compat` (`asterbound_compat`) use this asset release number. Asterbound's asset series starts at `0.1.0.0`, independent of the former upstream main and compatibility versions. Product and asset versions remain independently controlled.

`cmake -P cmake/AsterboundVersion.cmake` checks without writing. CMake configuration, CI source checks and all platform assembly scripts reject stale metadata. Changing either metadata file also triggers a CMake reconfiguration. The runtime's strict main-asset version check remains enabled for `asterbound_base`; network versions are unchanged.

macOS maps `a.b.c.d` to short version `a.b.c`, build version `(a*100+b).c.d`, and the full version in `CFBundleGetInfoString`. Each original component must be in `0..99` and the folded first component positive; unsupported values fail packaging. See Apple's [short-version](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleshortversionstring) and [build-version](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleversion) requirements.

Linux validation is not proof of Windows/macOS native packaging or SDK interoperability; use each platform's runner for those checks.

## Continuous integration

The native [Build workflow](../.github/workflows/build.yml) retains Windows x64, Linux x86_64/ARM64 and macOS ARM64 coverage and all development/client/server/installer artifacts. Asset-free tests run before distribution compression. Fast source checks have a 10-minute deadline; native and sanitizer jobs have 120-minute deadlines. New push/PR runs cancel superseded runs for the same event and branch; manual dispatches remain independent. Branch names containing `/` are supported.

The shared [native build action](../.github/actions/native-build/action.yml) pins CMake 3.29.2, Ninja 1.13.2, xclang 23.1.3.6 and immutable third-party Action revisions. Lua download/hash/patch preflight runs before expensive dependency configuration and exports the same prepatched source tree to the engine.

The GitHub Lua source tree does not ship the release-only `lua.hpp` wrapper. C++ implementation, module-global-fragment and test consumers include the official `lua.h`, `lualib.h` and `lauxlib.h` under `extern "C"` directly. Download/patch preflight is not a substitute for compiling the actual `star.lua` consumers and running `core_tests`.

Caches are restored and saved separately. Completed vcpkg packages survive later configuration/build/test failures; changed cache inventories produce new immutable keys instead of trying to overwrite an existing key. Keys include the compiler, architecture, preset and actual SDK/runner image identity. The compiler cache also receives the SDK identity through `SCCACHE_C_CUSTOM_CACHE_BUSTER`.

CI uses sccache 0.17.0 for C/C++ objects and named-module BMI sidecars, with direct mode disabled. Native `clang-scan-deps` and linking remain outside the compiler cache. The local disk compiler cache is limited to 512 MiB; ThinLTO's cache pruning policy targets 512 MiB. Neither cache restores across SDK identities. Do not replace this with ccache or cache an entire build directory: standard-module support and SDK/BMI identity must be preserved.

Windows builds sccache from the SHA-256-pinned v0.17.0 source archive using [the installer](../scripts/ci/install-sccache.py), Rust 1.90.0 and `cargo build --locked --no-default-features` (local disk cache only). The [narrow source patch](../scripts/ci/sccache-windows-clang-response.patch) addresses [upstream issue 2397](https://github.com/mozilla/sccache/issues/2397): expanding Ninja's response file can still produce a Windows Clang preprocessor/compile command beyond `CreateProcessW`'s 32,767 UTF-16-unit limit. Only detected GNU-driver Clang commands on Windows regenerate an oversized argument list as a temporary UTF-8 GNU-quoted response file, retained until the child finishes; cache hashing keeps the expanded arguments. GCC, clang-cl/MSVC, compiler discovery, dependency scanning and other platforms are unchanged. Linux/macOS retain the pinned official sccache Action.

The patched executable cache includes architecture, runner image and installer/patch hashes; Windows object-cache keys also include those source-build inputs. Before configuring dependencies, the [Windows runtime smoke](../scripts/ci/smoke-sccache-windows.py) compiles an incoming response file exceeding 32K with spaces, quotes, backslashes and non-ASCII arguments, runs the linked result, deletes/restores its object through a measured cache hit, and checks macro invalidation. Its private foreground server does not change the default daemon.

Configuration, build timings, Ninja logs, resource samples and sccache statistics are uploaded as `Build-Diagnostics-<preset>`; failed CTest runs also upload their logs and JUnit results. The profiler's `--sccache` mode starts a private foreground server so compiler children remain inside the measured process tree/Windows Job, collects statistics before flushing and stopping it, and cleans up owned processes on cancellation without stopping unrelated cache servers. Timing summaries report successful link/archive edges from that invocation, not failed links or additive wall time. Resource semantics are recorded alongside the measurements.

The nightly [Sanitizers workflow](../.github/workflows/sanitized.yml) uses Nix 2.35.0 and a GitHub Actions-backed Nix store cache; no external Cachix account or new secret is required. Valid completed store paths can be saved after a failed build. Its 4 GiB GC setting is a target, not a hard quota for rooted closures. Nix, vcpkg, compiler and link caches share the repository's Actions cache quota; monitor eviction and restore/save costs after rollout.

Before saving the Nix store, `cache-state.py nix-snapshot --exclude-file ...` writes an explicit GNU tar exclusion list for unregistered top-level store locks. Registered outputs named `Cargo.lock` are preserved; other unreadable files remain errors. The save-only `TAR_OPTIONS` leaves cache path/version matching unchanged and neither deletes locks nor changes their ownership/permissions. Fast source checks run `scripts/ci/smoke-nix-cache.py` against real unreadable files and archive byte/mode checks as an unprivileged Linux user.

Linux/macOS `.tar.lz` distributions use tarlz level 3 without changing their contents or format. Measurements used identical existing Linux client/server inputs and four workers:

| tarlz level | Combined time, pass 1 | Combined time, pass 2 | Combined archive bytes |
|---|---:|---:|---:|
| 9 | 148.16 s | 163.38 s | 179,619,993 |
| 3 | 60.58 s | 84.29 s | 203,094,204 |

Level 3 trades 13.07% more archive bytes for shorter compression. Byte/mode/type/symlink round trips passed. These are local packaging measurements, not macOS or whole-CI speedup claims.

## ThinLTO

ThinLTO defaults to ON for `Release`, `RelWithDebInfo`, `RelWithAsserts` and `MinSizeRel`; Debug is unchanged. CMake checks C/C++ IPO support and uses xclang's `-flto=thin` with matching LLVM archive tools for project targets, including `std` modules and Lua. Prebuilt dependencies are not rebuilt with LTO.

Pass `-DSTAR_ENABLE_THINLTO=OFF` at configure time to disable it. Changing the option rebuilds affected objects and libraries. Linking needs more time and memory; performance gains depend on workload.

For CI-style link control, configure `-DSTAR_LINK_JOBS=1 -DSTAR_THINLTO_CACHE_DIR=/absolute/cache/path`. Both options are opt-in; ordinary local builds keep their previous scheduler and no persistent link cache. `STAR_LINK_JOBS` requires Ninja and a positive integer. CI conservatively starts with one simultaneous link/archive job because each ThinLTO link can use multiple backend threads; use the uploaded timings and memory reports before increasing it. Debug, ThinLTO-OFF builds and individual targets opting out of IPO do not receive ThinLTO cache flags.

A local two-executable ThinLTO fixture also compared link concurrency. These wall times include the profiling wrapper; RSS is a sampled process-tree sum, not an exact peak. The fixture supports starting conservatively, not a universal engine concurrency recommendation:

| Concurrent links | Cold-cache wall time | Warm-cache wall time | Cold-cache sampled RSS |
|---|---:|---:|---:|
| 1 | 4.98 s | 0.60 s | 297 MiB |
| 2 | 7.86 s | 0.61 s | 537 MiB |

## Steam and Discord SDKs

| Integration | Dependency | Default |
|---|---|---|
| Steam | Steamworks SDK 1.65(a), [mirror commit `df2baab`](https://github.com/rlabrecque/SteamworksSDK/commit/df2baabf574a738ef1ea90a7e89339107fc0a279) | Enabled for the default x86_64 Nix package and supported native presets |
| Discord | [Game SDK 3.2.1](https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip), with local portability fixes | Disabled; requires your own application registration |

Valve's [official SDK download](https://partner.steamgames.com/doc/sdk) requires a Steamworks account; the mirror normalizes text line endings. All three desktop platforms use matching headers/libraries. Steam statistics use the preloaded cache; SDK 1.61 removed `RequestCurrentStats`.

Keep Steam running and use an account owning Starbound. The Nix client launcher defaults `SteamAppId` to Starbound's `211820` without overriding a nonempty supplied value or changing the working directory. Do not invent a new Steam ID for a product rename.

### Enabling Discord

Append both options to the native `cmake --preset` configure command, replacing
the ID with your registered application ID:

```sh
cmake --preset linux-release -DSTAR_ENABLE_DISCORD_INTEGRATION=ON \
  -DSTAR_DISCORD_APPLICATION_ID="$DISCORD_APPLICATION_ID"
```

Run this from `source/` after setting `DISCORD_APPLICATION_ID`. A Nix package
override uses `discordSupport = true; discordApplicationId = "<your-registered-id>";`.
The ID must be a positive decimal string fitting an int64. Registering the
application and configuring its presence assets are external steps.

Discord has [archived Game SDK](https://docs.discord.com/developers/developer-tools/game-sdk). This integration retains presence, invitations and lobbies rather than migrating to Social SDK:

- Presence does not require a fetched Discord user.
- Unsupported `CREATE_LOBBY` disables Discord lobby networking for that session; presence, Steam networking and direct-IP connections remain separate. No repeated lobby-creation attempts occur on later world joins.
- `UpdateActivity` failures are warnings. Unchanged failed activities are not retried each frame; later changed activities can publish. Lobby/network and logging failures retain their checked error boundaries.

The `discord_lobby_tests` target covers these SDK boundaries when enabled. SDK availability does not prove that overlays, invitations or multiplayer work on every platform.

## Windows cross-builds

From Linux, enter `nix develop .#full` and export the native compiler root before running the compiler installer from the native-build instructions:

```sh
export OSB_NATIVE_COMPILER_ROOT="$(dirname "$(dirname "$(command -v clang)")")"
```

Use `windows-cross` with the same vcpkg/bootstrap steps. Tests require Wine 11+ and `-DCMAKE_CROSSCOMPILING_EMULATOR=wine`, or a native Windows runner. macOS cross-builds remain unverified and require an authorized Apple SDK.
