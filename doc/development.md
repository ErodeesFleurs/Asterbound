# Development guide

[Project overview](../README.md) · [Build guide](building.md) · [Tests and benchmarks](testing.md) · [Engine notes](engine-notes.md)

## Tooling

- C++ uses named modules, `import std;` and `std::expected`, without RTTI or exceptions. Use fresh build trees after compiler, ABI or sanitizer changes.
- Configure clangd with `nix develop -c bash scripts/ide/configure-clangd.sh` and its generated compilation database; enable clangd 23's `--experimental-modules-support`.
- Check changed C++/module lines with the pinned formatter; use `--apply` to format or `--base <commit>` for a branch comparison:

```sh
nix shell --inputs-from . nixpkgs#python3 nixpkgs#clang-tools --command scripts/format-source.sh --check
```

## Threading and cancellation

Thread creation and joining retain a narrow native backend returning `std::expected`: project code disables exceptions, while `std::thread`/`std::jthread` report creation failures by throwing. Mutexes, condition variables and atomic waits use the standard library. The writer-preferring `ReadersWriterMutex` exposes standard locking operations for `std::shared_lock` and `std::unique_lock`; its multiple shared acquisitions by one thread and queued-writer priority are not replaced by `std::shared_mutex`.

Worker cancellation uses `std::stop_source`/`std::stop_token`, with stable tokens for each run and a new cancellation domain only after previous workers have joined. Terminal owners retain one lifetime domain. Stop requests still require explicit condition-variable notification or native I/O shutdown before joining, and derived owners join before destroying borrowed state. `Socket::close()` shuts down under shared ownership before waiting for exclusive descriptor release, so blocking send/receive cannot prevent their own shutdown. Worker pools retain checked startup rollback, queued-work cancellation versus draining, stored-lvalue task invocation and checked result publication.

## C++ implementation boundaries

- Keep non-template module definitions in implementation units with their own direct imports. Register those units as private CMake sources with `CXX_SCAN_FOR_MODULES` enabled; importing an interface does not expose its implementation dependencies.
- Reuse the checked `Star::transform` for owning transforms. `transformInto` appends and preserves the committed prefix on insertion failure; do not introduce eager capacity errors. Computed-key sorting constructs keys only after successful evaluation.
- Fixed ASCII enum labels use `sortedEnumNames`, `enumName<Names>` and `enumValue<Names>`. Retain canonical output spelling and case-insensitive input; runtime-extensible tables still need `EnumMap`.
- Qt connections use typed signals and an explicit receiver context. Keep `Q_OBJECT` and moc where custom signals or reflection require them, not merely for ordinary receiver methods.
- Payload-free alternatives use `std::monostate`. Preserve JSON/Lua nil, variant ordering, and network/inventory marker tags and zero-byte payloads; named inventory wrappers still distinguish swap and trash slots.
- Use `auto(expr)` for owning traversal snapshots and `std::views::zip` for synchronous read-only iteration over retained ranges. Pane dismissal callbacks may modify the original collection, so these snapshots must not become borrowed views.
- Typed sequence `contains` converts its argument to the element type before `std::ranges::contains`; replacing the member call with an untyped range query can change mixed-type membership.
- Use `std::clamp` only with proven ordered bounds and value consumption. Legacy `Star::clamp` deliberately returns the upper bound for reversed ranges. Preserve vector interpolation, integer rounding, wrapped coordinates and fast-math-safe finite classification rather than replacing their arithmetic wholesale.
- Byte-order scalars use the `byte_order_value` concept and `to_byte_order` / `from_byte_order`; `to_big_endian` and `to_little_endian` are encoding conveniences, while decoding uses `from_byte_order`. Only padding-free integrals and 32/64-bit floating representations are accepted; aggregates and pointers are not serialized by reversing their object representation. Raw `reverse_bytes` / `reverse_bytes_copy` support unaligned storage and null zero-length ranges; copy/conversion ranges must be disjoint or identical, never partially overlapping.
- `Line` uses `line_scalar` (non-boolean arithmetic coordinates) and positive dimensions. Two-dimensional operations use direct dimension constraints; integer storage and three-dimensional basic operations remain available. Use `make_positive`, `line_projection`, `distance_to`, `set_center` and snake_case reflection helpers. `min()` / `max()` are oriented endpoints, not componentwise bounds. Boolean intersection shares the full-result predicate but skips point, parameter-output and glancing-result construction. Endpoint order, tolerances, integer truncation and legacy degenerate-line behavior remain unchanged; fixes to those geometry contracts must be reviewed separately.
- DataStream uses snake_case APIs (`read_data`, `write_data`, `read_container`, `read_map_container`, `read_vlq_u`) and operation-based `stream_*` concepts. VLQ scalar adapters accept integral and enum values. Synchronous `deserialize*` helpers borrow `span<const char>` input; keep its owner alive and unchanged until decoding returns. Decoded fields still own their storage, and serializers transfer completed output with `take_data()`.
- Prefer the final `DataStreamReader` for contiguous read-only decoding. It batches VLQ byte inspection without reading ahead; extensible `DataStreamBuffer` and `DataStreamExternalBuffer` retain virtual `read_data` behavior, including subclass transformations and failures. Preserve accepted container prefixes and complete-value commits on truncated input. Memory-device end-relative seek, clone cursors and closed empty reads retain their existing contracts rather than being unified with files.
- Logging uses snake_case APIs (`add_sink`, `set_level`, `log_exception`, `log_point`) and constrained formatting/spatial helpers. Accepted calls retain an immutable sink registry snapshot; registration changes affect later calls, and removed sinks remain alive for in-flight delivery. Registry locks are released before formatting and callbacks. An independent recursive dispatch lock preserves message ordering across all sinks of one Logger while allowing callback registration/removal and same-thread nested logging. Custom sinks shared across Loggers must synchronize themselves, as before; built-in file/stdout sinks already do. Callbacks must not wait on another thread dispatching to the same Logger.
- Filter before formatting without retaining a snapshot for rejected messages, then recheck current levels under the dispatch lock. File sinks reject short writes without adding retries, rollback or durability sync. Spatial queries do not insert missing spaces; capped queues retain the newest suffix, and draining transfers ownership.

