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

### Entity snapshot export

`EntitySnapshot.*` exercises the production `world.entitySnapshot` binding:
missing entities, versioned persistent-entity round trips, a player replica
received through the real entity-create packet path, and projectile/plant-drop
full-state restoration without consuming replication updates.

Run it with owned assets through the isolated runner:

```sh
GTEST_FILTER='EntitySnapshot.*' ASTERBOUND_ASSETS=/absolute/path/to/Starbound/assets \
  ctest --test-dir build/linux-release -R '^game_tests$' --output-on-failure
```

Snapshots are saved with existing mod storage; there is no separate JSON-file
export API. Replica player snapshots may contain initialized defaults instead
of unsynchronized inventory, quests or script storage. Do not report them as
complete remote saves merely because serialization succeeded.

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

`ExceptionTest.*` covers typed error propagation, temporary summary ownership,
related-error aliasing and concurrent first rendering. `what()` is the short
original diagnostic, not the replaceable structured summary. Full rendering
resolves the captured origin, not the current caller's stack.

Compare `error_benchmarks --benchmark_filter='^Error/'` using matching binaries.
Separate capture without rendering, capture plus first full rendering, copying
an already rendered string, and assembling a report from cached details. The
first full render is per error; process-wide symbol caches may already be warm.
Compare the checked fixture labels, not raw addresses across processes.

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

### Python-style slicing

Run `ListSlice.*` in `core_tests` for owned results, extreme signed bounds,
full-width virtual input sizes, and ordered failure during bounded appends.
Check reverse slices with an omitted stop (`std::nullopt`) separately from an
explicit `-1`; a zero stride returns an empty result. For `String::slice`,
check codepoint selection rather than byte offsets, including embedded NUL and
malformed UTF-8 in an unselected suffix. `StringList::slice` owns its elements
and has no encoding failure to report.

For matched CPU timings, use `list_benchmarks --benchmark_filter=SliceCopy`
and `string_benchmarks --benchmark_filter=SliceMiddle`. The former measures
owned lists at unit and stride-three steps; the latter selects five middle
codepoints from ASCII, mixed UTF-8 and embedded-NUL inputs.

For a chat-bubble regression, send successive messages from the same entity.
The new bubble is inserted first, followed by the youngest previous bubbles.
With `maxMessagePerEntity` set to one, none are reinserted. For zero or negative
values, insertion preserves the old end-relative stop: omit the oldest
`1 - maxMessagePerEntity` previous bubbles, or all if there are fewer. The
subsequent per-tick visibility filter still applies.

### Command words

`split_shell_words` produces owning arguments, not POSIX shell syntax. Quoted
and unquoted segments concatenate; `""` is one empty argument, while a run
of separators contributes none. An unmatched quote extends to end of input.
Backslashes introduce escapes even inside single quotes. `ShellParser.*`
exercises client/server argument boundaries independently of command dispatch.

### SHA-256 compatibility

`ShaTest.*` checks independent digest vectors, chunk boundaries and hasher reuse.
Repeated `compute()` returns the same digest; the next `push()`, even empty,
starts a new message. The bit length uses all eight standard big-endian bytes.
Digests of messages below 512 MiB remain compatible; at and above that size,
old builds truncated the length. Persisted hash keys derived from such inputs
may need rebuilding from the original keys. The handshake hashes the unchanged
sequence of UTF-8 password bytes, UTF-8 account bytes and binary salt.

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

Run `TileSectorArrayTest.*` for wrapped world coordinates and default tiles.
Sector regions are half-open; zero width or height visits nothing, including
when the origin is inside a sector rather than at its boundary. A missing
column has no span but retains its clipped length. Parallel callbacks can
finish out of order: a `false` stops that strip, and all submitted work must
finish before the callback or array can be released. Distinct logical cells
of a packed-bit output are not necessarily independently writable.

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

### Polygon geometry

`PolyTest.*` covers strict SAT penetration (touching is not penetration),
singleton hulls, adjacent vertex compaction, axis-aligned bounds and signed
area. For movement changes run the existing `performance_benchmarks` with
`--benchmark_filter='^Collision/'`: compare the full result and permutation
labels between builds before comparing timings. One iteration is 64 moves;
these workloads do not measure clipping or hull construction.

