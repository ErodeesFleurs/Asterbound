# Tests and benchmarks

[Project overview](../README.md) · [Build guide](building.md) · [Development guide](development.md) · [Engine notes](engine-notes.md)

Run commands from the repository root. The [build guide](building.md) covers
configuration and dependencies; this page covers selection, isolation and evidence.

- [Native CTest suites](#native-ctest-suites)
- [Nix asset-backed tests](#nix-asset-backed-tests)
- [Regression coverage and test design](#regression-coverage-and-test-design)
- [Benchmarks](#benchmarks)
- [Runtime performance evidence](#runtime-performance-evidence)

## Native CTest suites

Substitute your configured build directory for `build/linux-release`.
The [native presets](../source/CMakePresets.json) select asset-free tests:

```sh
ctest --test-dir build/linux-release -L '^NoAssets$' --output-on-failure
```

Run `game_tests` and, with `STAR_BUILD_GUI`, `frontend_tests` using owned assets:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets \
  ctest --test-dir build/linux-release -L '^Assets$' --output-on-failure
```

Use `-L '^Frontend$'` for frontend only. Keep label anchors: unanchored `Assets`
also matches `NoAssets`. Configuring
`-DSTAR_TEST_ASSETS=/absolute/path/to/Starbound/assets` overrides the environment.
Without either setting, asset-backed tests are explicitly **skipped**, not passed;
an invalid configured directory fails.

The [native asset runner](../source/test/run_asset_tests.cmake) uses one process
per executable, with private boot configuration, storage and logs under
`<build>/test/runtime/<executable>/<run-id>/`. It retains the runtime and
`results.json`, includes vanilla assets plus the three bundled sources
(`assets/asterbound`, `assets/asterbound-compat`, `assets/asterbound-legacy`), and
never reads normal boot configuration or saves. See
[test registrations](../source/test/CMakeLists.txt) for labels.

Asset-free CTest presets allow **900 seconds per whole executable**, not per
GoogleTest case. Windows CI runs verbose CTest, writes
`build/windows-release/test-results.xml`, and uploads it with `Testing/Temporary/`
as `Windows-Test-Failure-Logs` on failure.

### Graphical client startup smoke

Windows, Linux and macOS CI exercise the actual graphical client's no-assets
`-version` path with redirected output, either output absent, and both absent.
Windows uses detached native process creation with explicit NULL handles.
To run the same bootstrap check:

```sh
python3 scripts/ci/smoke-client-startup.py dist/asterbound
```

On Windows use `python` and `dist/asterbound.exe`. This is not asset-loading,
rendered-session or cross-platform gameplay evidence.

## Nix asset-backed tests

These apps package `game_tests`, not `frontend_tests`:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets nix run .#game-tests
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets nix run .#asserts-tests
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets ASTERBOUND_TEST_JOBS=4 nix run .#sanitized-tests
```

With no arguments, the [runner](../nix/run-game-tests.py) discovers cases and runs
each in a separate process/private runtime. Arguments after `--` instead select a
single process with streamed output. To filter without losing per-case isolation:

```sh
ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets \
  GTEST_FILTER='DamageDatabaseTest.*' nix run .#game-tests
```

| Setting | Meaning |
|---|---|
| `ASTERBOUND_TEST_JOBS` | Concurrent cases in the no-argument sweep; default `1`. Increase only with sufficient memory. |
| `ASTERBOUND_TEST_OUT` | Report/runtime directory; default `./game-tests-report`. Separate retained runs. |
| `ASTERBOUND_TEST_TIMEOUT` | Process timeout; default 300 seconds, or 900 for assertion/sanitized apps. |

Sweep and single-process durations repeat asset initialization differently; they
are not comparable engine-speed measurements. Sanitized builds enable mimalloc
`MI_TRACK_ASAN` and `MI_DEBUG`: small overruns may be detected on free, not at the
write. LSan checks intercepted system allocations, **not individual mimalloc
allocations**, so a pass does not prove engine/Lua allocations leak-free.

## Regression coverage and test design

These are coverage boundaries to preserve, not claims that a build was tested.
Prefer bounded representative cases and independent complete-result oracles over
repeated matrices. Check error types and state/recovery; compare exact diagnostics
only when wording or first-error selection is the contract.

`star_test_gtest` compiles the shared GoogleTest implementation once. Test mains,
Root providers, syscall wrappers and SDK configuration stay executable-specific.
Reuse fixtures/assertion sequences without collapsing distinct failures,
single-pass inputs, recovery paths or lifecycle transitions.

### Native output and fatal errors

Preserve noninteractive fatal termination, oversized growth/write-offset and
impossible-allocation checks. `NativeRuntimeStdioDeathTest.*` isolates stream
changes in children: absent outputs, preserved file/pipe bytes and file logging,
required-output/configured-device failures, Windows CRT/Win32 handle mismatch
under the default invalid-parameter policy, and closed POSIX stdin/outputs.

Absent-output recovery must set the existing `FILE` error with raw
`fwrite`/`fflush` before repair. Constructing a project exception first can cache
object files and reuse closed descriptors, invalidating the precondition.
See [native output rules](development.md#logging-and-native-process-output).

### Core correctness and concurrency

- Use short real-clock smoke tests and explicit synchronization, not long sleeps.
  Retain cold-cache contention, Lua's three entry points and post-error recovery,
  and bounded randomized insert/erase/reinsert/lifetime checks.
- Keep cancellation, fresh-worker restart/reconfiguration, duplicate start,
  writer priority and real blocking-socket close coverage. Readiness wakeups and
  buffered delivery after close are distinct. `fixed_scheduler_tests` covers
  pacing, catch-up, clock discontinuities, invalid rates and slow presents without
  assets.
- Exercise failed assets through cache clearing, unrelated queue draining and
  repaired hot reload. Collection repairs must preserve unknown providers,
  versions and bytes, reject partial matches, and cover CRLF, patch order and
  failure recovery through the real loader.

### Images, gameplay and frontend lifecycles

- Image kernels retain full format/scaler coverage, byte-exact independent
  oracles, aliasing/ownership, checked layout and overflow, callback failure
  commits and PNG channel order. Pipelines use representative combinations and
  ordered failure prefixes instead of repeating every kernel matrix.
  `ImageScalingTest.*` uses narrowly scoped precise-FP scalar references;
  volatile loads alone do not prevent fast-math sampling changes. Keep independent
  nearest/directive checks on both sides of half-integer boundaries.
- Asset-backed damage, humanoid rendering and blueprint tests preserve catalog,
  lighting and state/error boundaries. Presentation checks retain wrapped and
  repeatable sampling, tick-owned effects and gameplay-RNG isolation. Frontend
  matrices use fresh real panes; dedicated reload cases check live continuity.
- Spawn checks advance the real client until sectors and observed entity/script
  schedules are ready, acknowledge a post-update world message, then inspect
  rendered images. Use bounded readiness timeouts, not fixed frame counts; keep
  fixed-world coordinator script compilation explicit even without its entity.

## Benchmarks

### Build and run

Build the headless executables, or use a separate result link for GUI targets:

```sh
nix build .#benchmarks
./result/bin/core_benchmarks --benchmark_repetitions=7 --benchmark_out=core.json --benchmark_out_format=json
nix build .#benchmarks-gui -o result-benchmarks-gui
```

Native builds require `STAR_BUILD_BENCHMARKS=ON` and
`VCPKG_MANIFEST_FEATURES="qt-tools;benchmarks"`; executables land in `dist/`
instead of `./result/bin/`. See the [build guide](building.md) and
[benchmark targets](../source/benchmark/CMakeLists.txt).
Asset-backed cases require owned assets and explicit private
`-bootconfig /absolute/path/to/sbinit.config`, with separate storage/logs:
**never use your saves**. See [runtime paths](building.md#runtime-paths-and-data-import).
GUI cases require a real display.
Executable targets remain separate; `star.benchmark_runner` shares CLI/reporting
and benchmark lifetime management, not Root/GUI ownership or workload timing.

### Catalog and filters

For recorded runs, append `--benchmark_repetitions=7 --benchmark_out=<name>.json
--benchmark_out_format=json`. The target list also includes cases outside this
catalog; implementation and detailed labels live in [benchmark sources](../source/benchmark/).

| Area | Filtered command | Work |
|---|---|---|
| Lua | `./result/bin/core_benchmarks --benchmark_filter='Lua/'` | Calls, callbacks, contexts and coroutines; asset workers share one VM. |
| JSON | `./result/bin/json_benchmarks` | Parsing, preserved formatting, path updates and patches. |
| Algorithms | `./result/bin/algorithm_benchmarks --benchmark_filter='Algorithm/'` | Ordinary/checked cached-key and stable sorting; held-key filtering. |
| Optional values | `./result/bin/optional_benchmarks --benchmark_filter='Optional/'` | Owning extraction, borrowing, traced empty errors and nested diagnostics. |
| Byte order | `./result/bin/bytes_benchmarks --benchmark_filter='Bytes/'` | Scalar and misaligned raw reversal/conversion. |
| Geometry | `./result/bin/line_benchmarks --benchmark_filter='Line/'` | Complete/boolean intersections and point distance. |
| Matrices | `./result/bin/matrix3_benchmarks --benchmark_filter='Matrix3/'` | Arithmetic, aliasing, affine composition, transforms, inverse and row-major packing. |
| Scalar math | `./result/bin/math_common_benchmarks --benchmark_filter='MathCommon/'` | Fractions, modular wrapping, angles, comparison policy and byte quantization; scalar and batched inputs. |
| I/O | `./result/bin/io_benchmarks --benchmark_filter='IO/'` | Borrowed deserialization, VLQ, UUID, file writes and full asset payload reads. |
| Logging | `./result/bin/logging_benchmarks --benchmark_filter='Logging/'` | Raw/formatted messages, sinks, concurrent callers, files and spatial logs. |
| Flat hash storage | `./result/bin/flat_hash_benchmarks --benchmark_filter='FlatHash/'` | Insertion/duplicates, collision rehash, lookup, churn, reuse and sparse iteration. |
| Images | `./result/bin/image_benchmarks --benchmark_filter='^(Image/\|Vector/ImageBlend/)'` | Ownership, pixel operations, scaling, masks/blends, directives and libpng; historical `Vector/ImageBlend/*` rows now live here, not in `vector_benchmarks`. |
| LRU/TTL caches | `./result/bin/lru_benchmarks --benchmark_filter='^(Lru\|Ttl)/'` | Hits/misses, updates, trim/shrink, TTL refresh and cleanup. |
| Pools and small vectors | `./result/bin/block_allocator_benchmarks --benchmark_filter='^(BlockAllocator\|StdList\|StdMap\|SmallVector)/'` | Family allocation/reuse/release, real list/map nodes and inline/spill vectors. |
| Type identity and casts | `./result/bin/casting_benchmarks --benchmark_filter='Casting/'` | Identity, graph/owner/borrow casts, diagnostics and real Reader/NetElement graphs. |

`tile_performance_benchmarks` owns a visible SDL/OpenGL session on its calling
thread and uses the concrete renderer with a current context, not the threaded
application renderer facade. Keep cached/fresh framebuffer comparisons and
setup-failure/recovery checks before timing. This isolated renderer workload is
not production frame pacing or FPS evidence.

### Batch accounting and correctness oracles

- **Units and boundaries:** batch CPU time is not single-call latency. Use each
  label's batch count and work unit for amortized cost; vector rows are per vector,
  not per element. Concurrent logging uses process CPU time, including wakeup and
  completion. JSON includes destruction and, for path/patch cases, serialization.
  Respect labeled copies, resets, construction and destruction: construction-only
  pool/vector rows and casts excluding owner release are not whole-lifetime costs.
- **Oracles:** preserve untimed setup and independent complete-result validation
  before/after timing, including fixture/result fingerprints, order, identities,
  ownership, guards, bytes and cursors as applicable. Checksums alone cannot replace
  full-value checks. Keep real production allocators/codecs and deterministic
  fixtures; do not weaken checks to make a baseline pass. Labels/source define
  target-specific timed work and excluded cleanup.
- **Interpretation:** warm asset reads are payload work, not startup or physical
  storage throughput; file/log writes exclude durability sync. Full-hash collisions
  are stress cases. The image RGB alpha-only control is constant-foldable: count
  reductions, not pixel memory traffic (normalize the frozen consumer to one
  reduction per iteration). Matrix upload packing is CPU copying, not GL/GPU work.
- **Matrix baseline:** keep independent long-double full-value and cofactor
  oracles. Historical inverse results are incorrect; missing direction support and
  narrowed double-angle results also make `AfterOnly` cases incomparable. Exclude
  them from baseline speedup claims rather than relaxing validation.
- **Scalar-math baseline:** preserve the original implementation and complete pre/post timed-output oracles. Ordinary-domain angle comparisons use the same independent numerical tolerance; their output bits can differ. Fixed endpoint/extended-precision behavior and fractions outside the old integer-conversion domain are correctness cases, not baseline speedups. Labels report fixture/output fingerprints and results per item; normalize by both batch size and result count.

### Comparable measurements

Freeze the original production implementation and benchmark consumer before
rebuilding. Compare the same consumer with identical inputs, compiler, flags
(including ThinLTO), dependencies and filters. Adapt only API spellings and
module/namespace isolation; do not substitute reimplemented algorithms.

Match labels and complete-result fingerprints, pin CPUs and use repeated,
balanced original/current runs (for example, A/B/B/A). Retain raw JSON, every
regression and identical-binary A/A controls. Use four CPUs for concurrent
logging and one for single-caller cases; do not overlap timing with other work
or impose shared-CI timing thresholds. Image comparisons also check serialized
outputs and real Assets/metadata/Lua bindings in private storage.

Allocation/copy/hash counters explain work, not latency. Report allocation bytes
separately; microbenchmark results alone do not establish frame-rate gains.

## Runtime performance evidence

Use the [simulation/rendering architecture](engine-notes.md#independent-simulation-and-rendering)
for runtime design. Measure main-thread and `ClientLogic` CPU time at fixed and
uncapped rates; compare instruction counts at the same simulation/render cadence.
Uncapped per-frame counts also vary with fixed ticks amortized into each frame.
Use identical-binary A/A controls: affinity does not remove clock/cache/system
noise, and overlapping work does not guarantee higher FPS.

Measure uncapped rendering with VSync, capped with VSync and the same cap without
VSync separately; record the actual swap interval, not just the requested setting.
Keep synthetic waits out of performance runs. Delayed frame-setup handshakes and
slow swaps are separate checks; compare presentation alpha with the scheduler's
post-handshake sample.

Rendered-session evidence needs bounded readiness, not a fixed frame delay, and
real native scene layers rather than HUD imitations. Preserve same-tick display
sampling and lifecycle/reset checks without advancing simulation or repeating
integration. For cold entry, distinguish application-cache misses from kernel
page-cache coldness; correlate resource spans with the same native frame and
preceding updates without adding nested spans twice.

Private instrumentation, offscreen swaps and finer sampled coordinates do not
prove physical-display pacing. Startup smoke, same-build unit tests and Linux
measurements do not establish native GUI behavior elsewhere, cross-platform
interoperability or compatibility with historical/future protocol versions.