## JSON values and formatting

- `Json` is immutable. Prefer `find`, `arrayPtr` / `objectPtr` or `iterateArray` / `iterateObject` for synchronous reads; `toArray` / `toObject` make mutable container copies. A borrowed child must not outlive its retained owner.
- Borrow read-only generation arrays without changing random draw order. Explicit monster part overrides still consume their normal random part draw; NPC equipment retains its object snapshot and slot iteration order. Rebuilding a hash table can change traversal order, so object copies cannot always be replaced by borrowed iteration.
- Reuse `jsonToList` for ordered item conversion. Preserve each field's missing/null default separately; a present `example: null` is not an absent example.
- Contiguous UTF-8 input borrows unescaped tokens during synchronous callbacks; escaped strings and generic iterators retain the decoded path. Unicode validation, parser error order and missing/null distinctions remain unchanged.
- `FormattedJson::create` reuses the immutable semantic value. Use `createArray(span)` for batch output instead of repeatedly appending immutable arrays; child comments and numeric spellings are retained. Its private token index does not change the global JSON hash or wire ordering.
- Path updates still rebuild the immutable ancestor spine. Asset patch groups commit only on success, and search keeps the first match and defers value validation until a match exists.
- Configuration field tables may group only reads with the same type/default policy. Preserve source order, first-error behavior and missing/null distinctions when extracting domain readers.

## UI and rendering preparation

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

## Lua embedding

- `LuaConverter<T>` converts directly through `LuaStack` / `LuaStackValue`, including coroutine callbacks. Stack and string views are borrowed; retain values with `own()` or owning Lua handle types.
- Typed calls request zero, one, tuple-N or all results for `void`, scalar, tuple or `LuaVariadic` returns. Unused results are discarded before conversion.
- `LuaBaseComponent` owns session execution, reload and shutdown. Context retirement waits for the outermost call; workers are joined before dependent bindings disappear. Evaluation errors retain the context; invocation/message errors disable it.
- Callback removals requested after a reentrant `uninit` wait until its shutdown hooks return, so those hooks retain their host services.
- One-shot item scripts explicitly settle init/invoke/uninit under the Lua lock before committing item changes. Cleanup errors remain primary with earlier failures as context; retiring a failed context must not resurrect its uninit hook.
- Require callbacks take borrowed `(LuaStackValue environment, StringView module)` and execute through `environment.stack().load(environment, bytes)`.

## Lua module loading

- `require("/absolute/asset.lua")` executes in the caller's environment and coroutine, returning no values. Entry scripts and nested requires execute once per context; only bytecode is shared between contexts. `_SBLOADED` is no longer a mutable cache hook.
- Cycles report the asset-path chain. Failed modules can be retried; successful dependencies remain loaded. Global changes and native side effects are not rolled back.
- Reload/hot reload and engine restart invalidate cached bytecode. Old contexts retain loaded modules but must be recreated to load new ones. Invalidation during loading rejects completion rather than mixing revisions.
