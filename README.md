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
- Unix file opens inspect the acquired descriptor and reject directories with a recoverable IO error. A patch replaced by a directory fails asset loading instead of treating a directory seek position as a buffer size; only confirmed vanished patches are skipped.
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

Entity creation encoders transfer completed buffers instead of copying them. Read-only RPC and monster/NPC variant decoders borrow their input only during decoding; decoded fields remain owned. Network formats and compatibility versions are unchanged. Frame data owns reusable entity collections and particle snapshots; painters borrow them only during submission. Typed spatial queries collect a single owned result list without changing traversal order, wrapping, deduplication or insertion-safe candidate snapshots.

Team updates share one configuration snapshot within each update, without caching it across updates. Health, energy, position and travel state retain the configured status frequency. Names and portraits are included on the next eligible status update after ten seconds, or immediately on the next eligible update after first joining, changing teams or `forceUpdate`. Appearance changes can therefore reach other teammates later than dynamic status. The full-update timer advances only after successful local submission; encoding or enqueue failures do not consume it. Legacy RPC still maps remote failures to `null`, so submission is not a remote-delivery acknowledgment.

### Independent simulation and rendering

SDL event polling, window operations and OpenGL stay on the main thread. `ClientLogic` exclusively owns client-world updates, client Lua, UI and CPU draw preparation. Main consumes an owned command frame while the worker can run subsequent fixed ticks; the next draw preparation waits for presentation acknowledgment so ImGui is never mutated concurrently. This is backend/logic parallelism, not concurrent access to the live world or parallel Lua execution. Command capture and handoff add CPU and memory costs; higher uncapped FPS is not guaranteed.

Measure both main-thread and `ClientLogic` CPU time with fixed-rate and uncapped workloads. Use identical-binary A/A controls before attributing small differences to a change: thread affinity does not eliminate clock, cache or shared-system noise. Compare fixed-rate instruction counts at the same simulation/render cadence; uncapped instruction counts per display frame also change with the number of fixed ticks amortized into each frame. Private instrumentation and offscreen swap timings are diagnostic, not evidence of physical-display pacing or native Windows behavior.

`renderFpsLimit` defaults to `0` (unlimited). A finite positive value caps display submissions, including fractional limits such as `117.5`; negative, non-finite and out-of-range values are rejected. The graphics menu stages the FPS limit and VSync until **Apply**. **Back** discards them, and the fullscreen shortcut does not commit pending settings. Existing custom limits are preserved without rounding to a slider preset. VSync can impose a lower effective display rate; unlimited does not override the display driver.

Each scheduler pass performs at most `maxFrameSkip + 1` due simulation ticks. Backward clock changes or more than 250 ms of accumulated debt resynchronize deadlines with one immediately due tick, avoiding unbounded catch-up. While a frame is in flight, the worker waits for logic deadlines independently of presentation; completion also wakes it. Native window/clipboard/SDK operations and ordered renderer queries or configuration changes remain synchronous main-thread barriers and can delay their caller.

World presentation samples the previous and current fixed-tick states, normally adding one simulation tick of visual latency without extrapolating gameplay. Entity positions, supported native animation transforms, particles and the camera share the display sample. Pausing displays the current state. World/player replacement, reloads, native teleports and large corrections invalidate presentation history; authored small teleports can call [`mcontroller.resetPresentation()`](doc/lua/movementcontroller.md). Continuous movement must not reset it each tick.

Timers, one-shot audio/particle collection and beam transitions belong to ticks, including offscreen entities. Resource-cache jitter and native cosmetic noise use a separate presentation random service rather than consuming gameplay randomness. Display sampling does not write interpolated coordinates back to movement controllers. Camera conversions and overlay anchors use the displayed transform; input between frames retains the last displayed camera.

