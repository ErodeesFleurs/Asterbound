# Engine behavior and implementation notes

[Project overview](../README.md) · [Development guide](development.md) · [Tests and benchmarks](testing.md) · [Mod compatibility and diagnostics](modding.md)

Use this page to understand observable engine behavior and runtime ownership.
For implementation rules, see the [development guide](development.md); for test
commands and measurement limits, see [tests and benchmarks](testing.md).

- [Parametric table thresholds](#parametric-table-thresholds)
- [Subsystem behavior and upstream sources](#selected-upstream-optimizations-and-fixes)
  - [Image processing](#image-processing)
  - [Asset and texture caches](#asset-and-texture-caches)
  - [Gameplay catalogs and physics](#gameplay-catalogs-and-physics)
  - [Text layout](#text-layout)
- [Audio playback](#audio-playback)
- [Network and frame preparation](#network-and-frame-preparation)
- [Independent simulation and rendering](#independent-simulation-and-rendering)
  - [Thread ownership](#thread-ownership)
  - [Display rate and scheduling](#display-rate-and-scheduling)
  - [Presentation and tick-owned effects](#presentation-and-tick-owned-effects)
  - [Command transport and GPU resources](#command-transport-and-gpu-resources)
  - [Shutdown order](#shutdown-order)

## Parametric table thresholds

`ParametricTable::get` selects the value **strictly below** the query, clamping
to the first value if there is no lower index. For thresholds `1, 5, 10`, a
query of `5` still selects the value at `1`. Bulk `create` rejects duplicate
indexes; incremental `insert` places a duplicate after earlier entries at the
same index. Just above that threshold, the last duplicate supplies the value.

`index`, `value` and `get` borrow from an lvalue table. Do not retain their
results across mutation or destruction. In-place inversion requires matching
index and value types and strictly increasing values; failure leaves the table
unchanged. For a different specialization use `Target::convert(source)`, which
reports invalid conversions and preserves interpolation and bound modes for
functions.

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
- [Referenced-image caching](https://github.com/OpenStarbound/OpenStarbound/commit/3f33581f) keeps recently used blend/mask images longer. Boot `assetsSettings.referenceImageTimeToLive` defaults to 300 seconds and `referenceImagePixelBudget` to 16777216 logical pixels (64 MiB for RGBA32, not a total-cache memory cap). Actual reference cache hits refresh recency; newest references fitting the budget receive the extended lifetime. A zero budget or reference TTL no greater than `assetTimeToLive` disables the extension. Ordinary assets keep their normal TTL; `Assets::clear_cache` and Root reload still evict unheld references immediately, while externally held images retain normal persistence.
- [Vanished loose patches](https://github.com/OpenStarbound/OpenStarbound/commit/8852bada) are skipped with a warning only after a failed read confirms that their directory entry is absent. This applies to JSON and Lua image patches; unreadable or replaced files, dangling symlinks, packed-source read failures and JSON/Lua errors still fail the asset load.
- Unix file opens inspect the acquired descriptor and reject directories with a recoverable I/O error. A patch replaced by a directory fails asset loading instead of treating a directory seek position as a buffer size; only confirmed vanished patches are skipped.
- WOFF2 font sources decode to shared, immutable SFNT bytes once. Text painters retain independent FreeType faces and pixel sizes without repeating WOFF2 decompression. The asset cache retains a source while clones share its backing bytes; after the last clone is released, normal cache expiry applies.

### Gameplay catalogs and physics

- Bag and custom action-bar indices are zero-based bytes: a 256-slot bag ends at index 255. JSON and Lua reject an out-of-range index rather than wrapping to another item. Bag names cannot be the JSON slot discriminators `equipment`, `swap`, or `trash`. Equipment names such as `headCosmetic` and essential-tool save keys such as `beamAxe` retain their original spelling.
- Splitting an overfilled item removes only what the new stack can hold. A partially collected drop stays available with its remaining count. Merchant quantities are packs: three packs of five with a stack limit of ten grant ten plus five, and `onBuy` reports fifteen items.
- Ground drops merge only when the whole source stack fits. The combine radius uses wrapped world distance, including across the horizontal seam. The receiver replicates its new count.
- When a drop's item ages into a different item, its Lua `item` callbacks read the replacement and rendering discards the previous item's image and layer. A failed replica replacement leaves the previous item visible and can retry without another copy of the same delta.
- Native entity implementations override `meta_bound_box()`. Lua userdata still exposes `entity:metaBoundBox()`, and asset definitions still use the JSON field `metaBoundBox`.
- Bags fill compatible existing stacks in ascending slot order before placing the remainder in the first vacancy. Overfilled destinations have no spare capacity. Reading an incomplete container snapshot leaves the previous slots and item identities intact.
- Item descriptors keep distinct parameter variants as separate recipe and blueprint keys. A truncated descriptor on the wire leaves the previous item intact. A nameless descriptor is `null` in ordinary JSON but still carries its count and parameters in a versioned `Item` save.
- The Lua callbacks `player.consumeTaggedItem` and `player.consumeItemWithParameter` remove units from the stacks that matched, not from an earlier stack of the same name. Liquid collection rejects an overflowing item payout before draining or notifying clients.
- Grouped crafting containers ignore vacant input slots. Among craftable recipes that use every supplied material type, they select the largest total ingredient count, then the lexicographically first recipe asset path. A missing or zero-count output is not craftable.
- A recipe's currency requirements are unordered; its item inputs are ordered. Lua and assets keep the keys `currencyInputs` and `matchInputParameters`. An omitted or null `duration` uses `defaultCraftDuration`; an explicit duration, including zero, does not need that default.
- Containers reserve the last slot for output and cannot spend player currency. Each completed batch checks all input slots together, using exact parameters when requested. A missing ingredient leaves every stack unchanged, including earlier ingredients in the same recipe. Scripted containers still call `craftingRecipe` each crafting tick.
- One crafting-pane refresh uses the same inventory and currency snapshot for the material filter, rows, quantity limits, and buttons. Crafting itself checks the live inventory again before consuming anything.
- Shared item construction distinguishes a requested seed from a seed generated by the Lua builder. A generated seed does not claim the cache entry for a later explicit request. A malformed replicated cosmetic save does not clear the slot or acknowledge its revision; a corrected value can retry that revision.
- Entity interaction scripts return an action name or `[name, data]`; additional array entries are ignored. An object without a script result uses its configured action. A vehicle without a script action chooses the nearest enabled lounge position, retaining the first enabled seat on a tie. A vehicle with no enabled seats returns no action.
- Player interaction results are delivered in enqueue order among ready actions. A pending action does not block later ready actions. If observing an action fails, the batch remains queued for retry.
- [Known blueprint relearning](https://github.com/OpenStarbound/OpenStarbound/commit/9814f594) without a failure notification no longer constructs an Item or loads notification assets. New unlocks still validate Item construction before changing blueprint state.
- [Damage kind names](https://github.com/OpenStarbound/OpenStarbound/commit/b37ef403) are case-insensitive at insertion and lookup, including duplicate detection. Elemental type names remain case-sensitive and are validated against their own catalog.
- [Legacy humanoid back sleeves](https://github.com/OpenStarbound/OpenStarbound/commit/75eb1c9d) retain the armor's fullbright setting while holding items.
- `ArmorWearer` maps extra cosmetic slots `0–11` to physical slots `8–19`; invalid indices never overwrite functional armor. A facing change updates only visible garments with flip directives. Appearance caching does not suppress per-tick effect emission. Missing armor save keys leave existing slots unchanged.
- Behavior modules may appear in separate branches; a module referencing one already on its expansion path is a load error. `Dynamic` and `Randomize` require at least one child. Lua actions and decorators receive an opaque numeric node ID, not an address. Shared blackboards publish parameter subscriptions only after a successful bind; a behavior borrowing a board does not expire its owner's temporary outputs.
- An empty biome distribution places nothing. Periodic placement preserves the original float rounding and truncation toward zero; coordinates outside the representable integer range return an error. Tree candidates retain their configured order and duplicates (including leafless entries) so the same seed selects the same pair. Biome saves retain both distribution payloads, even the inactive one; material IDs, mod IDs, and hue bytes must fit their stored widths.
- Celestial IDs round-trip for systems, planets, and moons. Navigation requests keep their in-flight timeout when the same system is queried again; checking whether a region is loaded still requests every missing chunk. Chunk persistence is published to the master's cache only after storage accepts the write. Orbit generation draws occupied orbits before generating any bodies, preserving existing universe seeds.
- The chat router reserves `server` and resolves nickname collisions before updating either index. Invalid whispers produce neither a sender echo nor a command side effect; valid self-whispers retain both deliveries. Queued client input and command replies belong to the originating session, not the numeric ID that a later connection might reuse.
- A client context takes its main connection ID from `ConnectSuccess`, not from the first `WorldStart`. Its six replicated fields retain their original wire order; only protocol 15 and newer carry custom-world chunks. A rejected context frame does not publish partial chunks or fields.
- A server context retains its encoded update until the local connection queue accepts it. A retry sends that frame before later changes; queue acceptance is not a remote delivery acknowledgment. On the client, ship and custom-world changes stay pending until their respective writes succeed. A failed disconnect save retains the updates for retry without exposing the retired session as active.
- Known codices retain the edition learned by the player across catalog reloads. The reader polls the player library identity and revision; it fetches list summaries only when the library changes and loads the selected book's pages separately. The save format remains an object mapping codex IDs to read flags.
- Reusing a known codex item marks the book unread. A missing codex does not produce an “already known” message, and a malformed notification does not change the library.
- A collection unlock validates the entry and prepares its notification before recording progress. A failed unlock can be retried; a duplicate does not notify again. Item and monster entries resolve on demand, outside the collection cache lock. A relative inventory icon belongs to the resolved item, so looking it up never reruns its Lua builder.
- [Physics-object force bounds](https://github.com/OpenStarbound/OpenStarbound/commit/b66ad015) remain in object-local coordinates for spatial indexing, including at large world positions. Returned force regions still use world coordinates.

### Text layout

- [Chat bubbles](https://github.com/OpenStarbound/OpenStarbound/commit/9f412531) measure text with the final per-message style and the same scaled wrap positioning used for drawing, keeping custom font sizes inside their backgrounds.
- The bundled chat pane stores received messages oldest first, but still draws the newest message at the bottom. Unversioned pane checkpoints are converted when opened. The separately persisted input history keeps its original order.
- Chat packet field order and mode ordinals are unchanged. A truncated v4/v5 message leaves the destination unchanged; reading v4 into a reused message clears v5 metadata. A supplied JSON `fromConnection` must be an exact integer in `0–65535`, not a value to truncate or wrap.
- Native [right-aligned TextBox overflow](https://github.com/OpenStarbound/OpenStarbound/commit/be262918) scrolls toward the cursor correctly. The existing cursor-offset calculation and Lua-managed chat input are unchanged.

## Audio playback

Instance volume is a linear amplitude applied once, alongside global/group and
positional gains. This corrects the former squared instance gain: values between
zero and one can therefore sound louder than before. Pitch ramps now advance
across callback buffers rather than restarting from the same value.

Global speed zero pauses Effects/Instruments cursors without finishing them;
Music/Cinematic remain speed-independent. Volume/pitch ramps and wall-clock
start/stop scheduling continue during this pause. Empty infinitely-looping clips
finish instead of blocking the audio callback.

Uncompressing a Vorbis clip preserves its complete duration and current frame,
including an EOF cursor. Failure retains the original decoder and position.
The moving-average low-pass now uses exact integer sums; its quantization can
differ from the former float-normalized filter by one int16 LSB. Echo retains
its processed-output feedback. Mixer voice order and per-voice clipping remain
unchanged.

## Network and frame preparation

When a returning player joins, the server requests the star chart summary for
the chunk containing the saved ship position and the orbital details for the
original system. No save conversion is needed. If a celestial request fails,
the server logs it and still delivers the other completed responses; it does
not substitute an empty chunk for unreadable data.

### Entity network clock

`entityUpdateDelta`, `stepLead`, and `stepMaxDistance` are in legacy 60 Hz steps, regardless of simulation or display rate. Disabling interpolation disables clock tracking and extrapolation, not entity update transmission. A new tracker returns zero lead until its first heartbeat has been followed by a logic update. Once synchronized, a heartbeat changes the lead used by subsequent packets immediately; smoothing and distance limiting run on the next logic update.

### Buffer and snapshot ownership

Entity creation encoders transfer completed buffers instead of copying them. Read-only RPC and monster/NPC variant decoders borrow their input only during decoding; decoded fields remain owned. Network formats and compatibility versions are unchanged. Frame data owns reusable entity collections and particle snapshots; painters borrow them only during submission. Typed spatial queries collect a single owned result list without changing traversal order, wrapping, deduplication or insertion-safe candidate snapshots.

### Team status frequency

Team updates share one configuration snapshot within each update, without caching it across updates. Health, energy, position and travel state retain the configured status frequency. Names and portraits are included on the next eligible status update after ten seconds, or immediately on the next eligible update after first joining, changing teams or `forceUpdate`. Appearance changes can therefore reach other teammates later than dynamic status. The full-update timer advances only after successful local submission; encoding or enqueue failures do not consume it. Legacy RPC still maps remote failures to `null`, so submission is not a remote-delivery acknowledgment.

## Independent simulation and rendering

### Thread ownership

SDL event polling, window operations and both native graphics backends stay on the main thread. `ClientLogic` exclusively owns client-world updates, client Lua, UI and CPU draw preparation. Main consumes an owned command frame while the worker can run subsequent fixed ticks; the next draw preparation waits for presentation acknowledgment so ImGui is never mutated concurrently. This is backend/logic parallelism, not concurrent access to the live world or parallel Lua execution. Command capture and handoff add CPU and memory costs; higher uncapped FPS is not guaranteed.

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

Quad staging packs the four original vertices once, then copies the two repeated
packed vertices into the existing six-vertex triangle order. Rounding flags still
use each original outside neighbor; polygons retain their triangle-fan rules.
This changes neither upload layout nor draw ordering.

OpenGL effect-image streaming retains `glTexImage2D`: replacing it unconditionally with
subimage updates can add overwrite waits instead of improving throughput.
BGR/BGRA describe source channel order; their storage uses RGB/RGBA internal
formats. Float images retain RGB32F/RGBA32F storage.

Detached views borrow spans and a required typed reference to main's texture table, not an opaque context or lookup callback; the backend consumes them synchronously without retaining those references. Command builders return checked references. Synchronous main-thread requests remain stack-owned, linked through optional references and executed through a typed interface without queue heap allocations. GPU staging exposes data pointers only at native graphics C ABI boundaries.

### Native backend lifetimes

GUI clients include both OpenGL and Vulkan; there is no separate Vulkan build
switch. Vulkan is the startup default. `NativeRenderer` supplies the main-owned
frame, resize and detached-geometry interface consumed by `ThreadedRenderer`.
The runtime selection and explicit shader-variant contract are documented in
[renderer selection](building.md#renderer-selection).

The single sealed CPU command frame is independent of Vulkan's two fenced GPU
frames. Resetting a transport slot or acknowledging presentation never permits
overwriting GPU storage still in use. GPU frames retain texture/atlas allocations,
image and effect generations, descriptors, upload slices and retained-buffer
generations until their fences complete. Reusable buffer generations keep their
capacity without changing bytes referenced by earlier draws.

Vulkan keeps game coordinates and framebuffer sampling in the existing
bottom-left convention, then flips once when copying to the presentation image.
ImGui renders afterward in native window coordinates. Normal out-of-date or
zero-sized presentations retire CPU commands without acknowledging a displayed
camera or changing input capture. Hidden/minimized Vulkan windows suspend draw
preparation, not simulation; restoration wakes the owner.

Vulkan requires the 1.3 dynamic-rendering API, while runtime shaders target the
Vulkan 1.2 / SPIR-V 1.5 baseline so ordinary `discard` does not require optional
1.3 shader-demote features. MSAA sample counts and sample-rate shading, float
image filtering and attachment formats are checked against device capabilities.
The legacy `hdr` setting controls floating-point intermediate targets, not monitor
HDR. `renderOutput` separately selects legacy-compatible output, linear-scene SDR
tone mapping, or Vulkan HDR10; see [output settings](building.md#output-color-and-hdr10).

Linear output keeps scene and postprocess buffers floating-point. Byte sprite
textures use sRGB sampling views so decoding precedes hardware filtering; vertex
colors are decoded before interpolation, while float lighting textures remain
linear. Lighting values above one reach the final scene-to-interface boundary
without the legacy light-map shoulder. Scene exposure and a luminance-preserving
highlight shoulder are applied once there, with highlight desaturation only when
needed to fit the output gamut.

SDR encodes that scene to sRGB before the existing SDR UI. HDR10 instead composes
ordinary UI and ImGui in linear paper-white-relative space, then converts BT.709
to BT.2020 and encodes ST2084 PQ into an advertised 10-bit HDR10 swapchain. HDR
metadata is submitted when the driver supports it; MaxFALL remains unspecified,
not an invented measurement. Mode changes replace resources transactionally
between frames; rejected requests preserve the old output and configuration.
Exposure and luminance-only changes do not recreate the swapchain.


### Visual lighting buffers

Client lighting collection and propagation remain CPU-owned. Pending, completed
and displayed float maps exchange owned storage rather than allocating and zeroing
a new output every calculation. Capacity is retained for reuse; the calculator
overwrites every channel before publishing. A separate ready bit distinguishes a
fresh result from recycled storage, and a completed result is consumed only once.
World reset drains propagation and clears readiness before releasing world state.
Synchronous calculations serialize with the asynchronous worker.

Render-side borrowing ends before the displayed map is exchanged; the threaded
renderer owns its image copy before returning. Buffer reuse does not change the
spread/point algorithms, liquid correction, or independent CPU `world.lightLevel`
queries used by gameplay and headless servers.

### Shutdown order

Shutdown stops audio callbacks and client services first, releases SDK/GL owners
while the Root logger is alive, destroys the application on its worker, then joins
and tears down ImGui/SDL on main. For worker cancellation and borrowed-state
lifetime rules, see [threading and cancellation](development.md#threading-and-cancellation).