For rectangle changes run `RectTest.*` and the real-asset
`WeatherTest.CompleteStateRestoresAfterLocalClear`. Rectangle subtraction emits
left, right, bottom and top strips, omitting empty strips. Keep them inside the
original viewport: the weather client uses their areas and iteration order to
sample particle positions. A boolean line hit alone does not establish edge
intersection parity; also compare the first hit and its contact flags.
Integer centers retain truncation toward zero. `with_center` places the same
integer half-size on either side, so an odd requested size rounds down.
`set_aspect` and `make_cube` report failure without partially updating integer
bounds. Use `intersects` when a yes/no answer is sufficient: a directed
integer `intersection` requires a signed type wide enough for the displacement.

### Periodic scheduling

`PeriodicTest.*` checks tick cadence and phase preservation when the interval changes.
`PeriodicFunctionTest.*` checks that a large delta advances light flicker by only
one half-cycle and that invalid parameters fail before consuming random draws.
Stock lamps can specify period variance equal to or greater than the period.
A nonpositive sampled duration reaches its target immediately, without division
by zero or a replacement draw.
Use `AssetsTest.ObjectCursorPreviewColdAndWarmCachesPreserveGameplayRandom` for
the installed-asset path and its gameplay/presentation random stream separation.

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
Point-coordinate regressions check the two components separately: each cell
draws `x`, then `y`, then invokes the data callback. A next-state check alone
cannot detect swapped coordinates.

Keep integer fast paths interface-visible and error construction cold. Batched
finite classification retains its reference/noinline boundary under fast-math;
speculative width subtraction and overflow interpolation stay narrowly precise.

`fill_uniform` retains the ordered scalar float draws under one service lock;
batching changes the possible interleaving between concurrent callers.
`fill_bytes` emits each 32-bit draw least-significant byte first. A partial tail
consumes one whole draw, and an empty span consumes none.

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

### RPC promises and JSON transport

Native callers use `RpcPromise<T>::make_pair()` and `with_cancellation()`;
the Lua factory remains `sb.makePromise`. Native method names are not RPC
wire identifiers: do not rename registered remote method strings with them.

Use `core_tests --gtest_filter='RpcPromiseTest.*:JsonRpcTest.*:LuaTest.*Promise*'`
and the asset-backed `game_tests --gtest_filter='TeamClientTest.*'`. A remote
`fail` completes successfully with JSON null; malformed replies must not retire
the keeper for a subsequent valid response. A truncated binary batch must not
dispatch even its complete prefix. Local cancellation and transform diagnostics
are separate from these wire outcomes.

When comparing performance, measure direct observation, delegated observation,
cached transforms and a complete serialized round trip separately. Time the
query, not construction of a long chain, when measuring observation cost.

### Bidirectional maps

Run `core_tests --gtest_filter='BiMapTest.*'` for duplicate handling, cross-index
ownership and the lifetime of borrowed keys. An overwrite may be passed keys
borrowed from the map itself. Node addresses, not hash-table iterators, must
survive unrelated insertion. A reference obtained through a temporary
`expected` does not extend the lifetime of the map it contains.
Run `StringTest.FixedEnumNames*` for `enum_name`, `enum_value`, and `maybe_enum_value`:
canonical spellings, case-insensitive lookup, and malformed UTF-8 versus a valid miss.

Use the existing `core_benchmarks` filter `^Core/(BiMap|Perlin)/` for comparisons.
The 128-entry move round trip includes both move construction and move assignment;
the right-conflict case also checks the returned left iterator. The Perlin rows
measure a four-entry enum table and are not a proxy for moving a large map.

### Perlin noise

Run `core_tests --gtest_filter='PerlinTest.*'` in the optimized native build.
The suite checks empty and moved-from objects, explicit seed overrides, parsed
parameter reuse, fractional and zero octaves, and invalid narrowing. Runtime
IEEE bit patterns exercise nonfinite coordinate and parameter checks under
fast-math. Large finite coordinates must match their 512-cell periodic images;
coordinates overflowing only after an octave step must return an error.

