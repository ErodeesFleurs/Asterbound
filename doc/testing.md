# Tests and benchmarks

[Project overview](../README.md) · [Build guide](building.md) · [Development guide](development.md) · [Engine notes](engine-notes.md)

Run commands from the repository root. Build configuration and dependencies are
covered in the [build guide](building.md); this page describes test selection,
runtime isolation and how to interpret performance measurements.

- [Native CTest suites](#native-ctest-suites)
- [Nix asset-backed tests](#nix-asset-backed-tests)
- [Regression coverage and test design](#regression-coverage-and-test-design)
- [Benchmarks](#benchmarks)
- [Runtime performance evidence](#runtime-performance-evidence)

## Native CTest suites

Use your configured build directory in place of `build/linux-release`.
The [native test presets](../source/CMakePresets.json) select asset-free tests:

```sh
ctest --test-dir build/linux-release -L '^NoAssets$' --output-on-failure
```

Native builds also register `game_tests` and, with `STAR_BUILD_GUI`,
`frontend_tests`. Run both asset-backed suites with owned Starbound assets:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets \
  ctest --test-dir build/linux-release -L '^Assets$' --output-on-failure
```

Use `-L '^Frontend$'` to select only the frontend suite. Keep the anchors in
`'^Assets$'`: an unanchored `Assets` also matches the `NoAssets` label.
Alternatively, configure `-DSTAR_TEST_ASSETS=/absolute/path/to/Starbound/assets`;
this takes precedence over the environment. Without either asset setting, the
asset-backed suites are explicitly skipped; an invalid configured directory fails.

Each native asset-backed invocation runs one process per executable with a fresh
boot configuration, storage and logs under
`<build>/test/runtime/<executable>/<run-id>/`. `results.json` and the runtime are
retained for diagnosis. These tests do not read your normal boot configuration or
saves. See the [native asset runner](../source/test/run_asset_tests.cmake) for the
setup and [test registrations](../source/test/CMakeLists.txt) for suite labels.

Each asset-free CTest preset limits an entire test executable to 900 seconds, not
each GoogleTest case. Windows CI runs CTest verbosely so individual cases remain
visible while the process is running, writes
`build/windows-release/test-results.xml`, and uploads that report plus
`Testing/Temporary/` as `Windows-Test-Failure-Logs` on failure.

### Graphical client startup smoke

Windows, Linux and macOS CI run the actual graphical client's no-assets
`-version` path with redirected output, either output absent, and both absent.
Windows uses detached native process creation with explicit NULL handles.
Run the same entry/bootstrap check manually:

```sh
python3 scripts/ci/smoke-client-startup.py dist/asterbound
```

On Windows, use `python` and `dist/asterbound.exe`. This checks bootstrap behavior,
not asset loading, a rendered game session or cross-platform gameplay compatibility.

## Nix asset-backed tests

These apps run packaged `game_tests`, not `frontend_tests`:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets nix run .#game-tests
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets nix run .#asserts-tests
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets ASTERBOUND_TEST_JOBS=4 nix run .#sanitized-tests
```

With no command-line arguments, the [packaged runner](../nix/run-game-tests.py)
discovers cases and gives each case a separate process and private runtime.
Arguments after `--` instead select its single-process, streamed-output interface.
To filter while keeping per-case isolation, use the environment:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets \
  GTEST_FILTER='DamageDatabaseTest.*' nix run .#game-tests
```

| Setting | Meaning |
|---|---|
| `ASTERBOUND_TEST_JOBS` | Concurrent cases in the no-argument sweep; defaults to `1`. Increase only with sufficient memory. |
| `ASTERBOUND_TEST_OUT` | Report/runtime directory; defaults to `./game-tests-report`. Use separate directories for retained runs. |
| `ASTERBOUND_TEST_TIMEOUT` | Process timeout; defaults to 300 seconds for the normal runner and 900 for assertion/sanitized apps. |

Do not compare per-case sweep duration with a single-process suite as evidence of
an engine speedup: they repeat asset initialization differently.

Sanitized builds enable mimalloc's `MI_TRACK_ASAN` and `MI_DEBUG`. Small overruns
may be detected on free rather than at the write. LSan checks intercepted system
allocations, **not individual mimalloc allocations**; passing it does not prove
engine/Lua allocations are leak-free.

## Regression coverage and test design

This section records coverage boundaries to preserve when changing or simplifying
tests; it is not a claim that these checks were run for a particular build.

### Native output and fatal errors

Windows engine fatal errors write their emergency diagnostic and abort without
opening an application error dialog, so command-line tools and death-test children
do not wait for interactive acknowledgement.
`ByteArrayTest.OversizedGrowthTerminatesBeforeWriting` retains both
oversized-capacity and write-offset termination checks;
`MemoryTest.ImpossibleThrowingAllocationsTerminate` separately covers allocator
termination.

`NativeRuntimeStdioDeathTest.*` isolates stream changes in child processes:

- Absent outputs, preserved file/pipe bytes and file logging without a console.
- Required CLI output failures, configured read-only/full-device errors and
  emergency-reporting continuation.
- On Windows, CRT redirection with an empty Win32 handle slot and recovery under
  the default CRT invalid-parameter policy.
- On POSIX, closed stdin as well as outputs to catch accidental descriptor reuse.

Absent-output recovery sets the pre-existing `FILE` error using raw
`fwrite`/`fflush`, without constructing a project exception before repair.
Exception stack capture can open and cache object files (notably Mach-O files on
macOS), reuse closed standard descriptors and invalidate that precondition.
Required-output and configured-error cases separately exercise checked project
errors. See [native process output rules](development.md#logging-and-native-process-output).

### Core correctness and concurrency

- Timing tests use a short real-clock smoke and explicit worker synchronization,
  not long per-task sleeps.
- Lua limit tests retain all three execution entry points and post-error recovery
  at reduced instruction/recursion scales.
- Exception tests retain synchronized cold-cache contention while limiting
  repeated hot-cache reads. FlatHash randomized tests use bounded samples and
  retain insert, erase, reinsert and lifetime checks.
- Threading regressions cover queued-task cancellation, restart/reconfiguration
  with fresh workers, script-worker restart and duplicate start, writer priority,
  and closing a real socket during blocking receive. Native readiness wakeups and
  buffered-packet delivery after close retain separate coverage.
- `fixed_scheduler_tests` covers deadline pacing, bounded catch-up, clock
  discontinuities, invalid rates and repeated slow presents without game assets.

### Images, gameplay and frontend lifecycles

- `ImageProcessingTest.*` covers byte-preserving neutral directives, cached color
  conversions, replacement thresholds and ordered four-channel blends. Kernels
  retain the full format/scaler matrix and byte-exact sequential oracles;
  pipeline entry points use representative combinations, separate ownership
  checks and ordered failure-prefix assertions rather than repeating that matrix.
- `ImageStorageTest.*` covers checked layouts and wrapped rectangle endpoints,
  clipped raw-channel copies/conversions, aliased forward feedback, float bit
  preservation, callback failure commits, small windows of oversized conceptual
  scales, border overflow and three-channel BGR PNG output.
- Asset-backed `DamageDatabaseTest.*`, `HumanoidRenderTest.*` and
  `ItemTest.BlueprintLearningConstructsOnlyWhenNeededAndPreservesFailureState`
  cover catalog errors, sleeve lighting and blueprint state/error boundaries.
- Presentation regressions cover wrapped camera sampling, repeatable
  entity/animation sampling, tick-owned spawn/removal effects and beam
  transitions, and cold/warm visual-cache isolation from gameplay randomness.
- Frontend numeric matrices use fresh real panes without reloading all assets for
  each row; dedicated reload scenarios retain live-state continuity coverage.
- Spawn tests advance the real client until world sectors and observed
  entity/world-script schedules are ready, acknowledge a post-update world
  message, then validate rendered images. Readiness has a bounded timeout rather
  than a fixed frame count. Fixed-world coordinator script compilation stays
  explicit even when no coordinator entity is spawned.

Ordinary consumers check error types and state/recovery boundaries. Exact
diagnostic comparisons remain where wording or first-error selection is the
contract.

## Benchmarks

### Build and run

Benchmarks are opt-in. The headless Nix package includes the executables in the
catalog below:

```sh
nix build .#benchmarks
./result/bin/core_benchmarks --benchmark_repetitions=7 --benchmark_out=core.json --benchmark_out_format=json
```

For GUI targets, build a separate result link:

```sh
nix build .#benchmarks-gui -o result-benchmarks-gui
```

Native presets need `STAR_BUILD_BENCHMARKS=ON` and
`VCPKG_MANIFEST_FEATURES="qt-tools;benchmarks"`; see the
[build guide](building.md) and [benchmark targets](../source/benchmark/CMakeLists.txt).
Native executables are in `dist/`; substitute that path for `./result/bin/` below.
Asset-backed cases need owned assets and an explicit private
`-bootconfig /absolute/path/to/sbinit.config`, with separate storage and logs;
**never use your saves**. See [runtime path configuration](building.md#runtime-paths-and-data-import).
GUI cases require a real display.

### Catalog and filters

Append `--benchmark_repetitions=7 --benchmark_out=<name>.json
--benchmark_out_format=json` to a filtered command when recording results.
The target list above also includes benchmarks outside this focused catalog.

| Area | Filtered command | Measured work |
|---|---|---|
| Lua | `./result/bin/core_benchmarks --benchmark_filter='Lua/'` | Invocation, callbacks, return arity, contexts and coroutines. Asset workers share one Lua VM; extra workers do not parallelize Lua execution. |
| JSON | `./result/bin/json_benchmarks` | Parsing, format-preserving construction, path updates and patches. |
| Byte order | `./result/bin/bytes_benchmarks --benchmark_filter='Bytes/'` | 35 scalar integer/float fixed/mixed-order and misaligned raw in-place/copy reversal/conversion cases. The shared encode/decode implementation is measured once, not duplicated as To/From variants. |
| Geometry | `./result/bin/line_benchmarks --benchmark_filter='Line/'` | 20 float/double complete/boolean intersection and point-distance cases. |
| I/O | `./result/bin/io_benchmarks --benchmark_filter='IO/'` | 24 cases: borrowed string deserialization, 1/5/10-byte VLQ reads through `DataStream&`, fixed-storage UUID decoding, one-shot file writes and complete directory/packed asset payload reads. Owning/function-backed VLQ adapters are generic-path controls. |
| Logging | `./result/bin/logging_benchmarks --benchmark_filter='Logging/'` | Eleven cases: rejected/accepted raw/formatted messages, one/four sinks, four persistent native callers, real file lines and unobserved/observed/capacity-bound spatial logs. |
| Flat hash storage | `./result/bin/flat_hash_benchmarks --benchmark_filter='FlatHash/'` | 28 cases: new `try_emplace`/const-pair insertion, expensive duplicate map/set insertion, full-hash-collision rehash, hit/miss lookup, inline-value churn, retained-capacity clear/reuse and sparse iteration. |
| Images | `./result/bin/image_benchmarks --benchmark_filter='Image/'` | 52 sprite/large cases: ownership, fill, clipped paste/source-over, crops, channel widths, full/windowed scalers, partial-alpha borders, aligned/offset/aliased masks and blends, color sequences, small directive outputs, traversal and libpng encode/decode. |
| LRU/TTL caches | `./result/bin/lru_benchmarks --benchmark_filter='^(Lru|Ttl)/'` | 52 cases: ordered/hash LRU hit, const peek, full miss, duplicate update, insert-then-trim and capacity shrink at 64/256/1000/20000 entries; hash TTL refresh on/off and keep/drop cleanup at 256 entries. |

### Batch accounting and correctness oracles

Reported batch CPU time is **not single-call latency**. Divide by the documented
batch count for amortized per-item cost:

| Target | Batch and interpretation |
|---|---|
| `json_benchmarks` | Labels give the document count. Times include destruction; path/patch cases also include sorted serialization. Do not attribute the whole batch to one stage. |
| `bytes_benchmarks` | 64 operations. Raw throughput counts payload once, not combined reads and writes. |
| `line_benchmarks` | 64 operations and result writes. |
| `io_benchmarks` | 64 decode/VLQ/UUID operations; one operation per file iteration. |
| `logging_benchmarks` | Labels specify 64 messages, 256 concurrent messages or 192 mixed spatial objects. Concurrent timing uses process CPU time and includes wakeup/completion. |
| `flat_hash_benchmarks` | Labels give N entries/queries; churn counts 2N erase/insert operations and rehash counts N relocated residents. Divide batch CPU time accordingly. |
| `image_benchmarks` | One operation/sequence per iteration; labels state timed ownership and retained reset work. Workload throughput counts processed pixels; the constant-foldable RGB alpha control counts reductions, not memory traffic or frames. |
| `lru_benchmarks` | N operations/entries per sweep; `ShrinkHalf` counts N/2 evicted entries. Shrink/drop reset is untimed. String keys and shared immutable payloads are prebuilt; payload construction, FPS and single-call latency are not measured. |

Fixture setup and complete-result validation are untimed for byte, line, I/O,
logging, flat hash, image and LRU/TTL benchmarks. Preserve their independent checks and labels:

- **Bytes:** deterministic input and complete-result fingerprints; independent
  byte oracles check signed zeros, NaN payloads, raw guards and post-loop results.
  Allocation is untimed; in-place buffers alternate orientation across iterations.
- **Lines:** exact binary-fraction and 3-4-5 fixtures have hand-solved full-result
  checks before and after timing, with fixture/result fingerprints. Cases include
  crossings, endpoint glances, misses, parallel and positively oriented collinear
  overlap/disjoint lines in finite/infinite modes, plus finite/infinite point
  distance on nonzero lines.
- **I/O:** complete byte/value/cursor checks and deterministic fixture labels.
  Asset reads are warm payload reads, not metadata queries or whole startup.
  Writes include open/truncate/write/close, without durability sync.
- **Logging:** complete output checks; file lines are warm writes, without
  durability sync.
- **Flat hash storage:** an independent ordered oracle checks complete key/value membership and duplicate iteration before/after timing, including embedded-NUL/UTF-8 keys. New-insertion cases include owner creation/destruction; duplicate, lookup, churn, clear/reuse and sparse iteration keep owners outside timing. Collision rehash pauses copy/setup and includes only reserve/rebuild. Full-hash collisions are a stress case, not representative default-hash throughput.
- **Images:** complete shape/format/raw-byte fingerprints before and after timing, with independent raw crop/copy/conversion/compositing oracles and robust hand-solved sampling/reference cases. Owning outputs include allocation/release; retained targets include the labeled reset work. PNG cases include real libpng and Buffer ownership/cursors, not a copied codec.
  The RGB alpha-only reader is an explicitly constant-foldable control: its
  implicit alpha is always 255, so report reductions, not memory throughput or
  per-pixel traversal gains. The frozen comparison consumer retains its original
  labels/counters; normalize that control as one reduction per iteration.
- **LRU/TTL caches:** validate complete ordered key snapshots and every payload field before/after timing, with content fingerprints and exact producer counts. Full-miss and insert/trim cases alternate disjoint windows, then normalize the untimed final state so labels do not depend on calibration parity. Preserve soft capacity and the production allocator. TTL uses seeded randomness, future-deadline hits and already-expired keep/drop cleanup; refresh/cleanup mutation is escaped through a compiler barrier. Run the same frozen consumer against isolated old/new modules, require matching labels on every row, and retain A/A drift and regressions.

### Comparable measurements

Use identical inputs, compiler, flags (including ThinLTO), dependencies and filters,
with repeated CPU medians rather than shared-CI timing thresholds. Compare actual
original/current production modules and match available fixture/result
fingerprints before interpreting timings. Use the same benchmark translation
unit; a private baseline adapter may change API spellings, but must not copy or
replace the measured implementation or substitute a copied legacy algorithm.

For I/O, pin the CPU and alternate repeated original/current runs. Do not infer
frame-rate or physical-storage gains from warm payload benchmarks. For logging,
compare seven alternating original/current runs, pinning single-caller cases to
one CPU and concurrent callers to a fixed four-CPU set.
For flat hash storage, freeze the original table/map/set sources and build private
original/current consumers from the same benchmark translation unit, changing
only baseline module/namespace references. Pin one CPU, alternate seven
original/current runs, retain raw JSON and compare per-item CPU medians. Include
identical-binary A/A controls before attributing small changes; copy/move,
allocation and hash/equality counters explain work but do not establish timing
gains or frame-rate improvements.
For images, freeze the original production sources, BMIs and linked executable
before rebuilding. Build the same benchmark consumer against original/current
modules, adapting only baseline C++ API spellings. Match every fixture/result
label before timing; also compare full serialized image outputs across formats,
scales and aliasing cases and exercise actual Assets, metadata and Lua bindings
with private runtime storage. Pin one CPU, alternate seven original/current runs
and retain identical-binary A/A controls. Report regressions and allocation-byte
counts separately; pixel throughput does not establish frame-rate gains.

## Runtime performance evidence

The [simulation/rendering architecture](engine-notes.md#independent-simulation-and-rendering)
can overlap backend work and logic, but command capture/handoff also costs CPU and
memory. Higher uncapped FPS is not guaranteed.

Measure both main-thread and `ClientLogic` CPU time with fixed-rate and uncapped
workloads. Use identical-binary A/A controls before attributing small differences
to a change: affinity does not eliminate clock, cache or shared-system noise.
Compare fixed-rate instruction counts at the same simulation/render cadence;
uncapped instructions per display frame also vary with the fixed ticks amortized
into each frame.

Private instrumentation and offscreen swap timings are diagnostic, not evidence
of physical-display pacing or native Windows behavior. A startup smoke, same-build
unit tests and Linux measurements do not establish native GUI behavior on other
platforms or interoperability with historical/future protocol versions.
