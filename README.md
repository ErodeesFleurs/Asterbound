# OpenStarbound

OpenStarbound is a fork of the [Starbound 1.4.4 source code](https://archive.org/details/starbound_source_code), with bug fixes, new features and performance improvements.

**You must own Starbound.** Commercial game assets are not included.

## Installation

1. Download the [latest release](https://github.com/OpenStarbound/OpenStarbound/releases/latest) and install or extract it separately from vanilla Starbound.
2. Copy `assets/packed.pak` from your Starbound installation into OpenStarbound's `assets/` directory.
3. Optionally copy `storage/` for saves/settings, `mods/` for local mods and `assets/user/` for instrument songs. Keep Steam open to load subscribed Workshop mods.

[Nightly builds](https://github.com/OpenStarbound/OpenStarbound/actions?query=branch%3Amain) may be less stable.

## Features and documentation

- Multithreaded, extended-color lighting and additive point lights.
- Lua asset/image patches, Lua-managed chat and status-effect HUDs, scripted windows and custom keybindings.
- Positional Opus voice chat, character swapping, multiple fonts and instrument volume control.
- Improved mod compatibility, inventory recovery, world storage and client-side tile prediction. Custom tile collision requires an OpenStarbound server.

Use `/help` for commands. See the [command reference](assets/opensb/help.config.patch), [Lua API](doc/lua/openstarbound) and [JSON extensions](doc/json/openstarbound). Many StarExtensions mods remain compatible; StarExtensions itself is deprecated.

### Lua 5.5.1 migration

Compound assignment, safe navigation, `math.round` and `math.copysign` remain available.

- `global` is reserved; rename it or use `_ENV["global"]`.
- Numeric loop variables and the first generic-loop variable are read-only. OSB repairs vanilla `/scripts/actions/flying.lua` and `/stagehands/coordinator/npccombat.lua` in memory, preserving their fractional sample bounds; mod scripts still need migration.
- Rebuild Lua 5.3 bytecode. Finalizer errors become warnings; `math.random` sequences may change.
- Text templates use `<name>` and `<<` for a literal `<`, not brace-format placeholders.

## Building

Requires **C++23**, **CMake 3.28+**, **Ninja 1.11+** and [xclang](https://github.com/clice-io/xclang) **23.1.2.9** with bundled libc++/`std` modules. Dependencies must use the same C++ runtime; Windows uses MinGW. GCC, Apple Clang and MSVC builds are unsupported.

All configurations use static mimalloc with `MI_OVERRIDE=OFF`; dependencies must not mix allocator heaps.

Lua is fetched with a [version/hash lock](cmake/dependencies/lua.json) and [project patch](cmake/patches/lua-openstarbound.patch); Git is required unless using Nix or prepatched sources.

### Nix on Linux

Supports x86_64 and ARM64. From the repository root:

```sh
nix build --cores 16 --max-jobs 1
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/starbound
```

Variants: `.#server` (headless), `.#full` (Qt tools and Steam/Discord), `.#asserts` and `.#sanitized`. ARM64 supports SDK-free variants only. Use `nix develop` or `nix develop .#full` for development.

Writable data defaults to `$XDG_DATA_HOME/openstarbound` (`~/.local/share/openstarbound`). Override with `OPENSTARBOUND_RUNTIME_DIR`, `OPENSTARBOUND_MODS`, `OPENSTARBOUND_STORAGE` or `OPENSTARBOUND_LOGS`.

### Native CMake presets

Bootstrap [vcpkg](https://github.com/microsoft/vcpkg#quick-start) **outside this repository**, set `VCPKG_ROOT`, and keep the repository's overlay ports/triplets.

| Platform | Preset | Requirements |
|---|---|---|
| Linux x86_64 | `linux-release` | Native C SDK (`libc6-dev`) and packages listed in [CI](.github/workflows/build.yml) |
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

Binaries go to `dist/`. Before launching, supply assets, `sbinit.config` and vendor runtimes; see the [Linux](scripts/ci/linux), [Windows](scripts/ci/windows) and [macOS](scripts/ci/macos) assembly scripts. macOS tests also need `libsteam_api.dylib` beside the executable. Distributions never include commercial `packed.pak`.

### Windows cross-builds

From Linux, enter `nix develop .#full` and set the following **before** running the compiler installer above:

```sh
export OSB_NATIVE_COMPILER_ROOT="$(dirname "$(dirname "$(command -v clang)")")"
```

Then use the `windows-cross` preset with the same vcpkg/bootstrap steps. Tests require Wine 11+ and `-DCMAKE_CROSSCOMPILING_EMULATOR=wine`, or a native Windows runner. macOS cross-builds remain unverified and require an authorized Apple SDK.

## Tests and benchmarks

CTest presets run asset-free core tests.

For real-asset tests:

```sh
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets nix run .#game-tests
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets nix run .#asserts-tests
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets OPENSTARBOUND_TEST_JOBS=4 nix run .#sanitized-tests
```

Each case gets a private runtime. `OPENSTARBOUND_TEST_JOBS` defaults to `1`; reports go to `./game-tests-report`, overridden by `OPENSTARBOUND_TEST_OUT`.

Sanitized builds enable mimalloc's `MI_TRACK_ASAN` and `MI_DEBUG`. Small overruns may be detected on free rather than at the write. LSan checks intercepted system allocations, **not individual mimalloc allocations**; passing it does not prove engine/Lua allocations are leak-free.

Benchmarks are opt-in:

```sh
nix build .#benchmarks
./result/bin/core_benchmarks --benchmark_repetitions=7 --benchmark_out=core.json --benchmark_out_format=json
# GUI cases: nix build .#benchmarks-gui -o result-benchmarks-gui
```

Native presets need `STAR_BUILD_BENCHMARKS=ON` and `VCPKG_MANIFEST_FEATURES="qt-tools;benchmarks"`; see [benchmark targets](source/benchmark/CMakeLists.txt). Asset-backed cases need owned assets and a private `-bootconfig`; **never use your saves**. GUI cases require a real display. Compare identical inputs/build flags with repeated samples, not shared-CI timing thresholds.

Use `--benchmark_filter='Lua/'` for invocation, callback, return-arity, context and coroutine cases. Asset workers share one Lua VM; additional workers do not parallelize Lua execution.

## Development

- C++ uses named modules, `import std;` and `std::expected`, without RTTI or exceptions. Use fresh build trees after compiler, ABI or sanitizer changes.
- Configure clangd with `nix develop -c bash scripts/ide/configure-clangd.sh` and its generated compilation database; enable clangd 23's `--experimental-modules-support`.
- Check changed C++/module lines with the pinned formatter; use `--apply` to format or `--base <commit>` for a branch comparison:

```sh
nix shell --inputs-from . nixpkgs#python3 nixpkgs#clang-tools --command scripts/format-source.sh --check
```

### Lua embedding

- `LuaConverter<T>` converts directly through `LuaStack` / `LuaStackValue`, including coroutine callbacks. Stack and string views are borrowed; retain values with `own()` or owning Lua handle types.
- Typed calls request zero, one, tuple-N or all results for `void`, scalar, tuple or `LuaVariadic` returns. Unused results are discarded before conversion.
- `LuaBaseComponent` owns session execution, reload and shutdown. Context retirement waits for the outermost call; workers are joined before dependent bindings disappear. Evaluation errors retain the context; invocation/message errors disable it.
- Require callbacks take borrowed `(LuaStackValue environment, StringView module)` and execute through `environment.stack().load(environment, bytes)`.

### Lua module loading

- `require("/absolute/asset.lua")` executes in the caller's environment and coroutine, returning no values. Entry scripts and nested requires execute once per context; only bytecode is shared between contexts. `_SBLOADED` is no longer a mutable cache hook.
- Cycles report the asset-path chain. Failed modules can be retried; successful dependencies remain loaded. Global changes and native side effects are not rolled back.
- Reload/hot reload and engine restart invalidate cached bytecode. Old contexts retain loaded modules but must be recreated to load new ones. Invalidation during loading rejects completion rather than mixing revisions.

## Community

[Issues and contributions](https://github.com/OpenStarbound/OpenStarbound/issues) are welcome.

<details>
<summary>Discord invite</summary>

Reverse the text in this image to obtain the invite code (spam prevention):

<img width="380" height="70" alt="Reversed Discord invite code" src="https://github.com/user-attachments/assets/9db11a89-0c32-4034-8555-68b54e5918a8" />

</details>
