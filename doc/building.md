# Building and running Asterbound

[Project overview](../README.md) · [Tests and benchmarks](testing.md) · [Development guide](development.md)

Run commands from the repository root unless a step changes directories.

- [Requirements](#requirements)
- [Nix on Linux](#nix-on-linux)
- [Runtime paths and data import](#runtime-paths-and-data-import)
- [Native CMake presets](#native-cmake-presets)
- [Packaging and version metadata](#packaging-and-version-metadata)
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

Lua is fetched with a [version/hash lock](../cmake/dependencies/lua.json) and a [project patch](../cmake/patches/lua-openstarbound.patch). Git is required unless using Nix or supplying prepatched Lua sources.

## Nix on Linux

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

Move old bundled `opensb.pak` / `opensb-compat.pak` files or corresponding resource directories out of scanned asset directories before launch, so they do not shadow packaged Asterbound resources. Keep the owned `packed.pak` and your local content. See [resource names and Mod compatibility](modding.md#bundled-mod-compatibility).

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

### Linux and macOS

Replace `linux-release` with the target preset. On macOS, also export `SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"` before configuring.

```sh
export VCPKG_ROOT=/absolute/path/to/vcpkg
export VCPKG_MAX_CONCURRENCY=16
cmake -DXCLANG_INSTALL_DIR="$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
export XCLANG_ROOT="$PWD/build/compiler-sdk/xclang"
cd source
cmake --preset linux-release
cmake --build --preset linux-release --parallel 16
ctest --preset linux-release
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

Native Linux uses the host C SDK and xclang's static C++ runtime. CI targets Ubuntu 24.04/glibc 2.39, **not** glibc 2.17. Offline builds can set `FETCHCONTENT_SOURCE_DIR_STAR_LUA` to prepatched sources containing `src/`, with `FETCHCONTENT_FULLY_DISCONNECTED=ON`.

## Packaging and version metadata

Binaries go to `dist/`: `asterbound`, `asterbound_server` and enabled tools (`.exe` on Windows). Assemble the distribution with the platform's [Linux](../scripts/ci/linux/assemble.sh), [Windows](../scripts/ci/windows/assemble.bat) or [macOS](../scripts/ci/macos/assemble.sh) script. Commercial `packed.pak` is never included. macOS tests also require `libsteam_api.dylib` beside the executable.

Windows CI publishes `Asterbound-Windows-Client.zip`, `Asterbound-Windows-Server.zip` and an installer. ZIPs are created with CMake. Matching `asterbound.pdb` and `asterbound_server.pdb` files accompany their executables; keep them together for symbolized stack traces.

Root [`version.json`](../version.json) is the shared version source:

| Field | Contract |
|---|---|
| `version` | Four-part Asterbound product version; consumed by C++, Nix, Windows resources and the installer |
| `assetVersion` | Required `assets/asterbound/_metadata` version; keep the engine/resource match check |
| `starboundVersion` | Base-game compatibility identifier, not the product version |

A product-version change alone does not require a wire or asset-version change. See [protocol and resource compatibility](modding.md#asterbound-compatibility-identity).

macOS maps `a.b.c.d` to short version `a.b.c`, build version `(a*100+b).c.d`, and the full version in `CFBundleGetInfoString`. Each original component must be in `0..99` and the folded first component positive; unsupported values fail packaging. See Apple's [short-version](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleshortversionstring) and [build-version](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleversion) requirements.

Linux validation is not proof of Windows/macOS native packaging or SDK interoperability; use each platform's runner for those checks.

## ThinLTO

ThinLTO defaults to ON for `Release`, `RelWithDebInfo`, `RelWithAsserts` and `MinSizeRel`; Debug is unchanged. CMake checks C/C++ IPO support and uses xclang's `-flto=thin` with matching LLVM archive tools for project targets, including `std` modules and Lua. Prebuilt dependencies are not rebuilt with LTO.

Pass `-DSTAR_ENABLE_THINLTO=OFF` at configure time to disable it. Changing the option rebuilds affected objects and libraries. Linking needs more time and memory; performance gains depend on workload.

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
