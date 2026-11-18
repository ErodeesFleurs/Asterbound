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

### Native renderer regression suite

Linux GUI builds include `renderer_regression_tests`, which uses real SDL
windows and backend rendering without Root or game assets. Run it only inside
an owned Xvfb with private HOME/XDG/D-Bus directories:

```sh
ASTERBOUND_RENDER_TEST_DISPLAY="$DISPLAY" ./dist/renderer_regression_tests
```

Without that explicit display opt-in, all cases skip before SDL initialization.
Vulkan cases require the Khronos validation layer and enable synchronization
validation. Readback copies the engine's presentation-source image, not the
swapchain image; the latter is not allocated with transfer-source usage.

The six cases cover canceled atlas uploads before differently sized cell reuse,
two/three consecutive ping-pong passes in both backends, OpenGL fixed/divided
target viewports and active-target resize, bidirectional framebuffer scaling
with single-sample/4x MSAA and byte/float targets, and framebuffer sampler-size
uniforms after resize with same-effect reselection. Pixel tolerances use integer
byte differences, avoiding floating-point assertion behavior under fast-math.
The old backend failed all five non-atlas pixel cases; its separate atlas
reproducer reported `SYNC-HAZARD-WRITE-AFTER-WRITE`. The repaired suite passed all
six cases on Mesa 26.3 llvmpipe/lvp with no Vulkan validation errors, including
teardown. Both production backends also reached the title, resized to 960x720
without clipping, and exited normally through Quit in private client fixtures.
This is software Linux evidence, not Windows or hardware-driver proof.

### Vulkan/OpenGL rendered-session checks

Both backends are built into GUI clients. Use a private HOME/XDG/D-Bus and
storage fixture, then select `-renderer opengl` or `-renderer vulkan`; do not
share writable saves between instances. Vulkan needs an actual WSI surface:
SDL's OpenGL offscreen path is not a Vulkan window test. Enable the Khronos
validation layer and synchronization validation when available.

Swapchain replacement must pass the current non-retired handle in
`oldSwapchain`. That call retires the old chain even when creation fails, so
rollback cannot acquire from it again. Independent render targets and pipelines
are prepared before retirement; failed replacements release both swapchain
generations, and output-setting rollback creates a fresh chain for the previous
mode. Exercise resize, VSync and legacy/SDR changes, plus failures before
retirement, during creation and while creating replacement image views. Each
recoverable output failure must still allow presentation and later reselection.

The retirement regression probe on Linux X11/llvmpipe performed 32 real
swapchain creations and 29 image acquisitions, with three injected allocation
failures covering those boundaries. It rejected missing/retired `oldSwapchain`
handles and acquisitions from retired chains, recovered presentation after each
failure, and exited with no live swapchains or core/synchronization validation
errors. The real client also resized its window, applied SDR through Graphics,
and quit normally. This does not establish Windows or hardware-driver coverage.

The November 2026 Linux X11/Xvfb check used Mesa 26.3 llvmpipe for both APIs.
An actual `Application`/`ThreadedRenderer` probe produced byte-identical baseline
RGB images, including ImGui. It exercised ordered getter/transport-slot reuse,
texture deletion before native flush, retained-buffer updates after an earlier
draw, two ping-pong postprocess passes, rejected scriptable parameter types,
MSAA/HDR toggles, resize, VSync changes, reload and normal teardown. Hidden-window
checks observed no presentation acknowledgment until restoration and no busy
render loop. No Vulkan core or synchronization validation errors were observed.

The real Vulkan client also created a private character, entered its ship,
moved, opened inventory, reloaded assets and exited through Save & Quit.
A legacy-only shader override was rejected with the affected effect/stage and
OpenGL/variant migration instructions. The scheduler suite and 58 frontend cases
passed; frontend execution used a newer compatible glibc loader because a host
audio dependency required a newer symbol version than the pinned build runtime.

These checks are software-renderer evidence, not hardware performance, physical
VSync/VRR, monitor HDR, Windows/macOS or all-Mod compatibility evidence. Keep
actual driver strings and screenshots with comparisons; average FPS or successful
device creation alone is not rendered-session proof.