For the asset-facing path run
`game_tests --gtest_filter='TerrainDatabaseTest.UnknownNamesAndSeededNamedSelectorsPreserveSampling'`
with the normal game-test assets. Named-versus-direct agreement alone does not
prove seed compatibility: compare fixed-seed 1D/2D/3D sample bits for all three
modes against a pre-change build on the same target with the same FP flags.

### ID allocation

Use `core_tests --gtest_filter='IdMapTest.*:NetElements.*DynamicGroup*'` for inclusive
range wraparound, move-only payloads on collision, transactional decoding, and ordered
network removals. `core_benchmarks --benchmark_filter='^Core/IdMap/'` compares ordered
and hashed storage on the same ID sequences. The dense cases cross 256 occupied IDs
per operation; explicit duplicate rejection includes building the caller-visible error.
Do not infer hashed insertion cost from hole lookup alone: attempting insertion
at every occupied ID also runs the storage's duplicate-insertion path.

### Reload listeners

Run `core_tests --gtest_filter='Listener.*'` for aliased targets, cancellation during
recursive dispatch, the cross-thread clear barrier, and move-only callbacks.
`RootTest.SimultaneousRoots` exercises independent reload subscriptions with real
assets; `RootTest.LuaModulesRejectReentrantInvalidation` exercises hot reload from Lua.
The existing `core_benchmarks --benchmark_filter='^Core/Listener/'` separates idle
tracker polls from trigger-and-consume pairs. Each dispatch iteration calls 64
live listeners; the retained case also keeps 256 cancelled targets and handles alive.
These are dispatch costs, not full asset reload times.

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

### Universe directory locking

Run `core_tests --gtest_filter='LockFileTest.*'` for transfer, contention, and
filesystem errors. Unix subprocesses also check inode handoff, descriptor zero,
exec and process death; Linux checks an interrupted blocking `flock`.
With private assets and storage, run `game_tests --gtest_filter=ServerTest.DirectoryLockPrecedesLuaInit`.
The Lua counter distinguishes a rejected contender from a server that ran scripts
before discovering the lock, and checks reacquisition after failed initialization.

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
| Universe directory locks | `file_benchmarks --benchmark_filter='^LockFile/'` | Uncontended acquire/release, immediate contention and a 2 ms wait. Compare elapsed time for all three; CPU time distinguishes sleeping from busy retrying. |
| Logging | `logging_benchmarks --benchmark_filter='^Logging/'` | Ordinary and suppressed messages, whole short/multiline file records, four-recipient local/dedicated map preparation, concurrent callers, and spatial logs. Map timings exclude packet encoding and network I/O. |
| Audio | `ownership_benchmarks -bootconfig <private-config> --benchmark_filter='^Ownership/Audio/'` | Shared-byte clone/seek/read, PCM resampling, looping mixer callbacks and cold filter lifecycles. |
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

### Audio comparison boundaries

Reuse `ownership_benchmarks`; audio does not need a separate executable.
Clone rows include decoder/cursor reclamation. Resampling rows include seek-to-zero
and 512 output frames; mixer rows include eight 512-frame callbacks with 1/16/64
looping voices. Filter rows include buffer reset, factory, 64 stereo blocks and
reclamation, not just a steady-state kernel.

Validate complete PCM, read counts and frame cursors outside timing. Fixtures and
result labels carry fingerprints. Resampling, ordinary mixing and echo require
exact PCM; low-pass compares against independent zero-padded convolution within
one int16 LSB, so its original/current result hashes may intentionally differ.
Keep amplitude/pitch/pause/empty-loop fixes out of equivalent-throughput claims.
Clone measurements alone do not establish resampling or callback performance.

`AudioTest.*` and `MixerTest.*` cover scalar cursor ownership, finite inputs, short
resampling, empty loops, gain/ramp/pause behavior, batched positional gain, split
filter histories and effect error/reentry boundaries without assets. Asset-backed
coverage includes `AssetsTest.OggCloneIndependentCursorsAndLifetime` at nonzero/EOF
decompression positions and `MainMixerTest.*` for initially muted/live settings.
The latter permits one LSB after perceptual conversion and int16 quantization.

### Compression contracts and comparison

