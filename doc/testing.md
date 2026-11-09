# Tests and benchmarks

[Project overview](../README.md) · [Build guide](building.md) · [Development guide](development.md)

Run the commands below from the repository root unless noted otherwise.

## Native CTest suites

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

## Nix asset-backed tests

For real-asset tests:

```sh
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets nix run .#game-tests
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets nix run .#asserts-tests
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets OPENSTARBOUND_TEST_JOBS=4 nix run .#sanitized-tests
```

Each case gets a private runtime. `OPENSTARBOUND_TEST_JOBS` defaults to `1`; reports go to `./game-tests-report`, overridden by `OPENSTARBOUND_TEST_OUT`.

Sanitized builds enable mimalloc's `MI_TRACK_ASAN` and `MI_DEBUG`. Small overruns may be detected on free rather than at the write. LSan checks intercepted system allocations, **not individual mimalloc allocations**; passing it does not prove engine/Lua allocations are leak-free.

## Benchmarks

Benchmarks are opt-in:

```sh
nix build .#benchmarks
./result/bin/core_benchmarks --benchmark_repetitions=7 --benchmark_out=core.json --benchmark_out_format=json
# GUI cases: nix build .#benchmarks-gui -o result-benchmarks-gui
```

Native presets need `STAR_BUILD_BENCHMARKS=ON` and `VCPKG_MANIFEST_FEATURES="qt-tools;benchmarks"`; see [benchmark targets](../source/benchmark/CMakeLists.txt). Asset-backed cases need owned assets and a private `-bootconfig`; **never use your saves**. GUI cases require a real display. Compare identical inputs/build flags with repeated samples, not shared-CI timing thresholds.

Use `--benchmark_filter='Lua/'` for invocation, callback, return-arity, context and coroutine cases. Asset workers share one Lua VM; additional workers do not parallelize Lua execution.

`json_benchmarks` covers parsing, format-preserving construction, path updates and patches. Reported times include destruction; labels give the batch size, so divide batch CPU times by that count for per-document results. Path/patch cases also include sorted serialization. Use identical fixtures and repeated CPU medians rather than attributing the whole batch to one stage.

`bytes_benchmarks --benchmark_filter='Bytes/' --benchmark_repetitions=7 --benchmark_out=bytes.json --benchmark_out_format=json` measures scalar integer/float endian conversion (fixed and mixed orders), misaligned raw in-place/copy reversal, and mixed raw conversion. Its 35 cases measure the shared encode/decode implementation once rather than repeating To/From variants.

Each iteration is a 64-operation batch; divide CPU time by 64 for amortized per-operation cost, not single-call latency. Labels contain deterministic input and complete-result fingerprints. Untimed independent byte-oracle checks cover signed zeros, NaN payloads, raw guards and post-loop results. Allocation and validation are untimed; in-place buffers alternate orientation across iterations.

Compare original and current module builds with identical compiler, flags, dependencies, filters and repeated CPU medians. A private baseline benchmark TU may adapt API spellings but must not copy or replace the measured implementation. Match fixture/result fingerprints before comparing timings; raw byte throughput counts payload once, not combined reads and writes.

`line_benchmarks --benchmark_filter='Line/' --benchmark_repetitions=7 --benchmark_out=line.json --benchmark_out_format=json` runs 20 float/double cases for complete and boolean intersection (crossings, endpoint glances, misses, parallel and positively oriented collinear overlap/disjoint lines; finite/infinite modes) and finite/infinite point distance on nonzero lines. Each iteration contains 64 operations and result writes; divide batch CPU time by 64 for amortized per-operation cost. Exact binary-fraction and 3-4-5 fixtures have independent hand-solved full-result checks before and after timing, with fixture/result fingerprints in labels; initialization and validation are untimed. Compare the same benchmark TU against actual original/current modules with identical flags and fingerprints, never a copied legacy algorithm.

`io_benchmarks --benchmark_filter='IO/' --benchmark_repetitions=7 --benchmark_out=io.json --benchmark_out_format=json` covers 24 cases: borrowed string deserialization, 1/5/10-byte VLQ reads through `DataStream&`, fixed-storage UUID decoding, one-shot file writes and complete directory/packed asset payload reads. Owning and function-backed VLQ adapters remain generic-path controls. Decode/VLQ/UUID iterations contain 64 operations; divide CPU time by 64 for amortized per-item cost. File cases contain one operation. Setup and complete byte/value/cursor checks are untimed; labels identify deterministic fixtures. Asset reads are warm payload reads, not metadata queries or whole startup. Writes include open/truncate/write/close but no durability sync. Compare actual original/current production implementations with identical flags and fixtures, pinned CPU and alternating repeated CPU medians; do not infer frame-rate or physical-storage gains.