The HDR follow-up probe first confirmed that `(2, 4, 8)` survives a float scene
buffer but clips to white at legacy presentation; dividing before presentation
recovered the channels. The linear-SDR path then matched independent CPU
tone-map/alpha references within one byte across 26 sampled results, including
EV -2/0/+2. White and byte-gray UI stayed at 255/128. A linearly filtered
black/white byte texture produced midpoint 188, not gamma-filtered 128.
The same Vulkan probe exercised float-buffer overrides, MSAA, resize, VSync,
framebuffer reload, mode reselection and unsupported HDR10 rollback, then exited
normally without core/synchronization validation errors. The three
`RenderOutputSettings` boundary/nonfinite/JSON-narrowing cases passed.
The real 800×600 client also exercised the expanded graphics menu: applying SDR,
displaying an unsupported-HDR10 error without exiting, discarding staged changes
with Back, and restoring the saved SDR/EV settings on restart. The final header,
body and footer were inspected in the running client. A separate native OpenGL
probe rejected linear SDR, accepted legacy again and exited normally. The full
client's unsupported-output startup logged the intended error but its unattended
fatal-error path timed out; this is not clean-exit evidence.

This machine's internal eDP panel has a 128-byte EDID with no extension blocks
or CTA HDR metadata; the isolated software surface advertises no HDR10 format.
HDR10 rejection is verified, but successful HDR swapchain presentation, physical
peak/paper-white calibration and HDR UI appearance still require an HDR-capable
display/compositor. Do not infer those results from SDR screenshots.


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
- Interpolation preserves scalar/vector arithmetic, integer result conversion,
  full-size sampling windows and inverse lower/upper duplicate selection.
  Nonperiodic cubic axes of length two or three use linear fallback; periodic
  axes retain cubic wrapping, including safe two-point neighbors. Empty lists
  and raw tensors return default values; checked `MultiTable` sampling rejects
  empty axes and recovers after reconfiguration. Tensor weight callables are
  const-invocable, deterministic and side-effect-free, evaluated once per axis.
  Retain non-affine rank-two/three/four and short-axis coverage, nested arithmetic
  association, complete image bytes and the legacy shifted bicubic image window.
  Keep low-rank bound collection separate from weight evaluation and retain
  the local fast-math inlining/reassociation boundaries.

### Multidimensional storage

`MultiArray` owns composed storage with last-index-contiguous layout. `extents()`
and `extent(dim)` describe shape; `size()` counts elements. `resize` preserves the
coordinate intersection and initializes new cells; `set_size` retains the linear
prefix instead. Use the latter before full overwrites, not as uninitialized storage.
Checked shape/growth operations reject invalid dimensions and overflowing products
before mutation. Trusted constructors use the existing fatal policy for bad shapes.

Retain zero-axis, clipped-copy and recovery coverage. `clear` and empty resizes
retain capacity; other reshapes need not. Region copies reject overlapping
self-copies before writing; identical regions are no-ops and disjoint ones work.
`view`, `elements` and checked `at` borrow only from lvalues and require real
references; proxy storage uses indexing, `set`/`get` and traversal. Reacquire borrows
after structural changes. Visitors and table evaluators are borrowed, including
noncopyable mutable callables; moved-from owners retain valid empty shapes.

`MultiTable` validates nondecreasing axes once when configuring or adopting data.
Duplicates and empty axes remain supported; failed configuration preserves the
old table. `assign` move-adopts matching axes and data without a second grid.
The owning array is read-only; bounded value views remain writable. Preserve
inverse-lower duplicate selection, including flat-domain boundaries.
Generic array serialization writes dimensions then last-index-contiguous values;
tile-update packets keep their distinct y-outer/x-inner traversal.

### Colors and byte codecs

`Color` uses finite straight RGBA floats and defaults to transparent black.
RGB byte getters round; alpha and the shared float-to-byte helpers truncate.
Gamma conversion changes RGB only; mixing does not implicitly convert color
spaces or premultiply alpha. `data()` borrows only from lvalues; `to_rgba_f()`
returns an owned value.

HSV hue uses turns: factories and `set_hue` clamp, while shifts wrap.
Image hue directives take degrees. Placement warnings retain their original
clamped red hue. Keep getter/setter saturation arithmetic in image kernels:
reusing the HSV tuple can change half-byte rounding under optimized fast-math.
Color hue shifts compute hue once and reconstruct through scalar saturation/value
getters, retaining the old float and byte results without a second HSV conversion.
The secondary-channel interpolation in `Color::hsva` retains separate
multiply/subtract rounding: ARM FMA can otherwise change a half-byte result.
`StarColor.cpp` uses `-ffp-contract=fast-honor-pragmas` so its local precise,
noncontracting region is honored without disabling fast contraction elsewhere.
Plain `-ffp-contract=fast` overrides the source pragma, even when LLVM IR lacks
contraction flags. Keep the non-primary exact-byte regression on ARM and x86.

