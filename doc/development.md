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
- Configure clangd with `nix develop -c bash scripts/ide/configure-clangd.sh`, its generated `build/clangd/compile_commands.json` and clangd 23's `--experimental-modules-support`.
- Check changed C++/module lines with the pinned formatter; use `--apply` to format or `--base <commit>` for a branch comparison:

```sh
nix shell --inputs-from . nixpkgs#python3 nixpkgs#clang-tools --command scripts/format-source.sh --check
```

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

- Thread creation/joining use the native `std::expected` backend: `std::thread`/`std::jthread` throw on creation failure. Use standard mutexes, condition variables and atomic waits. `ReadersWriterMutex` retains repeated shared acquisitions by one thread and queued-writer priority, unlike `std::shared_mutex`.
- Keep stop tokens stable for a run; renew the domain only after previous workers join. Terminal owners retain one lifetime domain. Request stop, notify waiters or shut down native I/O, then join before destroying borrowed state, including in derived owners. `Socket::close()` shuts down under shared ownership before waiting for exclusive descriptor release.
- Worker pools retain checked startup rollback, cancellation-versus-draining semantics, stored-lvalue task invocation and checked result publication.

## C++ implementation boundaries

Use PascalCase for classes/structs and snake_case for functions/concepts. Compose
owning standard containers; formatter/trait utility inheritance is intentional.
Make ownership explicit, use optional reference wrappers for nullable borrows,
and keep raw addresses at native/storage boundaries, not business interfaces.
Borrows must not cross destruction, mutation or reentry that can replace owners.

### Modules and host integrations

- Non-template module definitions belong in implementation units with direct imports, registered as private CMake sources with `CXX_SCAN_FOR_MODULES`. Interface imports do not expose implementation dependencies.
- Use typed Qt signals and an explicit receiver context. Keep `Q_OBJECT`/moc for custom signals or reflection, not ordinary receiver methods alone.

### Type identity and capability casts

- Use `type_key`, `type_name`, `query_type`, `query_type_of` and `dynamic_type_name`. Keys normalize top-level cv/reference qualifiers and are process-local, not persistent/plugin ABI IDs. Identity/name functions accept all C++ types.
- Capability graphs use `capability_queryable`, `capability_base` and `capability_graph`. Qualify base queries to avoid redispatch. Shared virtual bases merge; distinct matching subobjects are ambiguous. Removing redundant registrations must not remove inherited capabilities/bases.
- `as_ref`/`convert_ref` borrow and preserve constness; `as`/`convert` preserve pointer/aliasing-owner semantics. Successful shared/intrusive rvalue casts transfer ownership; failure, including null stored-pointer aliases, leaves the source intact. Lvalue aliases retain their control block; weak aliases require successfully locked/cast owners.
- `RefPtr` retains unique storage ownership, an intrusive deleter, externally synchronized non-atomic counts, incomplete-type support, protected destruction through `RefCounter` and acquire-before-release reset. Entity callbacks may retain aliases; reentrant `NetItem`/stream callbacks require fresh borrows afterward.

### Collections, names and value semantics

