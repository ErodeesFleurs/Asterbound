# Development guide

[Project overview](../README.md) · [Build guide](building.md) · [Tests and benchmarks](testing.md) · [Engine notes](engine-notes.md)

Use this guide when changing engine code. See the [build guide](building.md) for
setup, [engine notes](engine-notes.md) for runtime architecture and ownership, and
[tests and benchmarks](testing.md) for test entry points and performance evidence.
Run commands from the repository root.

- [Tooling](#tooling) and [brand artwork](#brand-artwork)
- [Threading and cancellation](#threading-and-cancellation)
- [C++ implementation boundaries](#c-implementation-boundaries)
- [JSON values and formatting](#json-values-and-formatting)
- [UI and rendering preparation](#ui-and-rendering-preparation)
- [Lua embedding](#lua-embedding) and [module loading](#lua-module-loading)

## Tooling

- C++ uses named modules, `import std;` and `std::expected`, without RTTI or exceptions. Use fresh build trees after compiler, ABI or sanitizer changes; supported toolchains are in the build guide.
- Shared benchmark fixtures are imported from `star.benchmark_*` modules; the asset-provider fixture is private to `file_benchmarks`. Windows core implementations import `star.windows_crt` for the thread-local invalid-parameter handler instead of redeclaring the CRT function.
- Configure clangd with `nix develop -c bash scripts/ide/configure-clangd.sh`, its generated `build/clangd/compile_commands.json` and clangd 23's `--experimental-modules-support`.
- Check changed C++/module lines with the pinned formatter; use `--apply` to format or `--base <commit>` for a branch comparison:

```sh
nix shell --inputs-from . nixpkgs#python3 nixpkgs#clang-tools --command scripts/format-source.sh --check
```

### Command-line arguments

Use `-name value` for parameters. A parameter consumes the next token even
when it looks like a switch or is `--`. Otherwise, the first standalone `--`
ends option recognition: every remaining token, including another `--`, is a
positional argument. `-version` and `-help` take precedence over ordinary
usage errors; when both are present, version information is printed first.

### Brand artwork

Regenerate the ASCII A monogram, client/server ICO files, mod icon, installer
bitmap and title wordmark with Python, Pillow and DejaVu Sans Mono:

```sh
python3 scripts/generate-branding.py --font /path/to/DejaVuSansMono.ttf
```

The font is rasterized, not bundled. Keep source strings ASCII, draw small ICO
frames separately, and use terminal green for the client and blue for the server.
macOS ICNS derives from the same PNG. Inspect native-size 16/24/32/48/64px icons
and the title screen at its configured image scale after changes.

## Threading and cancellation

- `Thread` has a fixed address: a native entry borrows its derived owner. Move an owning pointer, not the thread. `is_running()` and `is_joined()` are independent observations: a completed run must still be joined before restart. Self-join reports an error. Native creation/join failures stay recoverable through `std::expected`.
- `ThreadFunction` is a movable one-shot handle. `finish()` joins before consuming its result or diagnostic. State queries do not authorize concurrent consumption, moves or destruction of a handle. Ordinary `Thread::run()` failures are reported by the worker instead.
- `ReadersWriterMutex` lets the same thread acquire multiple shared locks while no writer is queued. A queued writer blocks new shared acquisitions, including those by current readers; do not rely on recursive shared locking across a waiting writer.
- Keep stop tokens stable for a run; renew the domain only after previous workers join. Terminal owners retain one lifetime domain. Request stop, notify waiters or shut down native I/O, then join before destroying borrowed state, including in derived owners. `Socket::close()` shuts down under shared ownership before waiting for exclusive descriptor release.
- `WorkerPool::stop()` cancels queued tasks; `finish()` executes accepted work and joins. Neither may be called from the pool's own jobs. `can_parallelize()` also excludes the caller draining the same pool: nested sector work must run sequentially rather than wait for itself.
- `add_work()` and `add_producer<T>()` store the concrete callable and invoke it as an lvalue. Errors belong to the task's shared completion. `poll()` never waits; `wait(millis)` can time out. Keep a named producer promise alive while using a reference returned by `get()`. Publication does not synchronize subsequent edits to a mutable result.
- `UniqueFunction` calls its stored target as an lvalue. Empty invocation reports a `StarException`; an `expected<T, StarException>` result is not wrapped a second time. Reference results borrow their referent. A void task may discard an ordinary result, but not a `std::expected` error.
- A target's move constructor or destructor may observe the withdrawn source wrapper as empty and clear it again. Do not install a new target into storage whose previous target is still being destroyed.
- `ListenerGroup::subscribe()` deduplicates active subscriptions by shared **owner**, not target address. Any duplicate handle can cancel the registration. Re-subscribing after cancellation uses the newly supplied target, even if it aliases a different subobject of the same owner.
- Listener dispatch is serial per group and may recurse. `disconnect()` waits for an in-flight callback except on that callback's own thread. `prune()` never removes a running registration, so `clear()` can still drain it after the owner subscribes again. `TrackerListener::consume()` coalesces notifications; it is not an event counter.

## C++ implementation boundaries

In `source/core`, use PascalCase for classes and structs, and snake_case for
concepts, functions and variables (including constants). Struct fields have no
`m_` prefix. Use standard-library names such as `size()` when the contracts match.
Keep externally visible Lua names, configuration keys and wire encodings unchanged.
Use spans for bounded ranges, references for required borrows and optional
reference wrappers for nullable borrows. Confine raw addresses to the native
ABI or standard-library operation that requires them; do not propagate them
into engine interfaces. Compose owning standard containers; formatter and trait
utility inheritance is intentional.
Borrows must not cross destruction, mutation or reentry that can replace owners.
Name checked results by their operation or payload, never numeric suffixes.
Use `if (auto written = ...; !written)` for status-only checks; keep decoded
values in named locals and preserve first-error, ownership and transaction order.

### Modules and host integrations

- Non-template module definitions belong in implementation units with direct imports, registered as private CMake sources with `CXX_SCAN_FOR_MODULES`. Interface imports do not expose implementation dependencies.
- Use typed Qt signals and an explicit receiver context. Keep `Q_OBJECT`/moc for custom signals or reflection, not ordinary receiver methods alone.

### Type identity and capability casts

- Use `type_key`, `type_name`, `query_type`, `query_type_of` and `dynamic_type_name`. Keys normalize top-level cv/reference qualifiers and are process-local, not persistent/plugin ABI IDs. Identity/name functions accept all C++ types.
- Capability graphs use `capability_queryable`, `capability_base` and `capability_graph`. Forward `TypeQuery&` with `query_type_of<Derived, Bases...>(*this, query)`; use `query_type<T>(object)` for a direct, const-preserving capability borrow. Qualify base queries to avoid redispatch. Shared virtual bases merge; distinct matching subobjects are ambiguous. Removing redundant registrations must not remove inherited capabilities/bases.
- `as_ref`/`convert_ref` accept an lvalue or a nullable `optional<reference_wrapper<T>>` and preserve constness; `as`/`convert` take owners and preserve adjusted aliasing semantics. Successful shared/intrusive rvalue casts transfer ownership; failure, including null stored-pointer aliases, leaves the source intact. Lvalue aliases retain their control block; weak aliases require successfully locked/cast owners.
- `RefPtr<T>(T&)` retains a live object; `detach()` transfers one count into a move-only `DetachedRef`, and `adopt()` consumes it even when both owners refer to the same address. Dropping the token releases its count. Move assignment must detach before destroying the old target: the source may be a member of that target. `RefCounter` cannot be copied or moved; counts remain non-atomic and externally synchronized. Keep incomplete-type declarations and protected destruction through the counter.
- Entity callbacks may retain aliases; reentrant `NetItem`/stream callbacks require fresh borrows afterward.

### Collections, names and value semantics

- Reuse checked `Star::transform`; `transform_into` appends to a distinct destination and keeps committed prefixes on failure. Key sorts snapshot each key once before reordering; key failure leaves values unreordered. Free `Star::filter` and explicitly qualified `Star::erase_if` erase immediately, preserving callback-visible destruction/failure prefixes. Member compaction is only for audited sequence paths.
- `take_optional` constructs before disengaging without requiring default construction/assignment. `optional_ref` borrows named nullable objects, preserving constness/wrapper-slot identity without retaining ownership. `OptionalOutput`/`output_optional` are diagnostic borrows, not snapshots; reject temporary owners/borrows. Wire decoding stages payloads before replacement: failure preserves destinations, not consumed input. Declare `HasStreamReadFactory<T>` before non-default `DataStream::read<T>()` use. JSON null skips converters; Lua mismatch and successfully empty payloads remain distinct.
- Preserve evaluation and iteration order: lazy fallbacks must not change random draws, callback-mutated collections need owning `auto(expr)` snapshots, and `std::views::zip` needs synchronously retained ranges. Typed sequence `contains` converts to the element type before querying. Fixed ASCII enum helpers retain canonical output/case-insensitive input; extensible tables use `EnumMap`. `std::monostate` must preserve nil, variant ordering and wire tags; named inventory wrappers remain distinct.
- `IdMap::emplace(args...)` allocates an integral ID and constructs its value; `try_emplace(id, args...)` reports a duplicate without consuming the arguments. Explicit IDs may be outside the allocation range and do not advance the cursor. `next_id()` advances without reserving, so obtain and register under the same synchronization when uniqueness matters. Equality compares range and contents, not the cursor; the wire format stores all three.
- Flat hashes constrain operations locally for recursive records; equality is contextually boolean and policies may be mutable. Exact-key inputs construct after a miss; converting inputs materialize before hashing. Growth uses bounded load arithmetic/cached hashes without rehashing resident values. Factory insertion constructs before growth and re-probes after callbacks. Duplicate borrowed self-ranges must not allocate/invalidate; avoid arbitrary range pre-reservation and retain moved-from bucket cleanup.
- Cache entry lookups borrow; `keys()`/`values()` own snapshots. Duplicate upserts stay lookup-only. `insert_or_assign` does not enforce capacity; `set_capacity` trims immediately, and `get` misses trim before generation, including failure. Recheck duplicates after reentrant/unlocked producers without trimming again. Mutable lookup touches MRU; const lookup observes. TTL expires only in `cleanup`, at strict `now > deadline`; preserve smear/RNG order, including failures. Const/refresh-disabled hits do not sample; random services borrow and lvalue callbacks need not be copyable/truthy.
- `BlockAllocator` uses allocator traits and shared rebind families with external synchronization. Copies deallocate interchangeably before first allocation; moves transfer ownership, clear source cache borrows and permit lazy reuse. Typed pools are lazy; `allocate(n != 1)` stays on the standard path. Slots are uninitialized storage, not zeroed/live objects; retain LIFO free slots, stable nodes and at most one empty block. Keep registry/block creation off per-node paths and reuse typed-cache borrows.
- `SmallVector` owns capacity-aware storage with explicit live size and per-operation constraints. Check overflow before allocation and reserve known lengths once. Publish replacement storage before retiring elements; preserve live-size changes during assignment callbacks and reusable moved-from state. Bulk uninitialized construction must not hide observable element construction.

### Path search

- `AStar::Search` constrains `search_node`/`search_edge_for` by the operations it
  uses: copyable values, node `<`, numeric cost and compatible source/target.
  Nodes need neither default construction nor all relational operators.
  Search data structs use unprefixed snake_case fields.
- Metadata owns one ordered node table. Strictly cheaper open/pruned discoveries
  replace predecessors; candidate revisions discard stale heap/early entries.
  Closed nodes remain closed. Closest fallback retains node-order ties and
  terminal validator order, not a cached world-dependent best candidate.
- Callbacks own their callable and receive const references synchronously; they
  must not reenter the search. Errors repeat until `start`, which resets outcome,
  paths and active state while retaining scratch capacity. Pending, success and
  failure are independent of whether a partial path exists.
- `result()` borrows until mutation; `extract_result()` transfers the path and
  disengages it without changing the terminal outcome. Lua `result()` remains
  repeatable. Native snake_case names do not rename Lua methods/parameter keys.
- Per-call budgets count candidates, including stale entries. The legacy total
  cutoff remains `closed_count > max_nodes_to_search`; closest-path finalization
  is outside the candidate budget. Neither limit is a wall-time guarantee.
- Platform searches preserve wrapping `WorldGeometry`, two-times-Manhattan
  weighting, neighbor order, zero-cost Land edges and source-based distance
  filtering. Cache only immutable standing-polygon bounds; never retain world
  collision/liquid/gravity results across ticks. Preserve separate full/stand/drop
  overrides and lazy missing-polygon errors. Path splicing compares both old/new
  edge targets before retaining traversal progress.

### Numeric and geometry contracts

- `std::clamp` requires ordered bounds; legacy `Star::clamp` returns the upper bound for reversed ranges. Preserve interpolation, rounding, wrapping and fast-math-safe finite classification. Byte-order helpers accept padding-free integrals and 32/64-bit floats, not aggregates/pointers; decode with `from_byte_order`. Raw reversal permits unaligned/null zero-length ranges, but ranges must be disjoint or identical.
- `MathCommon` uses operation-specific concepts. `near_equal` compares integral pairs without signedness loss and floating pairs with relative epsilon; mixed integer/floating inputs require explicit conversion. `near_zero` retains its minimum-normal threshold, not an epsilon tolerance. Floating helpers require finite inputs/intermediates.
- `fractional_part` uses floor semantics without an integer-range limit and preserves signed zero through a local precise-FP region; a zero branch alone can be folded away under fast-math on ARM. `positive_mod` requires a positive integer modulus; floating zero modulus preserves its input. `wrap_difference` accepts signed integers or floats and preserves half-period direction. Angles use typed reduction into `[-pi, pi)` without adding pi first; a local full-precision constant avoids the pinned standard module's double-rounded `pi_v<long double>`. Degree-conversion constants retain their previous double values; byte quantization retains saturation and truncation.
- `Line` accepts non-boolean arithmetic coordinates/positive dimensions; 2D constraints must not exclude integer/3D basics. `min()`/`max()` are oriented endpoints. Preserve tolerances, truncation, endpoint/degenerate behavior; boolean intersection shares the full-result predicate without unused outputs.
- `Matrix3` defaults to zero and owns nine unpadded row-major elements (`Mat3F` 36 bytes, `Mat3D` 72 bytes). It multiplies column vectors without homogeneous division. `rotate`/`translate`/`scale` left-multiply the complete matrix, including a general third row; `operator*=` right-multiplies with self-aliasing. JSON/Lua/DataStream retain row/byte order.
- Matrix constraints are per operation: integer basics remain valid, unchecked angles accept floating types including `long double`, and checked numerics use finite-classifiable `float`/`double`. Inverse/orthonormalization mutations commit only complete success, rejecting nonfinite/degenerate results without a global determinant cutoff or condition-number promise. Orthonormalization's relative residual tolerance defaults to `16 * epsilon`; finite nonnegative values, including zero, are valid. `is_orthogonal` checks the complete row Gram matrix, accepts reflections and rejects invalid tolerances. Use fast-math-safe `is_finite`.
- Mixed-scalar point/direction transforms promote arithmetic, converting only final coordinates to the input vector type; matrix/vector `operator*` keeps its matrix-scalar result. Angle transforms use the common floating type before final conversion. Lvalue row indexing borrows; rvalue indexing owns. Pack through `copy_to(span<T, 9>)`, never across nested row subobjects. GL stack packing updates transforms once per nonempty buffer draw, without cross-draw caching; empty draws retain error checks without uniform/binding updates. Backend changes need matched evidence, not packing-only benchmarks.

### Binary data and I/O

- `HostAddress::from_bytes` accepts exactly four IPv4 or sixteen IPv6 bytes. `bytes()` borrows from an lvalue; extracting `address()` from a temporary endpoint instead returns an owning value.
- Endpoint text splits at the last colon and accepts bracketed or unbracketed IPv6. Ports are complete decimal tokens in `[0, 65535]`; overflow is an error, not a request for ephemeral port zero. Keep invitation text compatible: eight lowercase four-digit groups, no output brackets. Hostname selection stays with the system resolver.
- Socket timeouts are milliseconds. `set_timeout(0)` disables the send/receive timeout; `Socket::poll(query, 0)` checks readiness without waiting. A closed descriptor is an exception event, not a timeout.
- For a single socket use `wait_for_data(writable, deadline)`, then attempt nonblocking I/O: a successful wait can also mean timeout or shutdown. Keep the shared native handle alive without holding the socket lock across the wait. On Windows the shutdown wake stays readable for every waiter.
- `File` exclusively owns its native handle; Unix descriptor zero is valid. Explicit `close()` reports failures and consumes ownership even on error. Never retry it against a possibly reused descriptor. A clone reopens the existing path with an independent cursor and does not replay truncate or append.
- `LockFile::try_acquire` distinguishes a busy path (empty optional) from an OS error. `unlock()` consumes the handle even on close failure. On Unix the empty `universe.lock` **must remain in place**: the kernel locks the inode, not its name. Do not delete or replace the file to clear a lock. Stop servers from older versions that unlink it before upgrading a shared universe. Windows uses an exclusive, delete-on-close open.
- A `UniverseServer` owns its directory lock before Lua initialization and until its storage users are destroyed. A failed contender must not run mod scripts or clean universe files.
- Exclude lifecycle changes from concurrent file I/O. Unix positioned operations use `pread`/`pwrite`; Windows serializes the cursor save, operation and restore. A sequence of separate public `seek()` and `read()` calls is not atomic.
- On Windows, `File::at_end()` and `File::clone()` observe the native cursor directly under the cursor lock. A subclass that presents a logical position or size different from the native file must also implement the corresponding end/clone behavior.
- VLQ adapters accept integral/enum values. Synchronous `deserialize*` borrows unchanged, retained `span<const char>` input; decoded fields own storage and serializers transfer output with `take_data()`.
- Prefer final `DataStreamReader` for contiguous decoding without read-ahead. Extensible buffers retain virtual `read_data` transformations/failures. Preserve container prefixes and complete-value commits on truncation; memory-device seeks, clone cursors and closed empty reads retain their own contracts, not file semantics.
- B-tree comparisons and synchronous visitors borrow from retained node handles;
  owning queries/exports still copy the bytes they retain. Move only node ranges
  that are subsequently erased. Maintain the leaf encoded-size total at every
  mutation; it is in-memory metadata, not a new disk field.
- Database cache settings follow the main-lock-before-cache-lock order. Publish
  decoded indexes through the bounded cache; keep disk I/O outside its spinlock.
  Keep transaction staging, root publication barriers and explicit failure recovery.

### Audio decoding and mixing

- `Audio` clones share immutable encoded/PCM bytes but retain independent decoder
  or scalar-sample cursors. `total_samples`, `seek_sample` and `current_sample`
  count frames; `read_partial`, `read` and `resample` count interleaved samples.
  Partial PCM frames and their clone cursors remain valid.
- Complete decoding stages a fresh decoder at frame zero, uses a bounded metadata
  allocation hint, restores the original frame position and commits only success.
  Reuse typed, uninitialized resampling capacity; do not add buffer-wide clearing.
  Keep native codec pointers confined to C-API boundaries.
- Resampling retains per-call source-frame ceilings and integer eight-tap
  arithmetic. Share tap indices across channels and broadcast converted channels;
  a stateful fractional-phase converter is a separate audio-contract change.
- Mixer registry locks protect owning snapshots, not decoder work or external
  effects. Instance state and positional gains share the instance mutex.
  `read` remains single-reader/non-reentrant; attenuation runs synchronously in
  `update`, may inspect/change instances, but must not reenter `update`.
- Keep fixed group state in arrays and preserve per-voice traversal/saturation.
  Effects publish scratch only on success. Low-pass uses integer ring sums;
  echo stores the quantized processed output in its feedback ring.
- Native audio APIs use snake_case; Lua method strings and configuration keys
  retain their existing spellings.

### Asset locators

- `AssetPath` owns `base_path`, `sub_path` and `directives`. Parsing helpers
  borrow byte views only during the call. The first `:` before `?` introduces a
  sub-path; later colons are ordinary bytes. Only `/` separates directories.
  Do not apply filesystem normalization or fold `..`.
- `filename` and `extension` inspect the base path only, excluding sub-paths and
  directives. `relative_to` concatenates the source directory and a relative
  locator without interpreting its suffix.
- Use `set_sub_path`, `remove_sub_path`, `get_directives`, `add_directives` and
  `remove_directives`. Sub-path edits normalize existing empty `?` entries but
  preserve replacement bytes literally. Raw directive edits retain their
  existing boundary rules; `DirectivesGroup::append_to` is not canonical joining.
- `join` and `write_text` share canonical entry emission. An explicitly engaged
  empty sub-path emits `:`, although parsing a trailing colon discards it.
  Moves consume and reset source optional/directive state; self-move is stable.
  Wire reads replace the destination only after the complete String is decoded.
- Equality/hash remain byte-sensitive, including embedded payloads.
  `is_embedded` recognizes only lowercase `/opensb_data;`; `is_remote` currently
  has the same recognition rule and does not imply network fetching.

### Images and pixel processing

- Check layout arithmetic in `size_t`, bounded by `PTRDIFF_MAX`; `Image::size_bytes` validates without allocation. `for_overwrite` requires writing every byte. Construction/changed-layout reset zero storage; unchanged-layout reset preserves pixels.
- Pixel spans borrow unique-owned storage; reallocation invalidates them. Batches borrow retained, non-null reference-wrapped operations. Confine libpng handles/callback pointers/row tables to RAII adapters; jump targets stay in trivial frames, with Image/row/diagnostic owners outside.
- Bound bulk ranges without weakening hardening or advancing beyond allocation. Preserve raw BGR order, source-over truncation, self-overlap's forward feedback and legacy float-format byte access. Scalers retain sampling/quantization/hidden RGB/tap behavior; validate actual allocated windows. Fuse color operations only when intermediate byte truncation/parity is preserved; keep HSV/replacement/reference barriers and crop ordering.

### Logging and native process output

- Filter before formatting, then recheck levels under the dispatch lock. File sinks report short writes without retries/rollback/sync; checked `make_shared` construction uses a private key. Preserve non-inserting spatial queries, newest-suffix queue caps and ownership-transferring drains.
- Exception summaries and details share one dispatch. A summary callback can change Debug visibility before details are rendered; when a delivery fails, retry only that delivery's repeat record, not a newer one installed by a reentrant callback.
- Use `LogMap::snapshot()` for a sorted read and `drain()` to detach a complete batch before sorting. Never compose a read and `clear()` for HUD consumption. A dedicated server that cannot enqueue the debug batch restores only missing keys; newer writes take precedence. Local broadcasts must leave the HUD's values intact.
- The SDL logger borrow exists only after startup and resets after application destruction/native callback shutdown. GUI entries create `NativeRuntime` with `StandardStreams::Optional` before initialization/workers. Repair only missing stdout/stderr with `NUL` or `/dev/null`, preserving redirection/errors, including valid CRT output without a Win32 handle. POSIX repair reserves the original descriptor; default `Inherit` keeps CLI output checked.
- `EmergencyLog` is best-effort reporting, not exit policy. On stderr failure, send the original message/error to `OutputDebugString` or syslog without recursion/blocking dialogs. Restore thread-local CRT invalid-parameter policy after recovery. Ordinary logger/file/`Star::print` errors remain checked.

## JSON values and formatting

- `Json` is immutable. Prefer `find`, `array_ptr`/`object_ptr` and synchronous `iterate_array`/`iterate_object`; `to_array`/`to_object` copy mutable containers. Retain owners for borrowed children. Borrowing generation data must preserve random draws/object traversal order; not every snapshot can become a view.
- Array `append`, `insert` and `erase` build the final container directly; validate indices before copying and preserve self-aliasing/snapshots. Direct `JsonObject` text output must not construct a temporary owning `Json` or change traversal, UTF-8 checks or partial-output boundaries.
- Use `json_to_list` for ordered conversion. Preserve missing versus null, source order and first-error behavior in field/default tables. UTF-8 parsing borrows unescaped tokens only during synchronous contiguous-input callbacks; `utf8_remaining()` is a view, not stored pointer state. Escaped/generic input retains decoding, Unicode checks and error order.
- `JsonBuilderStream` caches the active sentry in existing stack entries. Copies, moves and failed closes must preserve valid nested-container state; moved-from builders remain reusable. Constrain input adapters, UTF-8 iterators, output sinks and serializer/builder protocols independently. Helper concepts use snake_case; state fields use semantic names such as `m_character`, not type prefixes.
- `FormattedJson::create` reuses semantic values; `create_array(span)` batches output while retaining comments/numeric spellings. Insertion fills one reserved token buffer; deletion copies only the retained prefix and suffix into final storage before rebuilding the index. Preserve asymmetric trivia ownership, repeated numeric spellings and duplicate-key reveal. Token indexing must not alter hashes/wire order.
- JsonPath free APIs use snake_case (`path_get_ref`, `path_apply`, `path_update`, `parse_pointer`). Readable and editable value protocols are separate; concrete parser/operation callables stay typed through traversal, including move-only mutable callbacks. Runtime `Path` keeps custom parser support and dispatches built-in parser variants once per call. Borrowed paths/callbacks never outlive their synchronous call.
- `json_patch` search mutations resolve, match and modify in one traversal. `path_update` returns `expected<optional<Json>, StarException>`: an empty optional means no change and must not rebuild ancestors; a contained null Json is a replacement. Keep the first match, per-operation field/error order, lazy unreachable path suffixes and immutable snapshots. Search `move` deliberately adds before rewriting the original source array, unlike ordinary move. Nested Assets patch groups commit only after complete success; preserve test-failure versus patch-error categories.

## UI and rendering preparation

Follow [runtime ownership and timing](engine-notes.md#independent-simulation-and-rendering);
these rules concern preparation within those boundaries.

### UI callbacks and draw ordering

- Options callbacks retain local changes before live writes/label updates, capture keys once and update Root's head-rotation cache. Measurement/wrapping shares only `font=`, `set` and `reset`, not full rendering commands.
- Draw all highlight underlays before main drawables/overlays; retain originals for overlays. Preload jitter uses presentation randomness. Queue every horizon layer's textures and load both halves before either submission; preserve geometry/UV/arithmetic order.

### Presentation views and animation caches

- Share one `PresentationContext` across each frame's geometry, lighting and overlays. Its interpolation/epoch is presentation state; discontinuities select current poses without changing the epoch.
- Query `NetworkedAnimator::sample(alpha)` views for sampled data, passing them to nested pose work. Root views borrow independent scratch slots; copies share overrides and reuse waits for the last copy. Animators outlive views. Views/attachment borrows cannot cross logical updates, equipment/configuration changes or reloads. Samples do not mutate logical animation/NetElements; `prepareAnimatorPose()` and its view overload prepare tick/display poses respectively.
- Cache discrete properties only for matching configuration/state/frames; evaluate timing/interpolation each sample. Mutable `parts()` disables cross-sample reuse, including after assignment; inspect via `constParts()`. Validation stays lazy and failures retryable. Local edits invalidate cached transforms for all view copies, not independent views; reused slots clear transforms, and zoom/flip stays outside the cache.

### Tick-owned effects

- `WorldClient` drains live events before entity destruction; `destroy()` adds only new death effects. `EffectSource::finishEventCollection()` acknowledges tick stages independently of rendering, without a second event bus.

## Lua embedding

See [engine notes](engine-notes.md) for runtime ownership, the
[Lua API reference](lua/openstarbound/) for scripting behavior and the
[mod compatibility guide](modding.md) for compatibility.

### Stack values and typed calls

Native C++ embedding methods use snake_case (for example, `create_context`, `set_execution_host`, and `set_promise_lifetime`). Lua-visible callback keys, including `setPause`, are mod APIs and keep their existing spelling.

- `LuaConverter<T>` converts through `LuaStack`/`LuaStackValue`, including coroutines. Stack/string views borrow; retain with `own()` or owning Lua handles.
- Typed `void`, scalar, tuple and `LuaVariadic` calls request zero, one, tuple-N and all results; discard unused results before conversion.
- Dotted path segments are byte strings: an embedded NUL is part of the table key, and adjacent dots denote an empty key. Reads and writes must respect Lua table metamethods. `eval` parses the entire byte span, trying the expression form before falling back to a statement on syntax error.
- Keep a saved `loadstring` function's default environment reachable through Lua, not through a reusable integer registry slot. Install native callbacks on the destination stack; only materialize a persistent function handle when a caller needs to retain it. Forward owned callback returns without consuming borrowed results.

### Context retirement and shutdown

- `LuaBaseComponent` owns execution/reload/shutdown. Retirement waits for the outermost call; join workers before dependent bindings disappear. Evaluation errors retain contexts; invocation/message errors disable them. Defer callback removal after reentrant `uninit` until hooks return, retaining host services.
- One-shot item scripts settle init/invoke/uninit under the Lua lock before committing changes. Cleanup errors stay primary with earlier failures as context; retirement must not resurrect failed uninit hooks.

## Lua module loading

- `require("/absolute/asset.lua")` runs in the caller's environment/coroutine and returns no values. Entry scripts/nested requires run once per context; only bytecode is shared. `_SBLOADED` is not a mutable cache hook.
- Cycles report the asset-path chain. Failed modules are retryable; successful dependencies remain loaded, without rollback of globals/native effects.
- Reload/hot reload and restart invalidate bytecode. Existing contexts keep loaded modules but need recreation to load new ones. Invalidation during loading rejects completion rather than mixing revisions.
- Require callbacks borrow `(LuaStackValue environment, StringView module)` and execute through `environment.stack().load(environment, bytes)`.