Angle boundary tests use a precise, non-inlined helper and derive pi from
`__builtin_acosl(-1.0L)`, rounded to the tested type. Caller pragmas do not
retroactively change the FP mode of `std::acos` compiled into `import std`;
the imported float wrapper can still reach an approximate macOS `acosf`.
Keep the independent `remainder` oracle and exact interval checks unchanged.

Hex parsing accepts only 3/4/6/8 ASCII digits; malformed digits fail rather than
silently becoming zero. Named lookup preserves the custom palette, ASCII case
folding, full UTF-8 validation and embedded-NUL extent. JSON/Lua retain byte
RGB[A] arrays and their existing coercions; binary colors remain four sequential
RGBA float scalars in the selected byte order, not packed ARGB32 words.
Retain independent byte oracles, failed-decode destination state and recovery.

### Hex and Base64

`hex_encode` / `base64_encode` and their decoders borrow bounded spans;
owning decoders accept a counted `std::string_view` and return `ByteArray`.
Span results count bytes written, not input consumed, and append no terminator.
Hex writes complete pairs, maps invalid nibbles to zero and ignores an odd tail.
Base64 accepts unpadded tails, may write a partial encoded group, and stops at
the first `=` or nonalphabet byte, including whitespace and NUL. Classification
is ASCII-only, independent of locale; URL-safe characters are not accepted.

Encoders require disjoint buffers; decoders additionally support the same starting
address, not arbitrary overlap. Retain independent full-byte and untouched-tail
oracles across capacities. Owning encoding checks exact lengths before allocating;
unrepresentable sizes follow the existing fatal allocation policy. UUID text
decodes directly into fixed storage after checking decoded length, preserving
32/33-character acceptance and permissive nibbles rather than truncating oversized
input. Reuse the existing ByteArray valid/permissive decode benchmarks.

### B-tree storage and transactions

`BTree` and `BTreeDatabase` use snake_case APIs and `erase`; record visitors
receive borrowed const key/value references for the synchronous call. Retained
records and `find` results own their bytes. Generic record, node and recovery
callbacks are constrained by their checked result and invoked as lvalues.
Callbacks run under the shared database lock: do not mutate, close or change
cache capacity on the same database from a callback.

`BTreeTest.*` covers owned/borrowed storage, less-than-only keys, mutable
noncopyable visitors, ordered ranges, local node pruning and root-leaf counts.
Node visitor `false` prunes locally, not globally. A root leaf counts as one,
including an empty tree. Relocating linked leaves must preserve incoming links.
`BTreeDatabaseTest.*` covers bounded index caching, concurrent configuration,
VLQ-size transitions, empty values, record ownership, free-index padding and
range erasure with per-key commit failures. Keep the commit fault matrix,
explicit rollback/retry and reopen checks: failed sync does not prove rollback.
Preserve the disk layout and all three durability barriers.

Reuse `File/BTree/StageRollback` and `File/BTree/Commit` for matched timings;
`CommitPhaseDiagnostic` is instrumented evidence, not throughput. Report cache
pressure separately: the corrected capacity bound can increase misses compared
with the former unbounded insertion path.

### Random stream compatibility

`RandomSource` and synchronized `RandomService` use snake_case C++ APIs; Lua's
existing random callback and source method names remain unchanged. Fixed-seed
MWC outputs, ordinary valid draw order, static hash encoding and closed unit
intervals remain compatibility contracts. Reseeding uses `seed`, entropy mixing
uses `mix_seed`, and raw draws use `next_u32`/`next_u64`.

Checked signed integer ranges, two-bound unsigned ranges, ranged reals, normals,
stochastic rounding and static selection/shuffle propagate errors to consumers.
Invalid parameters fail before drawing. Full-width integer ranges and rejected
quotient tails are corrected boundaries; normal sampling rejects zero radius
without caching a spare variate. Source shuffling uses a full-width URBG; service
shuffling retains its single seed draw, including empty inputs. Reference sampling
borrows only from lvalues or borrowed ranges, never owning temporaries.
Keep exact output/next-state regressions and error recovery; do not repin world
snapshots merely to hide changed output.

Keep integer fast paths interface-visible and error construction cold. Batched
finite classification retains its reference/noinline boundary under fast-math;
speculative width subtraction and overflow interpolation stay narrowly precise.