- Reuse checked `Star::transform`; `transform_into` appends to a distinct destination and keeps committed prefixes on failure. Key sorts snapshot each key once before reordering; key failure leaves values unreordered. Free `Star::filter` and explicitly qualified `Star::erase_if` erase immediately, preserving callback-visible destruction/failure prefixes. Member compaction is only for audited sequence paths.
- `take_optional` constructs before disengaging without requiring default construction/assignment. `optional_ref` borrows named nullable objects, preserving constness/wrapper-slot identity without retaining ownership. `OptionalOutput`/`output_optional` are diagnostic borrows, not snapshots; reject temporary owners/borrows. Wire decoding stages payloads before replacement: failure preserves destinations, not consumed input. Declare `HasStreamReadFactory<T>` before non-default `DataStream::read<T>()` use. JSON null skips converters; Lua mismatch and successfully empty payloads remain distinct.
- Preserve evaluation and iteration order: lazy fallbacks must not change random draws, callback-mutated collections need owning `auto(expr)` snapshots, and `std::views::zip` needs synchronously retained ranges. Typed sequence `contains` converts to the element type before querying. Fixed ASCII enum helpers retain canonical output/case-insensitive input; extensible tables use `EnumMap`. `std::monostate` must preserve nil, variant ordering and wire tags; named inventory wrappers remain distinct.
- Flat hashes constrain operations locally for recursive records; equality is contextually boolean and policies may be mutable. Exact-key inputs construct after a miss; converting inputs materialize before hashing. Growth uses bounded load arithmetic/cached hashes without rehashing resident values. Factory insertion constructs before growth and re-probes after callbacks. Duplicate borrowed self-ranges must not allocate/invalidate; avoid arbitrary range pre-reservation and retain moved-from bucket cleanup.
- Cache entry lookups borrow; `keys()`/`values()` own snapshots. Duplicate upserts stay lookup-only. `insert_or_assign` does not enforce capacity; `set_capacity` trims immediately, and `get` misses trim before generation, including failure. Recheck duplicates after reentrant/unlocked producers without trimming again. Mutable lookup touches MRU; const lookup observes. TTL expires only in `cleanup`, at strict `now > deadline`; preserve smear/RNG order, including failures. Const/refresh-disabled hits do not sample; random services borrow and lvalue callbacks need not be copyable/truthy.
- `BlockAllocator` uses allocator traits and shared rebind families with external synchronization. Copies deallocate interchangeably before first allocation; moves transfer ownership, clear source cache borrows and permit lazy reuse. Typed pools are lazy; `allocate(n != 1)` stays on the standard path. Slots are uninitialized storage, not zeroed/live objects; retain LIFO free slots, stable nodes and at most one empty block. Keep registry/block creation off per-node paths and reuse typed-cache borrows.
- `SmallVector` owns capacity-aware storage with explicit live size and per-operation constraints. Check overflow before allocation and reserve known lengths once. Publish replacement storage before retiring elements; preserve live-size changes during assignment callbacks and reusable moved-from state. Bulk uninitialized construction must not hide observable element construction.

### Numeric and geometry contracts

- `std::clamp` requires ordered bounds; legacy `Star::clamp` returns the upper bound for reversed ranges. Preserve interpolation, rounding, wrapping and fast-math-safe finite classification. Byte-order helpers accept padding-free integrals and 32/64-bit floats, not aggregates/pointers; decode with `from_byte_order`. Raw reversal permits unaligned/null zero-length ranges, but ranges must be disjoint or identical.
- `MathCommon` uses operation-specific concepts. `near_equal` compares integral pairs without signedness loss and floating pairs with relative epsilon; mixed integer/floating inputs require explicit conversion. `near_zero` retains its minimum-normal threshold, not an epsilon tolerance. Floating helpers require finite inputs/intermediates.
- `fractional_part` uses floor semantics without an integer-range limit and preserves signed zero. `positive_mod` requires a positive integer modulus; floating zero modulus preserves its input. `wrap_difference` accepts signed integers or floats and preserves half-period direction. Angles use typed reduction into `[-pi, pi)` without adding pi first; a local full-precision constant avoids the pinned standard module's double-rounded `pi_v<long double>`. Degree-conversion constants retain their previous double values; byte quantization retains saturation and truncation.
- `Line` accepts non-boolean arithmetic coordinates/positive dimensions; 2D constraints must not exclude integer/3D basics. `min()`/`max()` are oriented endpoints. Preserve tolerances, truncation, endpoint/degenerate behavior; boolean intersection shares the full-result predicate without unused outputs.
- `Matrix3` defaults to zero and owns nine unpadded row-major elements (`Mat3F` 36 bytes, `Mat3D` 72 bytes). It multiplies column vectors without homogeneous division. `rotate`/`translate`/`scale` left-multiply the complete matrix, including a general third row; `operator*=` right-multiplies with self-aliasing. JSON/Lua/DataStream retain row/byte order.
- Matrix constraints are per operation: integer basics remain valid, unchecked angles accept floating types including `long double`, and checked numerics use finite-classifiable `float`/`double`. Inverse/orthonormalization mutations commit only complete success, rejecting nonfinite/degenerate results without a global determinant cutoff or condition-number promise. Orthonormalization's relative residual tolerance defaults to `16 * epsilon`; finite nonnegative values, including zero, are valid. `is_orthogonal` checks the complete row Gram matrix, accepts reflections and rejects invalid tolerances. Use fast-math-safe `is_finite`.
- Mixed-scalar point/direction transforms promote arithmetic, converting only final coordinates to the input vector type; matrix/vector `operator*` keeps its matrix-scalar result. Angle transforms use the common floating type before final conversion. Lvalue row indexing borrows; rvalue indexing owns. Pack through `copy_to(span<T, 9>)`, never across nested row subobjects. GL stack packing updates transforms once per nonempty buffer draw, without cross-draw caching; empty draws retain error checks without uniform/binding updates. Backend changes need matched evidence, not packing-only benchmarks.

### Binary data and I/O

