# Engine behavior and implementation notes

[Project overview](../README.md) · [Development guide](development.md) · [Tests and benchmarks](testing.md) · [Mod compatibility and diagnostics](modding.md)

Use this page to understand observable engine behavior and runtime ownership.
For implementation rules, see the [development guide](development.md); for test
commands and measurement limits, see [tests and benchmarks](testing.md).

- [Subsystem behavior and upstream sources](#selected-upstream-optimizations-and-fixes)
  - [Image processing](#image-processing)
  - [Asset and texture caches](#asset-and-texture-caches)
  - [Gameplay catalogs and physics](#gameplay-catalogs-and-physics)
  - [Text layout](#text-layout)
- [Network and frame preparation](#network-and-frame-preparation)
- [Independent simulation and rendering](#independent-simulation-and-rendering)
  - [Thread ownership](#thread-ownership)
  - [Display rate and scheduling](#display-rate-and-scheduling)
  - [Presentation and tick-owned effects](#presentation-and-tick-owned-effects)
  - [Command transport and GPU resources](#command-transport-and-gpu-resources)
  - [Shutdown order](#shutdown-order)

## Selected upstream optimizations and fixes

These notes are grouped by subsystem, not by patch date. Linked OpenStarbound
commits identify upstream sources; they are not cross-platform validation reports.

### Image processing

- Image directives cache repeated hue/saturation/brightness conversions, skip exact neutral parameters, restrict blends to overlapping pixels, and prefilter large color replacements. Existing non-neutral color math is unchanged; neutral HSV directives preserve input bytes after any required RGB/BGR-to-alpha conversion. See upstream [color caching](https://github.com/OpenStarbound/OpenStarbound/commit/0cfbeb5f), [regional processing](https://github.com/OpenStarbound/OpenStarbound/commit/c4e5b17a) and [repeated replacements](https://github.com/OpenStarbound/OpenStarbound/commit/e1d575cd).
- [Nearest-neighbor scaling](https://github.com/OpenStarbound/OpenStarbound/commit/334ad9fb) precomputes source-column offsets and selects the source row once per output row for byte images. Sampling still divides before rounding and clamps to the source edges; channel order, transparent RGB and the existing float-image behavior are preserved.
- [Scale/crop fusion](https://github.com/OpenStarbound/OpenStarbound/commit/c4e5b17a) computes only the retained window for eligible nearest/bilinear scales, including across directive groups. Invalid windows, negative scales, bicubic scales and float-image pipeline operations retain sequential execution.
- [Color-operation scheduling](https://github.com/OpenStarbound/OpenStarbound/commit/549af71b), including the [empty-image fix](https://github.com/OpenStarbound/OpenStarbound/commit/f097a32d), moves eligible pixel-local operations before an enlarging nearest scale or after a shrinking crop. Both directive execution entrypoints preserve output pixels, earlier execution errors and successfully applied prefixes before later errors.
- [Dirty-region replacement](https://github.com/OpenStarbound/OpenStarbound/commit/6fe1c895) restricts a replacement after consecutive blends to their affected union only when a preceding replacement removed its keys and the new key set is a qualifying subset. Other operations invalidate this shortcut.

### Asset and texture caches

- [Texture cleanup](https://github.com/OpenStarbound/OpenStarbound/commit/21e584d2) spaces full scans by one tenth of the timeout, capped at one second. Expired textures can remain until the next scan; Root reloads still clear the group immediately.
- [Referenced-image caching](https://github.com/OpenStarbound/OpenStarbound/commit/3f33581f) keeps recently used blend/mask images longer. Boot `assetsSettings.referenceImageTimeToLive` defaults to 300 seconds and `referenceImagePixelBudget` to 16777216 logical pixels (64 MiB for RGBA32, not a total-cache memory cap). Actual reference cache hits refresh recency; newest references fitting the budget receive the extended lifetime. A zero budget or reference TTL no greater than `assetTimeToLive` disables the extension. Ordinary assets keep their normal TTL; `clearCache` and Root reload still evict unheld references immediately, while externally held images retain normal persistence.
- [Vanished loose patches](https://github.com/OpenStarbound/OpenStarbound/commit/8852bada) are skipped with a warning only after a failed read confirms that their directory entry is absent. This applies to JSON and Lua image patches; unreadable or replaced files, dangling symlinks, packed-source read failures and JSON/Lua errors still fail the asset load.
- Unix file opens inspect the acquired descriptor and reject directories with a recoverable I/O error. A patch replaced by a directory fails asset loading instead of treating a directory seek position as a buffer size; only confirmed vanished patches are skipped.
- WOFF2 font sources decode to shared, immutable SFNT bytes once. Text painters retain independent FreeType faces and pixel sizes without repeating WOFF2 decompression. The asset cache retains a source while clones share its backing bytes; after the last clone is released, normal cache expiry applies.

### Gameplay catalogs and physics

- [Known blueprint relearning](https://github.com/OpenStarbound/OpenStarbound/commit/9814f594) without a failure notification no longer constructs an Item or loads notification assets. New unlocks still validate Item construction before changing blueprint state.
- [Damage kind names](https://github.com/OpenStarbound/OpenStarbound/commit/b37ef403) are case-insensitive at insertion and lookup, including duplicate detection. Elemental type names remain case-sensitive and are validated against their own catalog.
- [Legacy humanoid back sleeves](https://github.com/OpenStarbound/OpenStarbound/commit/75eb1c9d) retain the armor's fullbright setting while holding items.
- [Physics-object force bounds](https://github.com/OpenStarbound/OpenStarbound/commit/b66ad015) remain in object-local coordinates for spatial indexing, including at large world positions. Returned force regions still use world coordinates.

### Text layout

- [Chat bubbles](https://github.com/OpenStarbound/OpenStarbound/commit/9f412531) measure text with the final per-message style and the same scaled wrap positioning used for drawing, keeping custom font sizes inside their backgrounds.
- Native [right-aligned TextBox overflow](https://github.com/OpenStarbound/OpenStarbound/commit/be262918) scrolls toward the cursor correctly. The existing cursor-offset calculation and Lua-managed chat input are unchanged.

## Network and frame preparation

### Buffer and snapshot ownership

Entity creation encoders transfer completed buffers instead of copying them. Read-only RPC and monster/NPC variant decoders borrow their input only during decoding; decoded fields remain owned. Network formats and compatibility versions are unchanged. Frame data owns reusable entity collections and particle snapshots; painters borrow them only during submission. Typed spatial queries collect a single owned result list without changing traversal order, wrapping, deduplication or insertion-safe candidate snapshots.

### Team status frequency

Team updates share one configuration snapshot within each update, without caching it across updates. Health, energy, position and travel state retain the configured status frequency. Names and portraits are included on the next eligible status update after ten seconds, or immediately on the next eligible update after first joining, changing teams or `forceUpdate`. Appearance changes can therefore reach other teammates later than dynamic status. The full-update timer advances only after successful local submission; encoding or enqueue failures do not consume it. Legacy RPC still maps remote failures to `null`, so submission is not a remote-delivery acknowledgment.

## Independent simulation and rendering

### Thread ownership

SDL event polling, window operations and OpenGL stay on the main thread. `ClientLogic` exclusively owns client-world updates, client Lua, UI and CPU draw preparation. Main consumes an owned command frame while the worker can run subsequent fixed ticks; the next draw preparation waits for presentation acknowledgment so ImGui is never mutated concurrently. This is backend/logic parallelism, not concurrent access to the live world or parallel Lua execution. Command capture and handoff add CPU and memory costs; higher uncapped FPS is not guaranteed.

For CPU accounting, A/A controls and limits on performance claims, see
[runtime performance evidence](testing.md#runtime-performance-evidence).

### Display rate and scheduling

`renderFpsLimit` defaults to `0` (unlimited). A finite positive value caps display submissions, including fractional limits such as `117.5`; negative, non-finite and out-of-range values are rejected. The graphics menu stages the FPS limit and VSync until **Apply**. **Back** discards them, and the fullscreen shortcut does not commit pending settings. Existing custom limits are preserved without rounding to a slider preset. VSync can impose a lower effective display rate; unlimited does not override the display driver.

Each scheduler pass performs at most `maxFrameSkip + 1` due simulation ticks. Backward clock changes or more than 250 ms of accumulated debt resynchronize deadlines with one immediately due tick, avoiding unbounded catch-up. While a frame is in flight, the worker waits for logic deadlines independently of presentation; completion also wakes it. Native window/clipboard/SDK operations and ordered renderer queries or configuration changes remain synchronous main-thread barriers and can delay their caller.

### Presentation and tick-owned effects

World presentation samples the previous and current fixed-tick states, normally adding one simulation tick of visual latency without extrapolating gameplay. Entity positions, supported native animation transforms, particles and the camera share the display sample. Pausing displays the current state. World/player replacement, reloads, native teleports and large corrections invalidate presentation history; authored small teleports can call [`mcontroller.resetPresentation()`](lua/movementcontroller.md). Continuous movement must not reset it each tick.

Presentation alpha is sampled after the synchronous main-thread ImGui backend
handshake and renderer frame setup, immediately before application draw
preparation. Waiting for that setup does not freeze the sample at the earlier
render deadline. Fixed-update deadlines, the render cap and single-frame
backpressure are unchanged; subsequent preparation, backend work and presentation
can still add latency.

World and title-screen sky motion use the display alpha for epoch-based drift,
orbital motion, star/world offsets and wrapped rotations. Only displaying clients
retain the compact motion endpoints; headless simulation does not capture them.
Clock corrections, sky replacement, flight-phase changes and explicit world
presentation resets discard that history instead of sweeping across a jump.
Lighting, sky colors, audio and flight events remain tick-owned. Wind-driven
parallax integrals also advance only on ticks, but their direction, magnitude and
signed-time endpoints are sampled at the display alpha. Clock corrections and
presentation resets do not blend unrelated wind histories.

Nameplates and speech bubbles sample both their entity anchors and separation
offsets. Separation solving and offset tweening run only on ticks; render visits
are read-only. Newly created or replaced entities/bubbles start without stale
offset history. Camera presentation still aligns the tile grid to screen pixels;
higher sampling does not remove that spatial quantization.

Timers, one-shot audio/particle collection and beam transitions belong to ticks, including offscreen entities. Resource-cache jitter and native cosmetic noise use a separate presentation random service rather than consuming gameplay randomness. Display sampling does not write interpolated coordinates back to movement controllers. Camera conversions and overlay anchors use the displayed transform; input between frames retains the last displayed camera.

Lua pane `render()` may run several times without an intervening `update(dt)`, or less often than updates. Keep gameplay changes, timers, effect drains and unseeded gameplay-backed random draws out of render hooks. Rebuild ImGui widgets in the render hook only; the engine owns ImGui frame begin/end. See the [script pane lifecycle](lua/openstarbound/scriptpane.md#shared-script-pane-runtime) and [camera API](lua/openstarbound/camera.md). Arbitrary mod render hooks are not made read-only automatically.

### Command transport and GPU resources

Only one sealed command frame is in flight. Command storage owns copied pixels and vertices and uses numeric resource IDs; worker-side intrusive texture references never cross to main. Real graphics resources and platform SDK callbacks remain main-owned, including ImGui's clipboard and IME callbacks. Audio retains its existing callback-thread contract.

Main packs detached geometry directly into owned, typed GPU vertices; legacy primitives use the same packing kernel. Immediate segments remain batched until a real renderer flush, even across ordered queries and transport-slot reuse. Texture pins protect atlas coordinates while batches are pending, and reusable GPU buffers retain their allocated capacity. Command headers are separate from names, shader/configuration payloads and images: creation uploads are released after consumption, while recurring effect-image storage is reused and cleared at configuration reload.

Detached views borrow spans and a required typed reference to main's texture table, not an opaque context or lookup callback; the backend consumes them synchronously without retaining those references. Command builders return checked references. Synchronous main-thread requests remain stack-owned, linked through optional references and executed through a typed interface without queue heap allocations. GPU staging exposes data pointers only at the OpenGL upload boundary.

### Shutdown order

Shutdown stops audio callbacks and client services first, releases SDK/GL owners
while the Root logger is alive, destroys the application on its worker, then joins
and tears down ImGui/SDL on main. For worker cancellation and borrowed-state
lifetime rules, see [threading and cancellation](development.md#threading-and-cancellation).