`fill_uniform` retains the ordered scalar float draws under one service lock;
batching changes the possible interleaving between concurrent callers.

`secure_random_fill` borrows a bounded byte span and uses only the OS CSPRNG;
errors may leave a filled prefix. Grid point generation retains whole intersecting
cells rather than exact polygon clipping, with checked bounds and integer sector
keys and once-validated configuration. Invalid configuration still fails before
draws or callbacks in `generate`. No-argument TTL cleanup may skip traversal only
with a conservative earliest-timestamp bound; negative smear, TTL changes and
failed-insertion timestamps must remain covered. Filtered cleanup still visits
every entry. Revisiting evicted cells re-executes their deterministic callbacks.

### Weighted selection compatibility

`WeightedPool` retains original positive weights and entry order for JSON and
binary serialization. Checked insertion/range construction rejects nonfinite
weights and finite-total overflow; finite nonpositive weights remain ignored.
JSON decoding still converts an entry's item before insertion, even when its
weight is ignored, and returns the first conversion or insertion error.

Single selection retains the normalized scan and final-index fallback for
ordinary valid inputs: `select_index` consumes one `uniform_double` even for an empty pool;
owning `select` returns `Item{}` without consuming randomness when empty.

`select_unique` uses bounded weighted sampling without replacement by entry index,
not item equality. For partial selections of at least two entries, independent
exponential-clock ranks in log space avoid overflowing weight ratios; partitioning
and sorting the chosen indices take average `O(size + requested * log(requested))`
work and `O(size)` scratch space. Empty, one-entry and whole-pool requests use
shortcuts. Results retain original-index order, with index tie-breaking for equal
ranks. Partial multi-entry ranking consumes one seeded hash per stored entry;
the service overload always consumes exactly one `next_u64`, including a zero
request. Historical generated-seed results for unique selections, including
dungeon choices, intentionally change.
These are algorithm/compatibility guarantees, not measured runtime speedups.

### JSON values and preserved formatting

- Keep array bounds, self-aliasing and borrowed-child/snapshot coverage alongside
  direct-object writer traversal, embedded-NUL and invalid-UTF-8 partial-output
  checks. Builder copy/move coverage must finish active nested containers and
  reuse moved-from builders after malformed closes.
- Formatted insert/erase checks retain adjacent comments, duplicate-key reveal
  and exact numeric spellings, not just semantic equality. Preserve first-error
  order and missing/null/empty distinctions in real consumer checks.
- Patch regressions retain search-miss owner sharing, overlapping search-move
  insertion behavior, lazy merge values and unreachable suffix/error precedence.
  Native path protocol probes exercise read-only values, move-only mutable
  callbacks, custom parsers and empty callables. Actual Assets group-commit and
  real `json_tool` pointer/query get/set/add/remove checks complement unit tests.
- Reuse `json_benchmarks` plus the `Core/Json/` and `Hash/Json/` filters; do not
  create a parallel benchmark implementation. Direct-object output and immutable
  array edits include result destruction. Formatted token-only and
  serialize/destroy rows are separate; validate complete tokens/text and fixture
  fingerprints outside timing.
- Patch/path rows include serialization and destruction; do not attribute the
  complete timing to mutation alone. Keep identical consumer tokens apart from
  renamed APIs, compile flags, fixture/result labels and runtime dependencies.
  Retain alternating before/after samples, old/old drift and every regression.

### Path search

- `core_tests --gtest_filter='AStar.*'` covers cheaper open/pruned rediscovery,
  stale heap/early candidates, incremental/zero budgets, ordered validated
  fallback, stable partial failure, restart/extraction, active copies/moves and
  sticky typed callback errors. Compile-time consumers include non-default nodes
  with only the required ordering operations and reject malformed edges.
- `game_tests --gtest_filter='PlatformerAStar.*:PathController.*:MovementController.*'`
  uses private ephemeral real WorldServer terrain: floor walking, wall-detour
  flying, falling/platform drops and zero-cost landing, ledge/bounds overrides,
  missing-polygon error timing, opposite-target replan progress and actual Lua
  parameter/edge keys with repeatable incremental results.

### Asset locators

- `core_tests --gtest_filter='AssetPathTest.*:PathEmbedTest.*:ImageDirectivePipelineTest.*'`
  covers base-only filename/extension ranges, UTF-8/NUL and bounded input views,
  delimiter normalization, literal replacement sub-paths, relative concatenation,
  embedded identity, moves/source reuse, canonical text/wire and truncated reads.