- VLQ adapters accept integral/enum values. Synchronous `deserialize*` borrows unchanged, retained `span<const char>` input; decoded fields own storage and serializers transfer output with `take_data()`.
- Prefer final `DataStreamReader` for contiguous decoding without read-ahead. Extensible buffers retain virtual `read_data` transformations/failures. Preserve container prefixes and complete-value commits on truncation; memory-device seeks, clone cursors and closed empty reads retain their own contracts, not file semantics.

### Images and pixel processing

- Check layout arithmetic in `size_t`, bounded by `PTRDIFF_MAX`; `Image::size_bytes` validates without allocation. `for_overwrite` requires writing every byte. Construction/changed-layout reset zero storage; unchanged-layout reset preserves pixels.
- Pixel spans borrow unique-owned storage; reallocation invalidates them. Batches borrow retained, non-null reference-wrapped operations. Confine libpng handles/callback pointers/row tables to RAII adapters; jump targets stay in trivial frames, with Image/row/diagnostic owners outside.
- Bound bulk ranges without weakening hardening or advancing beyond allocation. Preserve raw BGR order, source-over truncation, self-overlap's forward feedback and legacy float-format byte access. Scalers retain sampling/quantization/hidden RGB/tap behavior; validate actual allocated windows. Fuse color operations only when intermediate byte truncation/parity is preserved; keep HSV/replacement/reference barriers and crop ordering.

### Logging and native process output

- Filter before formatting, then recheck levels under the dispatch lock. File sinks report short writes without retries/rollback/sync; checked `make_shared` construction uses a private key. Preserve non-inserting spatial queries, newest-suffix queue caps and ownership-transferring drains.
- The SDL logger borrow exists only after startup and resets after application destruction/native callback shutdown. GUI entries create `NativeRuntime` with `StandardStreams::Optional` before initialization/workers. Repair only missing stdout/stderr with `NUL` or `/dev/null`, preserving redirection/errors, including valid CRT output without a Win32 handle. POSIX repair reserves the original descriptor; default `Inherit` keeps CLI output checked.
- `EmergencyLog` is best-effort reporting, not exit policy. On stderr failure, send the original message/error to `OutputDebugString` or syslog without recursion/blocking dialogs. Restore thread-local CRT invalid-parameter policy after recovery. Ordinary logger/file/`Star::print` errors remain checked.

## JSON values and formatting

- `Json` is immutable. Prefer `find`, `arrayPtr`/`objectPtr` and synchronous `iterateArray`/`iterateObject`; `toArray`/`toObject` copy mutable containers. Retain owners for borrowed children. Borrowing generation data must preserve random draws/object traversal order; not every snapshot can become a view.
- Use `jsonToList` for ordered conversion. Preserve missing versus null, source order and first-error behavior in field/default tables. UTF-8 parsing borrows unescaped tokens only during synchronous contiguous-input callbacks; escaped/generic input retains decoding, Unicode checks and error order.
- `FormattedJson::create` reuses semantic values; `createArray(span)` batches output while retaining comments/numeric spellings. Token indexing must not alter hashes/wire order. Path updates rebuild ancestors; patch groups commit on success. Searches keep the first match and defer value validation until it exists.

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

- `LuaConverter<T>` converts through `LuaStack`/`LuaStackValue`, including coroutines. Stack/string views borrow; retain with `own()` or owning Lua handles.
- Typed `void`, scalar, tuple and `LuaVariadic` calls request zero, one, tuple-N and all results; discard unused results before conversion.

### Context retirement and shutdown

- `LuaBaseComponent` owns execution/reload/shutdown. Retirement waits for the outermost call; join workers before dependent bindings disappear. Evaluation errors retain contexts; invocation/message errors disable them. Defer callback removal after reentrant `uninit` until hooks return, retaining host services.
- One-shot item scripts settle init/invoke/uninit under the Lua lock before committing changes. Cleanup errors stay primary with earlier failures as context; retirement must not resurrect failed uninit hooks.

## Lua module loading

- `require("/absolute/asset.lua")` runs in the caller's environment/coroutine and returns no values. Entry scripts/nested requires run once per context; only bytecode is shared. `_SBLOADED` is not a mutable cache hook.
- Cycles report the asset-path chain. Failed modules are retryable; successful dependencies remain loaded, without rollback of globals/native effects.
- Reload/hot reload and restart invalidate bytecode. Existing contexts keep loaded modules but need recreation to load new ones. Invalidation during loading rejects completion rather than mixing revisions.
- Require callbacks borrow `(LuaStackValue environment, StringView module)` and execute through `environment.stack().load(environment, bytes)`.
