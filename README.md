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

### Selected upstream optimizations and fixes

- Image directives cache repeated hue/saturation/brightness conversions, skip exact neutral parameters, restrict blends to overlapping pixels, and prefilter large color replacements. Existing non-neutral color math is unchanged; neutral HSV directives preserve input bytes after any required RGB/BGR-to-alpha conversion. See upstream [color caching](https://github.com/OpenStarbound/OpenStarbound/commit/0cfbeb5f), [regional processing](https://github.com/OpenStarbound/OpenStarbound/commit/c4e5b17a) and [repeated replacements](https://github.com/OpenStarbound/OpenStarbound/commit/e1d575cd).
- [Texture cleanup](https://github.com/OpenStarbound/OpenStarbound/commit/21e584d2) spaces full scans by one tenth of the timeout, capped at one second. Expired textures can remain until the next scan; Root reloads still clear the group immediately.
- [Vanished loose patches](https://github.com/OpenStarbound/OpenStarbound/commit/8852bada) are skipped with a warning only after a failed read confirms that their directory entry is absent. This applies to JSON and Lua image patches; unreadable or replaced files, dangling symlinks, packed-source read failures and JSON/Lua errors still fail the asset load.
- [Referenced-image caching](https://github.com/OpenStarbound/OpenStarbound/commit/3f33581f) keeps recently used blend/mask images longer. Boot `assetsSettings.referenceImageTimeToLive` defaults to 300 seconds and `referenceImagePixelBudget` to 16777216 logical pixels (64 MiB for RGBA32, not a total-cache memory cap). Actual reference cache hits refresh recency; newest references fitting the budget receive the extended lifetime. A zero budget or reference TTL no greater than `assetTimeToLive` disables the extension. Ordinary assets keep their normal TTL; `clearCache` and Root reload still evict unheld references immediately, while externally held images retain normal persistence.
- [Known blueprint relearning](https://github.com/OpenStarbound/OpenStarbound/commit/9814f594) without a failure notification no longer constructs an Item or loads notification assets. New unlocks still validate Item construction before changing blueprint state.
- [Damage kind names](https://github.com/OpenStarbound/OpenStarbound/commit/b37ef403) are case-insensitive at insertion and lookup, including duplicate detection. Elemental type names remain case-sensitive and are validated against their own catalog.
- [Legacy humanoid back sleeves](https://github.com/OpenStarbound/OpenStarbound/commit/75eb1c9d) retain the armor's fullbright setting while holding items.
- [Physics-object force bounds](https://github.com/OpenStarbound/OpenStarbound/commit/b66ad015) remain in object-local coordinates for spatial indexing, including at large world positions. Returned force regions still use world coordinates.
- [Nearest-neighbor scaling](https://github.com/OpenStarbound/OpenStarbound/commit/334ad9fb) precomputes source-column offsets and selects the source row once per output row for byte images. Sampling still divides before rounding and clamps to the source edges; channel order, transparent RGB and the existing float-image behavior are preserved.
- [Scale/crop fusion](https://github.com/OpenStarbound/OpenStarbound/commit/c4e5b17a) computes only the retained window for eligible nearest/bilinear scales, including across directive groups. Invalid windows, negative scales, bicubic scales and float-image pipeline operations retain sequential execution.
- [Color-operation scheduling](https://github.com/OpenStarbound/OpenStarbound/commit/549af71b), including the [empty-image fix](https://github.com/OpenStarbound/OpenStarbound/commit/f097a32d), moves eligible pixel-local operations before an enlarging nearest scale or after a shrinking crop. Both directive execution entrypoints preserve output pixels, earlier execution errors and successfully applied prefixes before later errors.
- [Dirty-region replacement](https://github.com/OpenStarbound/OpenStarbound/commit/6fe1c895) restricts a replacement after consecutive blends to their affected union only when a preceding replacement removed its keys and the new key set is a qualifying subset. Other operations invalidate this shortcut.
- [Chat bubbles](https://github.com/OpenStarbound/OpenStarbound/commit/9f412531) measure text with the final per-message style and the same scaled wrap positioning used for drawing, keeping custom font sizes inside their backgrounds.
- Native [right-aligned TextBox overflow](https://github.com/OpenStarbound/OpenStarbound/commit/be262918) scrolls toward the cursor correctly. The existing cursor-offset calculation and Lua-managed chat input are unchanged.

### Network and frame preparation

Entity creation encoders transfer completed buffers instead of copying them. Read-only RPC and monster/NPC variant decoders borrow their input only during decoding; decoded fields remain owned. Network formats and compatibility versions are unchanged. Rendering transfers complete entity layer collections, while typed spatial queries collect a single owned result list without changing traversal order, wrapping, deduplication or insertion-safe candidate snapshots.

Team updates share one configuration snapshot within each update, without caching it across updates. Health, energy, position and travel state retain the configured status frequency. Names and portraits are included on the next eligible status update after ten seconds, or immediately on the next eligible update after first joining, changing teams or `forceUpdate`. Appearance changes can therefore reach other teammates later than dynamic status. The full-update timer advances only after successful local submission; encoding or enqueue failures do not consume it. Legacy RPC still maps remote failures to `null`, so submission is not a remote-delivery acknowledgment.


### Lua 5.5.1 migration

Compound assignment, safe navigation, `math.round` and `math.copysign` remain available.

- `global` is reserved; rename it or use `_ENV["global"]`.
- Numeric loop variables and the first generic-loop variable are read-only. OSB repairs vanilla `/scripts/actions/flying.lua` and `/stagehands/coordinator/npccombat.lua` in memory, preserving their fractional sample bounds; mod scripts still need migration.
- Rebuild Lua 5.3 bytecode. Finalizer errors become warnings; `math.random` sequences may change.
- Text templates use `<name>` and `<<` for a literal `<`, not brace-format placeholders.

### Bundled mod compatibility

`assets/opensb-compat` is a separate, optional-fix asset source, shipped as `opensb-compat.pak` beside `opensb.pak` in distributions and as a sibling directory in Nix packages. Normal asset-directory scanning loads it automatically; custom boot configurations using explicit `assetSources` must include it. It does not require any third-party mod to be installed.

For [StarCore Simplified Chinese](https://steamcommunity.com/sharedfiles/filedetails/?id=807695810) (`SChinese`, version `V108_241106_09`), it removes four unused imports from `/scripts/replaceTags.lua` in memory, breaking `util.lua → replaceTags.lua → util.lua`. The replacement function body, utility functions and translations remain unchanged; the engine still rejects cyclic `require` calls. The repair applies only when the effective script provider is that known SChinese version and its import header matches the [verified upstream release](https://github.com/sffxzzp/Starbound-Chinese/releases/tag/UTC-241105-1953). Other providers, unknown versions and unrecognized headers are left untouched. No installed mod files or saves are rewritten; restart the game after installing the compatibility source.

### Mod error diagnostics

Lua runtime errors retain their original traceback and append a deduplicated `Lua source providers` summary identifying the scripts on the failed stack, including coroutine failures. Engine-loaded scripts use their actual loaded asset provider: the mod's `friendlyName`, internal `name`, and directory or `.pak` path. Syntax and indexed-script read failures also include provider context. Missing or invalid optional metadata falls back to the source path. Chunk names remain unchanged; provider snapshots are recorded during loading, not by scanning mods every frame.

JSON and image Lua patch errors identify the target asset, patch path, and the exact executing patch provider, even when another mod overrides the same virtual patch path. Asset `onLoad`/`postLoad` failures also include provider metadata. These diagnostics identify code and patch providers, not necessarily the mod that caused a conflict; they do not track configuration changes field by field. Dynamically generated code without a registered asset provider retains its original error information.

Error logs present a concise, reason-first summary before `Details:`: Lua syntax/runtime failures, Lua patch syntax/runtime failures, and resource failures have separate categories, with execution phase, live script locations, and providers where available. Typed causes are retained through native callbacks and ordinary Lua rethrows; repeated wrapper contexts do not obscure the original reason. Distinct call sites remain visible. The complete original exception chain, Lua traceback and native stacks follow in the details rather than being suppressed.

Database per-file validation also adds the concrete asset path and provider where available, including valid JSON with missing or invalid fields. More specific nested asset and patch attribution takes precedence. Recoverable entries still log and skip or use their existing fallback; adding diagnostic context does not make those failures fatal.

Native errors capture stack locations at their origin, but defer symbol resolution until full details are requested. Error copies share immutable diagnostic payloads; short and full renderings are cached independently and support concurrent readers. Captured addresses and module-relative locations remain in full details when symbols or a loaded module are no longer available. Related cleanup failures retain both diagnostics without changing the primary error's classification.

Native callback errors keep their string-valued Lua representation, and built-in `error`/`pcall` behavior is unchanged. Summary transport is identity-bound and bounded; errors deliberately rewritten by a script are treated as new errors unless a complete known payload with a verified live Lua location prefix is preserved. This avoids guessing a mod or reusing a previous failure's attribution.

## Building

Requires **C++23**, **CMake 3.28+**, **Ninja 1.11+** and [xclang](https://github.com/clice-io/xclang/releases/tag/23.1.3.6) **23.1.3.6** (LLVM **23.1.3**) with bundled libc++/`std` modules. Dependencies must use the same C++ runtime; Windows uses MinGW. GCC, Apple Clang and MSVC builds are unsupported.

All configurations use static mimalloc with `MI_OVERRIDE=OFF`; dependencies must not mix allocator heaps.

Lua is fetched with a [version/hash lock](cmake/dependencies/lua.json) and [project patch](cmake/patches/lua-openstarbound.patch); Git is required unless using Nix or prepatched sources.

### Nix on Linux

Supports x86_64 and ARM64. From the repository root:

```sh
nix build --cores 16 --max-jobs 1
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/starbound
```

The default x86_64 GUI package enables Steam and Discord integration. Variants: `.#server` (headless, SDK-free), `.#full` (also includes Qt tools), `.#asserts` and `.#sanitized`. ARM64 remains SDK-free. Use `nix develop` or `nix develop .#full` for development.

Writable data defaults to `$XDG_DATA_HOME/openstarbound` (`~/.local/share/openstarbound`). Override with `OPENSTARBOUND_RUNTIME_DIR`, `OPENSTARBOUND_MODS`, `OPENSTARBOUND_STORAGE` or `OPENSTARBOUND_LOGS`.

### Steam and Discord SDKs

Steam headers and redistributable libraries are from **Steamworks SDK 1.65(a)**, pinned to [SteamworksSDK mirror commit `df2baab`](https://github.com/rlabrecque/SteamworksSDK/commit/df2baabf574a738ef1ea90a7e89339107fc0a279). Valve's [official download](https://partner.steamgames.com/doc/sdk) requires a Steamworks account; the public mirror normalizes text line endings. Linux, Windows and macOS use matching headers and libraries. Local statistics use Steam's preloaded cache; the `RequestCurrentStats` startup API was removed in SDK 1.61.

Discord binaries and C++ bindings use the official [Game SDK 3.2.1](https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip), with the existing local header portability fixes retained. Discord has [archived Game SDK](https://docs.discord.com/developers/developer-tools/game-sdk); this integration retains its existing presence, invitation and lobby APIs rather than migrating to Social SDK.

Rich Presence does not require a fetched Discord user, so presence-only RPC clients can publish status independently of lobby support. If the Discord client rejects `CREATE_LOBBY` as an unsupported command, OpenStarbound disables Discord lobby networking for that session, reports the capability loss, and keeps the game and Rich Presence running. It does not retry lobby creation on later world joins; Steam networking and direct-IP connections remain separate. Clients that support the lobby API retain the existing Discord P2P path. The asset-free `discord_lobby_tests` CTest entry covers the downgrade, successful creation, cancelled callbacks and unrelated failure diagnostics.

Keep Steam and the Discord desktop client running for their respective integrations. Standalone Nix client launchers default `SteamAppId` to Starbound's `211820`, without changing the working directory or overriding a nonempty supplied value. A Steam account owning Starbound is required. SDK availability does not imply that Steam Overlay or Discord multiplayer has been verified on every platform.

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

Windows CI publishes separate `OpenStarbound-Windows-Client.zip` and `OpenStarbound-Windows-Server.zip` artifacts, plus the installer. ZIP archives are created with CMake; no separate archiver is required. Client and server packages include their matching `starbound.pdb` and `starbound_server.pdb`; the installer includes both. Keep each PDB beside its executable to enable symbolized Windows stack traces.

### Windows cross-builds

From Linux, enter `nix develop .#full` and set the following **before** running the compiler installer above:

```sh
export OSB_NATIVE_COMPILER_ROOT="$(dirname "$(dirname "$(command -v clang)")")"
```

Then use the `windows-cross` preset with the same vcpkg/bootstrap steps. Tests require Wine 11+ and `-DCMAKE_CROSSCOMPILING_EMULATOR=wine`, or a native Windows runner. macOS cross-builds remain unverified and require an authorized Apple SDK.

## Tests and benchmarks

CTest presets select asset-free core tests. Native builds also register `game_tests` and, with `STAR_BUILD_GUI`, `frontend_tests`:

```sh
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets \
  ctest --test-dir build/linux-release -L '^Assets$' --output-on-failure
```

Use your configured build directory; `-L '^Frontend$'` selects only the frontend suite. Alternatively, configure `-DSTAR_TEST_ASSETS=/absolute/path/to/Starbound/assets`; this takes precedence over the environment. Without either asset setting, these suites are explicitly skipped; an invalid configured directory fails.

Each native asset-backed CTest invocation runs one process per executable with a fresh boot configuration, storage and logs under `<build>/test/runtime/<executable>/<run-id>/`. `results.json` and the runtime are retained for diagnosis. These tests do not read your normal boot configuration or saves. The packaged Nix runner below instead isolates every case when invoked without arguments.

`ImageProcessingTest.*` covers byte-preserving neutral directives, cached color conversions, replacement thresholds and ordered four-channel blends. Asset-backed `DamageDatabaseTest.*`, `HumanoidRenderTest.*` and `ItemTest.BlueprintLearningConstructsOnlyWhenNeededAndPreservesFailureState` cover catalog errors, sleeve lighting and blueprint state/error boundaries.

Core timing tests use a short real-clock smoke and explicit worker synchronization rather than long per-task sleeps. Lua limit tests retain all three execution entry points and post-error recovery at reduced instruction/recursion scales. Image matrices retain byte-exact sequential oracles, with separate value-ownership checks for the List entry point; descriptor parsing formats are checked independently from matching semantics.

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

`json_benchmarks` covers parsing, format-preserving construction, path updates and patches. Reported times include destruction; labels give the batch size, so divide batch CPU times by that count for per-document results. Path/patch cases also include sorted serialization. Use identical fixtures and repeated CPU medians rather than attributing the whole batch to one stage.

## Development

- C++ uses named modules, `import std;` and `std::expected`, without RTTI or exceptions. Use fresh build trees after compiler, ABI or sanitizer changes.
- Configure clangd with `nix develop -c bash scripts/ide/configure-clangd.sh` and its generated compilation database; enable clangd 23's `--experimental-modules-support`.
- Check changed C++/module lines with the pinned formatter; use `--apply` to format or `--base <commit>` for a branch comparison:

```sh
nix shell --inputs-from . nixpkgs#python3 nixpkgs#clang-tools --command scripts/format-source.sh --check
```

### C++ implementation boundaries

- Keep non-template module definitions in implementation units with their own direct imports. Register those units as private CMake sources with `CXX_SCAN_FOR_MODULES` enabled; importing an interface does not expose its implementation dependencies.
- Reuse the checked `Star::transform` for owning transforms. `transformInto` appends and preserves the committed prefix on insertion failure; do not introduce eager capacity errors. Computed-key sorting constructs keys only after successful evaluation.
- Fixed ASCII enum labels use `sortedEnumNames`, `enumName<Names>` and `enumValue<Names>`. Retain canonical output spelling and case-insensitive input; runtime-extensible tables still need `EnumMap`.
- Qt connections use typed signals and an explicit receiver context. Keep `Q_OBJECT` and moc where custom signals or reflection require them, not merely for ordinary receiver methods.

### JSON values and formatting

- `Json` is immutable. Prefer `find`, `arrayPtr` / `objectPtr` or `iterateArray` / `iterateObject` for synchronous reads; `toArray` / `toObject` make mutable container copies. A borrowed child must not outlive its retained owner.
- Contiguous UTF-8 input borrows unescaped tokens during synchronous callbacks; escaped strings and generic iterators retain the decoded path. Unicode validation, parser error order and missing/null distinctions remain unchanged.
- `FormattedJson::create` reuses the immutable semantic value. Use `createArray(span)` for batch output instead of repeatedly appending immutable arrays; child comments and numeric spellings are retained. Its private token index does not change the global JSON hash or wire ordering.
- Path updates still rebuild the immutable ancestor spine. Asset patch groups commit only on success, and search keeps the first match and defers value validation until a match exists.
- Configuration field tables may group only reads with the same type/default policy. Preserve source order, first-error behavior and missing/null distinctions when extracting domain readers.

### Lua embedding

- `LuaConverter<T>` converts directly through `LuaStack` / `LuaStackValue`, including coroutine callbacks. Stack and string views are borrowed; retain values with `own()` or owning Lua handle types.
- Typed calls request zero, one, tuple-N or all results for `void`, scalar, tuple or `LuaVariadic` returns. Unused results are discarded before conversion.
- `LuaBaseComponent` owns session execution, reload and shutdown. Context retirement waits for the outermost call; workers are joined before dependent bindings disappear. Evaluation errors retain the context; invocation/message errors disable it.
- Callback removals requested after a reentrant `uninit` wait until its shutdown hooks return, so those hooks retain their host services.
- One-shot item scripts explicitly settle init/invoke/uninit under the Lua lock before committing item changes. Cleanup errors remain primary with earlier failures as context; retiring a failed context must not resurrect its uninit hook.
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