- Run `AssetsTest.All:EmbeddedAssetsTest.*` with explicit owned `packed.pak` and
  repository `assets/asterbound` sources in a private boot configuration/storage.
  These exercise real Assets/Lua image frames/directives, WAV/font decoding and
  script/raw permissions. Keep the bundled-version guard active: regenerate
  CMake build metadata after `version.json` changes rather than changing asset
  markers or bypassing checks to make stale binaries pass.

### Images, gameplay and frontend lifecycles

- Image kernels retain full format/scaler coverage, byte-exact independent
  oracles, aliasing/ownership, checked layout and overflow, callback failure
  commits and PNG channel order. Pipelines use representative combinations and
  ordered failure prefixes instead of repeating every kernel matrix.
  `ImageScalingTest.*` uses narrowly scoped precise-FP scalar references;
  volatile loads alone do not prevent fast-math sampling changes. Keep independent
  nearest/directive checks on both sides of half-integer boundaries.
- Directives retain exact child text and ordered nonempty-child identity; empty
  appends preserve equality/hash and truth means nonempty. Entry ranges are UTF-8
  byte ranges. Parsed operations publish once and remain immutable; long children
  parse all operations on first operation access, not on metadata-only access.
  Preserve first-error order, committed successful prefixes, callback aliasing,
  source ownership and full pixels, including shared fade/scanline lookup tables.
  Font text policy must not mutate shared border syntax; glyph keys compare
  directive content and policy, not just hashes. Successful error reports suppress
  later reports per owner; failed callbacks remain retryable, without holding a
  lock across callbacks. Concurrent duplicate reports are permitted.
  The unchanged flattened string codec is not a lossless round trip for arbitrary
  group boundaries/prefixes; do not infer structural identity from flattened text.
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
| Asset locators | `./result/bin/core_benchmarks --benchmark_filter='^Core/AssetPath/'` | Byte parsing, canonical output, filename/extension, sub-path/directive edits and relative concatenation. |
| Path search | `core_benchmarks --benchmark_filter='^Core/AStar/'`; `game_benchmarks --benchmark_filter='^Game/AStar/'` | Graph chains/branches, partial fallback and budgeted restart; real WorldServer walk/flight. |
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
- **Asset-locator baseline:** use 64-result batches with 32/1024-byte UTF-8
  directories. Setup and independent full-result/fingerprint checks are untimed;
  result destruction is timed, and join/text use prebuilt locators. Compare
  filename/extension on base-only inputs: suffix-bearing cases fix an old range
  bug and are correctness regressions, not valid old/new speedups. Historical
  benchmark labels retain their spelling independently of the snake_case API.

### Path-search comparison boundaries

Eight `Core/AStar` rows validate complete independent graph paths and input/output
fingerprints. `searches/batch` is one for fresh searches and four for budgeted
restart rows; divide batch time by that count, not by visited nodes. Four
`Game/AStar` rows use real ephemeral WorldServer terrain; setup, terrain/oracles
and teardown are untimed. Search construction, exploration and result destruction
are timed. Registrations calibrate real time; report CPU and wall metrics
explicitly rather than confusing them.

Cheaper-rediscovery/pruned-node and repeated-partial-terminal defects are
correctness cases, excluded from equivalent speedups. Optional
`ASTERBOUND_ASTAR_DIAGNOSTICS=1` prints their untimed native outcomes. Keep original
production-linked binaries, consumer token/API-name parity, compiler/dependency
parity, alternating A/B results, A/A drift and every regression. These search-only
measurements do not establish whole-NPC update or rendered-frame gains.

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

Record texture redefinitions versus subimage updates, uploaded bytes and buffer
API CPU time before choosing a streaming scheme. Fewer storage-definition calls
do not prove less driver work: in-use texture or VBO overwrites can wait.
Validate packed upload bytes and native pixels for both legacy and detached
geometry, including atlas offsets, mixed primitives and multi-texture boundaries.

Keep GPU-query diagnostics separate from throughput runs. Timestamp pairs around
individual draw/upload calls measure those command spans, not utilization or the
whole frame; report skipped and unresolved queries rather than extrapolating.
They can materially perturb small draws. Offscreen EGL vendor selection must be
explicit and checked against the actual renderer; `DRI_PRIME` alone need not
override GLVND's selected vendor.

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