`star.compression` exposes span-based `compress`, `decompress` and
`decompress_gzip`; ByteArray is accepted as a contiguous range. Zlib keeps its
legacy empty-buffer representation and unlimited default decode size for saves.
Outputs clear before initialization; failed decode publishes only earlier
accepted codec calls. Overlapping input is staged before output mutation.
Owning results compact against actual size; reusable zlib outputs keep capacity.
The unused `CompressedFile` wrapper has been removed.

`ZstdContext` owns exclusive reusable native contexts and alternating scratch
buffers. Independent-frame errors preserve the caller's output; scratch can
retain its previous capacity until context destruction. `ZstdCompression` is the
one-shot convenience API. New empty results are valid Zstd frames; decoding also
accepts the old empty-buffer sentinel. Unknown-size frames still require streaming.

Move-only stream factories return values, not heap-allocated wrappers.
Compression flushes continuations rather than ending each frame. The bounded
decompression span overload reports consumed/produced counts for backpressure;
its input and output must not overlap. The ByteArray overload stages aliases,
appends within a per-call budget, and preserves the original logical output on
failure. Codec, progress and budget failures require `reset()` before reuse;
reset starts a new stream, not a retry of the discarded continuation.

TCP retains compressed input and native pending output while its plaintext buffer
is full, then resumes after packet parsing. This bounds buffering without imposing
a cumulative connection or multi-packet batch limit. P2P decoding applies the
same 64MiB packet-body bound as TCP. Embedded decoders independently retain their
strict 64MiB cap, bounded window, complete-frame consumption and rejection of
trailing/concatenated streams.
Embedded path layout errors take precedence over payload errors: validate all fields
before allocating decoded bytes. Raw image reads may omit frames but must validate
any supplied frame specification. Inline frame payloads are decoded by their JSON
consumer, not eagerly during image layout validation.

Run `CompressionTest.*:PathEmbedTest.*` in `core_tests`, and
`UniverseConnections.*:EmbeddedAssetsTest.*` in a private asset-backed `game_tests`
fixture. The transport regression includes two valid packets whose combined
plaintext exceeds one packet budget.

Reuse `byte_array_benchmarks --benchmark_filter='^ByteArray/(Zlib|Zstd)/'`.
Its eight owning one-shot rows use 17/4096-byte independent fixtures, full native
decode validation and four operations per batch, including destruction. Preserve
fixture/result labels across API-only consumer adaptation, balanced A/B samples,
identical-binary A/A controls, compiler flags and dependency identities. These
rows do not measure reusable-context throughput, network streams, embedded assets,
large high-ratio retained memory or whole-game performance.


### Font rasterization and glyph caching

Use the real grayscale and color fonts when checking the rasterizer. Compare
the full padded image (including transparent RGB), bearing and color flag,
not just nonzero alpha. Both formats place source row `y` at destination
`height - y`, leaving one border pixel on all four sides. Only active pixel
bytes belong to a row; source pitch can include padding or be negative.

Use `tile_performance_benchmarks --benchmark_filter='^Font/'` with a private
`-bootconfig` and a real OpenGL context. Separate CPU rasterization, cached
texture lookup, alternating-size width lookup and layered drawing. Compare
identical fonts, output fingerprints and renderer settings; synthetic pitch
tests cover row layouts that the installed fonts need not exercise.

### Either and its consumers

`Either` has two successful alternatives, not a success/error pair. Its wire
tags are **1 = left, 2 = right**, even when the payload types are identical.
Other tags consume only the tag and leave the destination unchanged. A
truncated payload reports an error without committing a partial replacement.
Do not substitute the zero-based `std::variant` stream codec.

When comparing lighting dispatch, use the existing `performance_benchmarks`
with a private `-bootconfig` and filter
`^Lighting/Propagate/(Colored|Monochrome)/Interior/Mixed/`. Compare the same
fixture, build profile and output oracle. These rows do not measure celestial
response assembly, wire throughput or whole-frame rendering.
Check the color and monochrome output fingerprints separately. In particular,
fast-math reassociation of the scalar RGB average can change the float bytes
even when both implementations pass their own serial-reference checks.

### Dynamic libraries