Lua pane `render()` may run several times without an intervening `update(dt)`, or less often than updates. Keep gameplay changes, timers, effect drains and unseeded gameplay-backed random draws out of render hooks. Rebuild ImGui widgets in the render hook only; the engine owns ImGui frame begin/end. See the [script pane lifecycle](doc/lua/openstarbound/scriptpane.md#shared-script-pane-runtime) and [camera API](doc/lua/openstarbound/camera.md). Arbitrary mod render hooks are not made read-only automatically.

Only one sealed command frame is in flight. Command storage owns copied pixels and vertices and uses numeric resource IDs; worker-side intrusive texture references never cross to main. Real graphics resources and platform SDK callbacks remain main-owned, including ImGui's clipboard and IME callbacks. Audio retains its existing callback-thread contract. Shutdown stops audio callbacks and client services first, releases SDK/GL owners while the Root logger is alive, destroys the application on its worker, then joins and tears down ImGui/SDL on main.

Main packs detached geometry directly into owned, typed GPU vertices; legacy primitives use the same packing kernel. Immediate segments remain batched until a real renderer flush, even across ordered queries and transport-slot reuse. Texture pins protect atlas coordinates while batches are pending, and reusable GPU buffers retain their allocated capacity. Command headers are separate from names, shader/configuration payloads and images: creation uploads are released after consumption, while recurring effect-image storage is reused and cleared at configuration reload.

Detached views borrow spans and a required typed reference to main's texture table, not an opaque context or lookup callback; the backend consumes them synchronously without retaining those references. Command builders return checked references. Synchronous main-thread requests remain stack-owned, linked through optional references and executed through a typed interface without queue heap allocations. GPU staging exposes data pointers only at the OpenGL upload boundary.


### Lua 5.5.1 migration

Compound assignment, safe navigation, `math.round` and `math.copysign` remain available.

- `global` is reserved; rename it or use `_ENV["global"]`.
- Numeric loop variables and the first generic-loop variable are read-only. OSB repairs vanilla `/scripts/actions/flying.lua` and `/stagehands/coordinator/npccombat.lua` in memory, preserving their fractional sample bounds; mod scripts still need migration.
- Rebuild Lua 5.3 bytecode. Finalizer errors become warnings; `math.random` sequences may change.
- Text templates use `<name>` and `<<` for a literal `<`, not brace-format placeholders.

### Bundled mod compatibility

`assets/opensb-compat` is a separate, optional-fix asset source, shipped as `opensb-compat.pak` beside `opensb.pak` in distributions and as a sibling directory in Nix packages. Normal asset-directory scanning loads it automatically; custom boot configurations using explicit `assetSources` must include it. It does not require any third-party mod to be installed.

For [StarCore Simplified Chinese](https://steamcommunity.com/sharedfiles/filedetails/?id=807695810) (`SChinese`, version `V108_241106_09`), it removes four unused imports from `/scripts/replaceTags.lua` in memory, breaking `util.lua → replaceTags.lua → util.lua`. The replacement function body, utility functions and translations remain unchanged; the engine still rejects cyclic `require` calls. The repair applies only when the effective script provider is that known SChinese version and its import header matches the [verified upstream release](https://github.com/sffxzzp/Starbound-Chinese/releases/tag/UTC-241105-1953). Other providers, unknown versions and unrecognized headers are left untouched. No installed mod files or saves are rewritten; restart the game after installing the compatibility source.

Additional provider/version/content-guarded repairs cover:

- Arcana 1.4.2.9/1.4.2.10's `dump` helper: use a local display key instead of assigning to Lua 5.5's read-only loop variable; deployment and relic behavior are retained.
- Known translation/UI and weather patches: preserve translated fields and registered patch order while adapting changed schemas. SChinese's complete matter-manipulator translation follows QuickbarMini's active `.original.config` pane; Project45 loot changes follow the relocated treasure pools.
- Neia projectile image fields and registration of its bundled spear-throw ability, Draconis staff emitters and pet-spawner path, PropPack's bundled upper-bed callback and empty drop pool, Halloween's malformed path/art references, and Brewology/docking-bay frame aliases. Existing spear-throw ability mappings supplied by another mod are preserved.
- Titancorp 2.4.4/2.4.5's byte-identical duplicate projectile and powered food-maker integration. Verified object, recipe and implementation fingerprints guard the repair; authored recipes and power behavior are retained. Unknown versions, changed content and foreign providers are not blindly patched.
- The Forge: Blade Dance 0.7.1's five custom-weapon recipes: make their global carrier-image paths absolute without replacing or changing the encoded artwork. Already-saved custom item descriptors are not migrated.
- Advanced Teleporter Interface 1.1.1: retain concise inspection warnings and keep complete caught diagnostics in Debug logs instead of embedding native stacks in every warning.

Rebuild the engine and compatibility source together: source-owned patch/load-script replacement uses the [current Assets APIs](doc/lua/openstarbound/assets.md). These are targeted compatibility fixes, not global suppression of invalid patches or Lua errors.

Voyage addons still require the real [Voyage base mod](https://steamcommunity.com/sharedfiles/filedetails/?id=2957477338); missing items, builders and NPC scripts are not fabricated. Halloween's absent `pumpkinfrag` physics definition is intentionally not guessed, so its two unresolved projectile references remain an upstream defect.

Neia's harpoons also require the real [Elithian Races Mod](https://steamcommunity.com/sharedfiles/filedetails/?id=850109963): their physics preset and return sound are absent from Neia itself. The compatibility source does not redistribute the missing audio or substitute unrelated sounds; installing Elithian also installs its gameplay content.

The compatibility pack does not download or install these dependencies. After installing them, restart and check for asset sources named `MWHArcanaAddon` (Voyage) and `Elithian_Races_Mod` in the log; a translation or addon with a similar name is not the base mod. Alternatively, disable the addons requiring the missing base rather than manufacturing its items or scripts.

### Mod error diagnostics

Lua runtime errors retain their original traceback and append a deduplicated `Lua source providers` summary identifying the scripts on the failed stack, including coroutine failures. Engine-loaded scripts use their actual loaded asset provider: the mod's `friendlyName`, internal `name`, and directory or `.pak` path. Syntax and indexed-script read failures also include provider context. Missing or invalid optional metadata falls back to the source path. Chunk names remain unchanged; provider snapshots are recorded during loading, not by scanning mods every frame.

JSON and image Lua patch errors identify the target asset, patch path, and the exact executing patch provider, even when another mod overrides the same virtual patch path. Asset `onLoad`/`postLoad` failures also include provider metadata. These diagnostics identify code and patch providers, not necessarily the mod that caused a conflict; they do not track configuration changes field by field. Dynamically generated code without a registered asset provider retains its original error information.

Error and warning logs present concise, reason-first summaries: Lua syntax/runtime failures, Lua patch syntax/runtime failures, and resource failures have separate categories, with execution phase, live script locations, and providers where available. Typed causes are retained through native callbacks and ordinary Lua rethrows; repeated wrapper contexts and identical provider labels do not obscure the original reason. Distinct call sites remain visible.

Start the client or server with `-loglevel debug` to include the complete original exception chains, Lua tracebacks and native stacks in Debug records. Full diagnostic rendering is deferred until a Debug sink is enabled; ordinary errors and warnings do not resolve native stacks. `-verbose` only enables stdout logging and does **not** enable Debug; `-quiet` disables stdout, not the configured log file. Final fatal dialogs and stderr summaries stay concise when the owning logger has reported the error. Failures before a logger is available, or while writing the log itself, retain full emergency diagnostics on stderr; bootstrap failures before log settings are applied cannot be controlled by `-loglevel`. Error objects and programmatic full diagnostics are unchanged.

Database per-file validation also adds the concrete asset path and provider where available, including valid JSON with missing or invalid fields. More specific nested asset and patch attribution takes precedence. Recoverable entries still log and skip or use their existing fallback; adding diagnostic context does not make those failures fatal.

Native errors capture stack locations at their origin, but defer symbol resolution until full details are requested. Error copies share immutable diagnostic payloads; short and full renderings are cached independently and support concurrent readers. Captured addresses and module-relative locations remain in full details when symbols or a loaded module are no longer available. Related cleanup failures retain both diagnostics without changing the primary error's classification.

Native callback errors keep their string-valued Lua representation, and built-in `error`/`pcall` behavior is unchanged. Summary transport is identity-bound and bounded; errors deliberately rewritten by a script are treated as new errors unless a complete known payload with a verified live Lua location prefix is preserved. This avoids guessing a mod or reusing a previous failure's attribution.

## Building

Requires **C++23**, **CMake 3.28+**, **Ninja 1.11+** and [xclang](https://github.com/clice-io/xclang/releases/tag/23.1.3.6) **23.1.3.6** (LLVM **23.1.3**) with bundled libc++/`std` modules. Dependencies must use the same C++ runtime; Windows uses MinGW. GCC, Apple Clang and MSVC builds are unsupported.

All configurations use static mimalloc with `MI_OVERRIDE=OFF`; dependencies must not mix allocator heaps.

Lua is fetched with a [version/hash lock](cmake/dependencies/lua.json) and [project patch](cmake/patches/lua-openstarbound.patch); Git is required unless using Nix or prepatched sources.

Thread creation and joining retain a narrow native backend returning `std::expected`: project code disables exceptions, while `std::thread`/`std::jthread` report creation failures by throwing. Mutexes, condition variables and atomic waits use the standard library. The writer-preferring `ReadersWriterMutex` exposes standard locking operations for `std::shared_lock` and `std::unique_lock`; its multiple shared acquisitions by one thread and queued-writer priority are not replaced by `std::shared_mutex`.

Worker cancellation uses `std::stop_source`/`std::stop_token`, with stable tokens for each run and a new cancellation domain only after previous workers have joined. Terminal owners retain one lifetime domain. Stop requests still require explicit condition-variable notification or native I/O shutdown before joining, and derived owners join before destroying borrowed state. `Socket::close()` shuts down under shared ownership before waiting for exclusive descriptor release, so blocking send/receive cannot prevent their own shutdown. Worker pools retain checked startup rollback, queued-work cancellation versus draining, stored-lvalue task invocation and checked result publication.

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

Rich Presence does not require a fetched Discord user, so presence-only RPC clients can publish status independently of lobby support. If the Discord client rejects `CREATE_LOBBY` as an unsupported command, OpenStarbound disables Discord lobby networking for that session, reports the capability loss, and keeps the game and Rich Presence running. It does not retry lobby creation on later world joins; Steam networking and direct-IP connections remain separate. Clients that support the lobby API retain the existing Discord P2P path.

An SDK `UpdateActivity` failure is a nonfatal warning, not an application shutdown. The SDK may still log its original error. An unchanged failed activity is not retried every frame; later changed activities can publish normally. Lobby/network and diagnostic-logging failures retain their checked error boundaries. The asset-free `discord_lobby_tests` CTest entry covers activity failure and subsequent publication, unsupported lobbies, successful creation, cancelled callbacks and unrelated failure diagnostics.

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

Core timing tests use a short real-clock smoke and explicit worker synchronization rather than long per-task sleeps. Lua limit tests retain all three execution entry points and post-error recovery at reduced instruction/recursion scales. Exception tests retain synchronized cold-cache contention while limiting repeated hot-cache reads; FlatHash randomized tests use bounded samples and retain insert, erase, reinsert and lifetime checks.

Threading regressions cover queued-task cancellation, restart/reconfiguration with fresh workers, script-worker restart and duplicate start, writer priority, and closing a real socket during blocking receive. Native readiness wakeups and buffered-packet delivery after close retain their separate coverage.

`fixed_scheduler_tests` exercises deadline pacing, bounded catch-up, clock discontinuities, invalid rates and repeated slow presents without game assets. Asset-backed presentation regressions cover wrapped camera sampling, repeatable entity/animation sampling, tick-owned spawn/removal effects and beam transitions, and cold/warm visual-cache isolation from gameplay randomness.

Image kernels retain the full format/scaler matrix and byte-exact sequential oracles. Pipeline entry points use representative format/scaler combinations, separate ownership checks and ordered failure-prefix assertions rather than repeating the kernel matrix. Ordinary consumers check error types and state/recovery boundaries; exact diagnostic comparisons remain where wording or first-error selection is the contract.

Frontend numeric boundary matrices run against fresh real panes without reloading all assets for each row; dedicated reload scenarios retain live-state continuity coverage. Spawn tests advance the real client until world sectors and observed entity/world-script schedules are ready, then acknowledge a post-update world message and validate rendered images. Readiness has a bounded timeout instead of a fixed frame count; fixed-world coordinator script compilation remains explicit even when no coordinator entity is spawned.

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
- Payload-free alternatives use `std::monostate`. Preserve JSON/Lua nil, variant ordering, and network/inventory marker tags and zero-byte payloads; named inventory wrappers still distinguish swap and trash slots.
- Use `auto(expr)` for owning traversal snapshots and `std::views::zip` for synchronous read-only iteration over retained ranges. Pane dismissal callbacks may modify the original collection, so these snapshots must not become borrowed views.
- Map/set snapshot accessors reserve the known size and use one `std::ranges::copy` traversal. Avoid blindly using `std::ranges::to<List>`: the iterator-pair constructor can add a distance traversal even for a sized range.
- Use `std::exchange` for scalar flags and audited empty collection/optional resets. Keep `Star::take` when replacement construction is observable: `SkyParameters` must move its old state before allocating its default JSON object.
- Text escape sets are bounded `constexpr std::string_view` values backed by static byte storage, not null-terminated strings. Preserve their explicit lengths when passing them to search or native APIs.
- Typed sequence `contains` converts its argument to the element type before `std::ranges::contains`; replacing the member call with an untyped range query can change mixed-type membership.
- Sequence `indexOf` retains element-type conversion and returns `NPos` for out-of-range starts before forming an iterator. Keep `lastIndexOf`'s exclusive prefix bound and original comparison order.
- Non-floating `Array` comparisons use standard algorithms with explicit legacy predicates: equality needs only contextual `!=`, and weak lexicographic ordering needs only `<`. Floating arrays retain their original loops: production fast-math changed infinity/NaN ordering through standard lexicographic dispatch in a paired-module probe.
- Use `std::clamp` only with proven ordered bounds and value consumption. Legacy `Star::clamp` deliberately returns the upper bound for reversed ranges. Preserve vector interpolation, integer rounding, wrapped coordinates and fast-math-safe finite classification rather than replacing their arithmetic wholesale.

### JSON values and formatting

- `Json` is immutable. Prefer `find`, `arrayPtr` / `objectPtr` or `iterateArray` / `iterateObject` for synchronous reads; `toArray` / `toObject` make mutable container copies. A borrowed child must not outlive its retained owner.
- Borrow read-only generation arrays without changing random draw order. Explicit monster part overrides still consume their normal random part draw; NPC equipment retains its object snapshot and slot iteration order. Rebuilding a hash table can change traversal order, so object copies cannot always be replaced by borrowed iteration.
- Reuse `jsonToList` for ordered item conversion. Preserve each field's missing/null default separately; a present `example: null` is not an absent example.
- Contiguous UTF-8 input borrows unescaped tokens during synchronous callbacks; escaped strings and generic iterators retain the decoded path. Unicode validation, parser error order and missing/null distinctions remain unchanged.
- `FormattedJson::create` reuses the immutable semantic value. Use `createArray(span)` for batch output instead of repeatedly appending immutable arrays; child comments and numeric spellings are retained. Its private token index does not change the global JSON hash or wire ordering.
- Path updates still rebuild the immutable ancestor spine. Asset patch groups commit only on success, and search keeps the first match and defers value validation until a match exists.
- Configuration field tables may group only reads with the same type/default policy. Preserve source order, first-error behavior and missing/null distinctions when extracting domain readers.

### UI and rendering preparation

- Options callbacks share volume and boolean writers, retaining local changes before live configuration writes and label updates. Capture configuration keys once when registering callbacks; head rotation also updates the running Root cache.
- Text measurement and wrapping share only the `font=`, `set` and `reset` command subset. Full rendering commands also change colors, shadows and other saved state.
- Entity highlights draw all underlays before main drawables and overlays. Keep the original drawables available for overlays; offscreen texture-preload jitter must use presentation randomness, not the gameplay random stream.
- Horizon rendering queues every layer's textures before testing readiness. Load both halves before submitting either; shared geometry preparation retains rotation, vertex/UV order and floating-point operation order.
- Capture one `PresentationContext` per display frame and pass it through world geometry, lighting callbacks and overlays. Its interpolation coefficient and scheduler epoch are shared frame state, not a simulation clock; an entity discontinuity selects the current pose without changing the frame's epoch.
- `NetworkedAnimator::sample(alpha)` returns an explicit view. Query that view, not the logical animator, for sampled geometry, points and lights; pass it to nested pose work. Each root view borrows an independent reusable scratch slot, so different-alpha views can coexist and be queried in either order. Copies share that view's overrides; the slot is reusable only after its last view is destroyed. Logical updates/configuration changes are the synchronization boundary, and the animator must outlive every view.
- Sample-local state, tags, transforms and borrowed item attachments never modify logical animation or NetElements. `Humanoid::prepareAnimatorPose()` prepares the logical tick pose; its explicit-view overload prepares display-only pose data. Do not retain views or attachment borrows across simulation updates, equipment changes or reloads. The scratch pool gains slots only when the maximum simultaneous view count grows; rendering does not clone the animator or hand Drawable collections.
- Sample scratch slots reuse discrete state/part property composition only while the immutable configuration, selected state and current/next frames match. Timing and affine interpolation are still evaluated for every display sample. Exposing mutable `parts()` disables cross-sample part-property reuse, including after assignment, because callers may retain and later edit that reference; use `constParts()` for read-only inspection. Invalid frame data remains lazily validated and failed queries remain retryable.
- Successful part and anchor transformations are reused within a view by geometry, points, polygons and lights. Local state or transformation-group edits invalidate those results for all copies of that view, without affecting independent views. Acquiring a reused scratch slot clears its transform results; global zoom and flip are applied outside the local-transform cache.
- `WorldClient` owns the final live-event drain before entity destruction. `destroy()` contributes only newly produced death effects, rather than recollecting already drained events. `EffectSource::finishEventCollection()` acknowledges the tick's effect stages independently of rendering; no second event bus is involved.

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
