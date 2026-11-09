# Building OpenStarbound

[Project overview](../README.md) · [Tests and benchmarks](testing.md) · [Development guide](development.md)

Run the commands below from the repository root unless a step changes directories.

Requires **C++23**, **CMake 3.28+**, **Ninja 1.11+** and [xclang](https://github.com/clice-io/xclang/releases/tag/23.1.3.6) **23.1.3.6** (LLVM **23.1.3**) with bundled libc++/`std` modules. Dependencies must use the same C++ runtime; Windows uses MinGW. GCC, Apple Clang and MSVC builds are unsupported.

All configurations use static mimalloc with `MI_OVERRIDE=OFF`; dependencies must not mix allocator heaps.

Lua is fetched with a [version/hash lock](../cmake/dependencies/lua.json) and [project patch](../cmake/patches/lua-openstarbound.patch); Git is required unless using Nix or prepatched sources.

## ThinLTO

ThinLTO is enabled by default for `Release`, `RelWithDebInfo`, `RelWithAsserts` and `MinSizeRel`, including Nix packages and native presets. Debug is unchanged. CMake checks C/C++ IPO support and uses xclang's `-flto=thin` with matching LLVM archive tools for project targets, including `std` modules and Lua; prebuilt dependencies are not rebuilt with LTO.

Pass `-DSTAR_ENABLE_THINLTO=OFF` when configuring to disable the project default. Changing this option rebuilds affected objects and libraries. ThinLTO adds link-time work and memory use; reduce build parallelism on memory-constrained machines. Runtime performance gains depend on the workload.

## Nix on Linux

Supports x86_64 and ARM64. From the repository root:

```sh
nix build --cores 16 --max-jobs 1
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/starbound
```

The default x86_64 GUI package enables Steam and Discord integration. Variants: `.#server` (headless, SDK-free), `.#full` (also includes Qt tools), `.#asserts` and `.#sanitized`. ARM64 remains SDK-free. Use `nix develop` or `nix develop .#full` for development.

Writable data defaults to `$XDG_DATA_HOME/openstarbound` (`~/.local/share/openstarbound`). Override with `OPENSTARBOUND_RUNTIME_DIR`, `OPENSTARBOUND_MODS`, `OPENSTARBOUND_STORAGE` or `OPENSTARBOUND_LOGS`.

## Steam and Discord SDKs

Steam headers and redistributable libraries are from **Steamworks SDK 1.65(a)**, pinned to [SteamworksSDK mirror commit `df2baab`](https://github.com/rlabrecque/SteamworksSDK/commit/df2baabf574a738ef1ea90a7e89339107fc0a279). Valve's [official download](https://partner.steamgames.com/doc/sdk) requires a Steamworks account; the public mirror normalizes text line endings. Linux, Windows and macOS use matching headers and libraries. Local statistics use Steam's preloaded cache; the `RequestCurrentStats` startup API was removed in SDK 1.61.

Discord binaries and C++ bindings use the official [Game SDK 3.2.1](https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip), with the existing local header portability fixes retained. Discord has [archived Game SDK](https://docs.discord.com/developers/developer-tools/game-sdk); this integration retains its existing presence, invitation and lobby APIs rather than migrating to Social SDK.

Rich Presence does not require a fetched Discord user, so presence-only RPC clients can publish status independently of lobby support. If the Discord client rejects `CREATE_LOBBY` as an unsupported command, OpenStarbound disables Discord lobby networking for that session, reports the capability loss, and keeps the game and Rich Presence running. It does not retry lobby creation on later world joins; Steam networking and direct-IP connections remain separate. Clients that support the lobby API retain the existing Discord P2P path.

An SDK `UpdateActivity` failure is a nonfatal warning, not an application shutdown. The SDK may still log its original error. An unchanged failed activity is not retried every frame; later changed activities can publish normally. Lobby/network and diagnostic-logging failures retain their checked error boundaries. The asset-free `discord_lobby_tests` CTest entry covers activity failure and subsequent publication, unsupported lobbies, successful creation, cancelled callbacks and unrelated failure diagnostics.

Keep Steam and the Discord desktop client running for their respective integrations. Standalone Nix client launchers default `SteamAppId` to Starbound's `211820`, without changing the working directory or overriding a nonempty supplied value. A Steam account owning Starbound is required. SDK availability does not imply that Steam Overlay or Discord multiplayer has been verified on every platform.

## Native CMake presets

Bootstrap [vcpkg](https://github.com/microsoft/vcpkg#quick-start) **outside this repository**, set `VCPKG_ROOT`, and keep the repository's overlay ports/triplets.

| Platform | Preset | Requirements |
|---|---|---|
| Linux x86_64 | `linux-release` | Native C SDK (`libc6-dev`) and packages listed in [CI](../.github/workflows/build.yml) |
| Linux ARM64 | `linux-arm-release` | Same; Steam/Discord disabled |
| Windows x64 | `windows-release` | MSYS2 UCRT64 `make`; set `MSYS2_ROOT` and add `ucrt64/bin` to `PATH` |
| macOS Apple Silicon | `macos-arm-release` | Xcode Command Line Tools, macOS 14-capable SDK, Homebrew `autoconf autoconf-archive automake libtool tarlz` |

Linux/macOS, from the repository root; replace the preset as needed:

```sh
export VCPKG_ROOT=/absolute/path/to/vcpkg
export VCPKG_MAX_CONCURRENCY=16
cmake -DXCLANG_INSTALL_DIR="$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
export XCLANG_ROOT="$PWD/build/compiler-sdk/xclang"
# macOS: export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
cd source
cmake --preset linux-release
cmake --build --preset linux-release --parallel 16
ctest --preset linux-release
```

Windows PowerShell, with `VCPKG_ROOT` and `MSYS2_ROOT` configured:

```powershell
$env:VCPKG_MAX_CONCURRENCY = "16"
cmake "-DXCLANG_INSTALL_DIR=$PWD/build/compiler-sdk" -P scripts/ci/install-xclang.cmake
$env:XCLANG_ROOT = "$PWD/build/compiler-sdk/xclang"
cd source
cmake --preset windows-release
cmake --build --preset windows-release --parallel 16
ctest --preset windows-release
```

Native Linux uses the host C SDK while retaining xclang's static C++ runtime. CI targets Ubuntu 24.04/glibc 2.39, **not** glibc 2.17. Offline builds can set `FETCHCONTENT_SOURCE_DIR_STAR_LUA` to prepatched Lua sources containing `src/`, with `FETCHCONTENT_FULLY_DISCONNECTED=ON`.

Binaries go to `dist/`. Before launching, supply assets, `sbinit.config` and vendor runtimes; see the [Linux](../scripts/ci/linux), [Windows](../scripts/ci/windows) and [macOS](../scripts/ci/macos) assembly scripts. macOS tests also need `libsteam_api.dylib` beside the executable. Distributions never include commercial `packed.pak`.

Windows CI publishes separate `OpenStarbound-Windows-Client.zip` and `OpenStarbound-Windows-Server.zip` artifacts, plus the installer. ZIP archives are created with CMake; no separate archiver is required. Client and server packages include their matching `starbound.pdb` and `starbound_server.pdb`; the installer includes both. Keep each PDB beside its executable to enable symbolized Windows stack traces.

## Windows cross-builds

From Linux, enter `nix develop .#full` and set the following **before** running the compiler installer above:

```sh
export OSB_NATIVE_COMPILER_ROOT="$(dirname "$(dirname "$(command -v clang)")")"
```

Then use the `windows-cross` preset with the same vcpkg/bootstrap steps. Tests require Wine 11+ and `-DCMAKE_CROSSCOMPILING_EMULATOR=wine`, or a native Windows runner. macOS cross-builds remain unverified and require an authorized Apple SDK.