`DynamicLib::open`, `open_base` and `open_current` return a move-only library.
`find<int(int)>(name)` returns a callable that retains the lookup handle: resolve
it once, check the `expected`, then call it after the `DynamicLib` is destroyed.
This pins exports of the opened library and its load-time dependencies. A POSIX
current-process lookup can also see independently loaded global libraries:
open the exporting library itself if it may otherwise be unloaded. The signature,
including calling convention, must match the export's ABI; the loader cannot
check it. Paths and names containing NUL are rejected rather than truncated.
Final release drops the loader reference; OS unmapping is not an API guarantee.

Run `core_tests --gtest_filter='DynamicLibTest.*'` with the CMake-built fixture
library (an independent DSO, not linked into the test executable). The existing
`core_benchmarks --benchmark_filter='^Core/DynamicLib/'` uses the same exported
C function. `OpenClose` releases the validation handle before timing; it does
not drop the filesystem page cache. `ResidentOpenClose` holds a separate loader
reference; `FindAndCall` resolves, invokes and releases a symbol each iteration.
Compare elapsed time with the same fixture and build flags, pinning the CPU and
alternating the two binaries. None of these rows measures gameplay latency.

### Ed25519 identities and broadcasts

`star.curve25519` now uses libsodium, not the removed bundled curve arithmetic.
Headers and runtime require libsodium 1.0.21 or newer (ABI 26.3): earlier
`crypto_core_ed25519_is_valid_point` versions have incomplete subgroup checks.
See [point validation](https://doc.libsodium.org/advanced/point-arithmetic).
Nix and vcpkg carry the dependency; this is an intentional backend change.

Root creates one move-only identity from 32 OS-CSPRNG bytes before publishing
services. Entropy failures propagate without a PRNG fallback. `from_seed` takes
standard Ed25519 seed bytes without the historical pre-clamp; supplying an old
effective, already-clamped seed still produces the same standard key/signature.
Owned private bytes and temporary generated seeds are wiped with `sodium_memzero`;
borrowed caller-owned seeds remain the caller's responsibility.

Length checks precede native reads. Verification requires canonical main-subgroup
public keys and R points, and native scalar/signature checks; malformed, small-order
and noncanonical inputs intentionally lose legacy acceptance. A reusable Verifier
retains validated public-key bytes, not an undocumented point table. Native
signing still hashes its seed; no expanded-secret format or custom scalar math is
introduced. Input message/signature spans are borrowed only for each call.

Broadcast verifiers belong to current player entities and invalidate on key
replacement/removal, entity reuse and world reset. Modern and StarExtensions
tags/prefixes retain their wire bytes. Publication compares actual tags, avoiding
per-update serialization while repairing script changes and main-player reentry.
Raw broadcasts do not perform a redundant second signature.

Run `core_tests --gtest_filter='Curve25519Test.*:SecureRandomTest.*'` for
[RFC 8032](https://www.rfc-editor.org/rfc/rfc8032#section-7.1) complete key/signature
vectors, binary messages, move ownership, exact input boundaries, scalar
malleability and identity-key rejection. Linux's scoped native read fault checks
entropy failure and recovery. In a private asset-backed fixture run
`RootTest.SecureIdentityStability:RootTest.SignedBroadcastPublicationAndKeyReplacement`
for actual WorldClient signing/receive paths, modern/legacy key replacement,
missing/wrong tags and repair after an update.

Reuse `core_benchmarks --benchmark_filter='^Core/Ed25519/'`, with no new executable.
Twelve rows cover seeded key generation/destruction, hot signing, one-shot verify
and reused verifier at three RFC fixtures (0/1/64 message bytes). Complete public
keys/signatures, accepted valid and rejected corrupt signatures are checked outside
timing. Freeze the original production native binary before dependency removal;
adapt only the small API adapters and public-key accessor spelling, not the
fixtures, full-result checks or measured operations. Native process initialization
and verifier construction are untimed; seeded identity construction/destruction
is timed only in key-generation rows.

This compares the original C implementation with the strict libsodium-backed
API, including its validation and secret-lifetime costs. Dependencies intentionally
differ; do not claim a wrapper-only or identical-backend speedup. OS randomness,
Root/world construction, publication serialization, end-to-end voice/network
latency and FPS are not measured. Keep balanced A/B pairs, A/A controls, raw JSON,
source/binary identities and every regression.

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
