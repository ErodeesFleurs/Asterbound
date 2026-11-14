# Interface

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `interface` table contains bindings which allow scripts to display a message at the bottom of the screen, among other things.

---

#### `void` interface.queueMessage(`String` message, [`float` cooldown, [`float` springState]])

Queues a message popup at the bottom of the screen with an optional **cooldown** and **springState**.

---

#### `void` interface.setHudVisible(`bool` visible)

Sets the HUD's visibility.

---

#### `bool` interface.hudVisible()

Returns the HUD's visibility.

---

#### `PaneId` interface.bindRegisteredPane(`String` paneName)
Binds a native registered pane or a dynamically created string-ID pane to its pane/widget callbacks. Returns `nil` if no instance exists. Native identifiers are listed below; bundled Lua registrations use their configured string identifiers, such as `"chat"` and `"songbook"`.
<details><summary><b>Panes</b></summary>
Inventory<br>
Cockpit<br>
Tech<br>
Ai<br>
Confirmation<br>
JoinRequest<br>
Options<br>
WireInterface<br>
CraftingPlain<br>
MmUpgrade<br>
Collections<br>
</details>

---

#### `void` interface.displayRegisteredPane(`String` paneName)

Displays an existing instance. For a Lua registration without an instance, requests opening through the Lua interface manager.

---

#### `bool` interface.paneDisplayed(`String` paneName)

Queries an existing native or Lua-managed instance without opening it or
constructing its pane/widget callback table. Returns `false` without an
instance. This is lifecycle display state, not visibility or focus: a hidden
pane that has not been dismissed still reports `true`.

#### `bool` interface.paneVisible(`String` paneName)

Returns whether an existing instance is both displayed and drawing-visible.
Returns false without an instance; does not open it or allocate widget callbacks.

#### `void` interface.togglePane(`String` paneName)

Toggles a native registered pane through its native path (including plain
crafting), or a Lua registration through the manager. Lua ordinary dismissal
honors `retain`; this is not explicit instance destruction.

#### `Json` interface.travelState()

Returns `{beamUp, beamDown, deploy, teamShip}` booleans from the current native
travel eligibility checks. Native errors propagate.

#### `bool` interface.travel(`String` operation, [`String` memberUuid])

Operations are `"beamUp"`, `"beamDown"`, `"deploy"` and `"teamShip"`.
Rechecks current native eligibility, then uses the normal native warp path,
including teleport hooks and cinematic/world-transition handling. Returns false
when disallowed, true when submitted; this is not an arrival acknowledgment.
`teamShip` additionally requires the selected current member to have a
non-None warp mode and a supplied UUID string. Invalid UUIDs and unknown
operations raise errors. UI visibility cannot grant travel permissions.

#### `NativePortraitHandle` interface.portraitEntity([`EntityId` entityId])

Returns an opaque owning PortraitEntity userdata, or nil for a missing/nonportrait
entity. With no ID, uses the actual local Player, including while detached from
the world. Pass it to `widget.setPortraitEntity`; it is not drawable JSON and
cannot be serialized into `storage`.

#### `Json` interface.cursorView()

Returns `{position, roundedPosition, size, scale}` for the native cursor.
Positions are physical window pixels; `position` is the current native
mouse-move position and `roundedPosition` its integer counterpart. `size` is
the cursor image size before scaling and `scale` its effective native scale.
Do not replace a click event's actual position with this cached hover position.

#### `Vec2F` interface.cursorWorldPosition()

Converts the native cached mouse-move cursor through the real world camera.
This is not an alias for the player's aim position or a fresh button-event position.

#### `bool` interface.inventoryChanged()

Reads the existing Inventory pane's ItemGrid new-item markers. This does not
acknowledge or clear them and is not a second PlayerInventory change flag.

---

#### `CanvasWidget` interface.bindCanvas(`String` name, [`bool` ignoreInterfaceScale = false])

Binds the canvas widget on the main interface with the specified name as userdata for easy access. The `CanvasWidget` has the same methods as described in the [base widget API](../widget.md).

- **ignoreInterfaceScale** is used to ignore the current interface scaling and bind the canvas with the screen size.

---

#### `float` interface.scale()

Returns the scale used for interfaces.

#### `NativePaneHandle` interface.itemTooltip(`NativeItemHandle` item)

Internal client UI service used by script panes' native ItemGrid/ItemSlot
tooltip fallback. It accepts an engine-produced opaque item userdata, not an
item descriptor, and returns an owning opaque native pane userdata. The native
builder uses the current UI player, including player-dependent armor previews.
These handles are not JSON and cannot be persisted in script storage.
This is not a return type supported by the Lua `createTooltip` hook.

#### `void` interface.playCinematic(`Json` definition)

Immediately loads a resolved cinematic definition into the client UI overlay.
Native parsing/load errors propagate to the caller. Unlike the queued
`player.playCinematic` API, this does not defer playback, resolve an asset path,
or apply unique-playback history. Available in client UI script contexts.

#### `bool` interface.cinematicActive()

Returns whether the client UI cinematic overlay has not completed. This is
independent of player/world attachment and input suppression; a cinematic can
be active while the player is in a world.

#### `void` interface.returnToTitle()

Requests the client's normal return-to-title transition. Saving, disconnecting
and interface teardown remain owned by the native client lifecycle; this does
not terminate the application. Available while a game interface exists.

## Lua-managed interface instances

The session controller loads `/interface.config:panes` using the shared Lua
manager. Registrations, opening/closing, session/world scope, event dispatch and
semantic action mapping are Lua policy; `BaseScriptPane` has no HUD-specific
configuration or state. See [script pane runtime](scriptpane.md).

Load the public Lua helper with:

```lua
require "/interface/asterbound/ui/api.lua"

ui.register("example", {
  config = "/interface/example/example.config",
  layer = "Window",
  scope = "session",
  open = false
})
ui.open("example")
ui.send("example", "refresh", {reason = "externalChange"})
ui.close("example")
```

These helpers enqueue requests; they do not mutate the pane stack inside the
calling script. Native IDs and `"$ui"` are reserved. `ui.send` dispatches through
the registration's `events` map, or `onEvent(name, payload)` when unmapped.
Messages sent by handlers are deferred to a subsequent delivery boundary.
World-scoped events and instances are discarded at world or player changes.
Player-scoped instances survive world changes but are discarded when their
player changes or becomes unavailable; session instances retain standard
`storage` through capability rebinding.

### Bundled notifications, combat bars and main bar

The canonical session registrations `"Notifications"`, `"TargetHealth"`,
`"SpecialHealth"` and `"MainBar"` open on `"Background"` in that drawing order,
after the breath HUD and before later debug/named-canvas/Hud/window drawing.
Their assets live under `/interface/asterbound/notifications/`, `targethealth/`,
`specialhealth/` and `mainbar/`. Lua owns their presentation state; native
world entities, damage, inventory, travel and the renderer remain native.

Notifications receives `message` events with `{text, cooldown?, spring?}` and
`pickup` events with `{descriptor, name, count}`. The native pickup path resolves
the actual item; Lua merges matching descriptors, formats counts, manages
overflow/expiry and advances the configured spring when rendering. Session
storage retains pending messages across script reload and player/world changes.
`interface.queueMessage` feeds this same registration.

TargetHealth owns sticky target selection/countdown, last-damage fallback,
portrait fitting/clipping and bar layout. Domain views remain live, including
animated portraits. SpecialHealth owns discovery selection, interpolation and
spacing. Its live bars deliberately use stable first-admission order: surviving
identities retain order and new identities append. It does not reproduce the
old unordered native container's incidental hash-bucket order.

MainBar owns the configured button images, physical-pixel polygons, hover
priority, new-item glow, travel selection and pane toggles. It uses actual
`onInput.physicalPosition` for clicks without an interface-pixel round trip.
The separate automatically opened session `"MainBarTooltip"` registration
renders the cursor text on `"Tooltip"` from Lua, retaining physical cursor
geometry and legacy integer-scalar truncation. These are not native main-bar
buttons or a native cursor-tooltip presentation updater.

### Bundled action bar, team bar and radio

`"ActionBar"`, `"TeamBar"` and `"RadioMessagePopup"` are canonical, automatically
opened session `"Hud"` Lua registrations, not native pane identifiers or aliases.
Assets live in `/interface/asterbound/actionbar/`, `team/` and `radio/`. The retained
instances and original named children support cached pane/widget handles across
reload and player/world service rebinding; interface teardown ends them.

ActionBar owns selection/hover, top/bottom layout, wheel/key/click policy and
slot display. It uses actual opaque Item handles, so item counts, durability,
link relocation and player-aware tooltips retain native item/inventory ownership.
Its `tooltipItem` hook walks the intended slot geometry even under overlays.
Native keyboard/modal/widget capture priority still applies.

TeamBar owns local health/energy/food display, member rows, directions and
invitation/menu scheduling. `"TeamInvite"`, `"TeamInvitation"` and
`"TeamMemberMenu"` are session, retained `"Window"` registrations (not modal).
Incoming invitation data is consumed only on opening its dialog, retained
through reload, and submitted once on acceptance. Decline, Escape and actual
external dismissal discard the local payload without a decline RPC; a next
pending native invitation is not consumed while this dialog is displayed.
Member controls reflect current membership/leader/self/travel eligibility;
native client/server domain checks remain authoritative.

RadioMessagePopup owns portrait/text animation, stage timers, chat-height
placement and chatter scheduling. Hidden displayed instances keep ticking.
Lua is the sole destructive consumer of the native Player radio queue and
interrupt flag; cinematic gating delays consumption, not animation of the
current message. One dequeue produces one local chat record with origin 0.
Unique-message accounting happens at native dequeue, not visual completion;
there is no completion acknowledgment or replay on reload. Native resolution,
filtering/delay/deduplication, audio instances/mixing and queue ownership remain
native. Current presentation/timers/sound survive session rebind and reload.

### Team domain service

The centrally rebound `team` service exposes the real native TeamClient.
It is available to managed client UI instances with a team capability; it is not
window-owned, and network/RPC membership and permission decisions remain native.

* `team.isMemberOfTeam()` returns a boolean.
* `team.currentTeam()` returns the current team UUID string or nil.
* `team.isTeamLeader(playerUuid?)` checks the supplied UUID or the local player.
* `team.members()` returns an ordered array of
  `{uuid, name, entity, healthPercentage, energyPercentage, position, onWorld,
  canTravel, leader, self, portraitRevision}`. `position` is the native reported
  world position; `canTravel` means the member's warp mode is not None, not a
  guarantee that local travel is currently allowed. Portraits are not included.
* `team.memberPortrait(uuid, knownRevision?)` returns `{revision, portrait}`
  with native serialized Drawable definitions, or nil for an unknown member
  **or** unchanged revision. Omit the token for the full current portrait;
  an empty portrait array explicitly clears it. Use `members()` to distinguish
  membership disappearance. Tokens are process-local, not save data.
* `team.hasInvitationPending()` is a nondestructive boolean query.
* `team.pullInvitation()` consumes the pending flag and returns `{uuid, name}`
  for the inviter, or nil when none is pending. It does not accept or decline.
* `team.acceptInvitation(inviterUuid)` submits the native acceptance RPC; it
  does not consume another pending invitation or synchronously confirm membership.
* `team.invitePlayer(name)` submits the native invitation operation.
* `team.makeLeader(uuid)` requires the local player to be leader.
* `team.removeFromTeam(uuid)` requires leader status or removal of self.

UUID arguments are strings parsed by the native UUID parser; malformed values
raise errors. Leader/removal operations are no-ops without the applicable local
team permission and use the native RPC path when allowed. Return from a command
does not prove remote completion.


### Bundled status-effect HUD

The `"status"` session-scoped `"Hud"` registration loads
`/interface/asterbound/status/status.config` and its `status.lua` script. Lua owns
icon filtering, ordering, layout, duration masks and tooltip hit testing, using
the shared `BaseScriptPane` and Canvas widgets. Status application and lifetime
calculation remain in the native `status` service.

The script reads the existing `/interface.config` settings `statusIconPos`,
`statusIconShift` and `statusIconDarkenImage`. Iconless effects do not occupy
layout slots; mixed image sizes and overlapping icons are supported. Top-left
anchoring follows window resizing and interface scaling without a separate
screen-size tracker. All presentation state is derived from the current
`status.activeUniqueStatusEffectSummary()` and rebuilt on script initialization.
Patch the registered pane configuration or script to customize this HUD.

### Bundled breath HUD

The `"breath"` session instance loads `/interface/asterbound/breath/breath.config`.
Lua owns the ten-block display, rounded visibility threshold and zero-breath
warning flash; resource regeneration, depletion and damage remain native.
The existing `breathPos` and `breathBarPos` settings remain supported.

The generic `"Background"` pane layer renders before notifications and other
native HUD elements, below the later World/Hud/window stack. This preserves
the original breath ordering without a dedicated native breath host.
The pane uses `interactive: false` so its screen-sized Canvas does not capture
input. Its physical-pixel Canvas preserves the original centering and integer
icon-step behavior at fractional interface scales.

### Bundled bookmark editor

The `"bookmarkEditor"` world-scoped `"ModalWindow"` registration loads
`/interface/asterbound/bookmarks/bookmarkeditor.config`. Its `edit` event accepts
`{bookmark, sourceEntityId, sourceUniqueId?}`; bookmark data uses the canonical
JSON returned by `player.teleportBookmarks()`. Native flag/teleporter
interactions and teleport-list edit buttons deliver this event through the
generic interface bridge, without a native editor class.

Lua owns new/edit mode, stored-name lookup, icon/target labels, focused name
editing, empty-name fallback, confirmation, deletion and cancellation.
Identity is the bookmark's target, never its name or current list index.
Persistence uses the existing player bookmark APIs. The original asset's
textbox naming constraints remain in effect.

The editor preserves its draft across script reload, but closes when its
interaction source disappears or its player/world context changes. World-scoped
instances and queued events are discarded on either player or world generation
changes. Teleport eligibility, reachability and warp execution remain native.

### Bundled ordinary popup

The `"popup"` registration uses the ordinary `"Window"` layer and `"player"`
scope. Its `show` event accepts `{message, title?, subtitle?, sound?}` through
the generic event bridge; native `ShowPopup` interactions use this same path.
Lua uses the original `/interface/windowconfig/popup.config` layout and sound
list, and the existing `pane.setTitle(title, subtitle)` for native title drawing.

Missing `sound` chooses a configured random default, `""` is silent, and a
nonempty value overrides the default. Each show replaces the current content
and plays its sound once; script reload restores content without replaying it.
Updating an already displayed popup does not raise it; closing and reopening
uses normal window stacking. World transitions retain the popup, while player
changes and interface teardown discard it.

This registration does not handle confirmation, join requests or HTTP trust.

### Bundled songbook

The `"songbook"` registration loads
`/interface/asterbound/songbook/songbook.config` on the `"Window"` layer with
`"player"` scope and `"retain": true`. Native instrument interactions open it
through the generic interface bridge. It reuses the original songbook widget
layout; no native songbook window class is involved.

Lua owns the `/songs/` ABC catalog, ASCII-case-insensitive sorting, literal
search and highlighting of every match. Rows carry original resource paths,
not filtered indices. Play submits `{resource, abc}` with the original asset
bytes and the entered band to the current player's `songbook.play`; no selection
leaves the window open. Closing the window never stops playback.

The derived catalog, display names, folded search keys and last-query cache are
context-local. `storage` checkpoints only ordered resource paths and the true
dismissal marker; the retained native widgets own search, band and selection.
Ordinary dismissal preserves those widgets when the catalog is unchanged.
Catalog changes while closed rebuild the list on reopening; live reload always
rescans and clears selection without replaying a song. World transitions retain
the instance; player changes and interface shutdown release it. Instrument
keepalive, ABC playback, audio and network synchronization remain native services.

Native `/swap` may reuse the current Songbook and continue its playback;
discarding player-scoped UI does not add an implicit `songbook.stop()`.

### Bundled codex

The `"codex"` registration loads `/interface/asterbound/codex/codex.config` as a
player-scoped, retained `"Window"`, reusing the original species tabs, list,
labels and paging buttons. Lua owns filtering, selection and paging; rows carry
stable book IDs, including when identical titles or read-driven sorting change
their positions. Empty books have no enabled page navigation.

The main-bar button uses the generic manager toggle. The
`globalActions.InterfaceCodex` registration opens or toggles the window even
before an instance exists, without overriding native keyboard capture or modal
barriers. Ordinary dismissal preserves species, book and page; world changes
retain the window, while player changes and interface teardown destroy it.

`/interface/asterbound/codex/unread.lua` owns automatic unread opening. It waits while
a modal window is present, selects the native first unread ID, and acknowledges
it through `player.markCodexRead`. Reload does not replay already acknowledged
books. Content parsing, unlocks, read flags and ordering remain native player
domain services; known content follows the player's held Codex objects.

### Bundled quest log

The canonical `"QuestLog"` registration loads
`/interface/asterbound/questlog/questlog.config` as a player-scoped, retained
`"Window"`, reusing the original layout and named widgets. The main-bar button
and `globalActions.InterfaceQuest` toggle it through the generic Lua manager;
there is no native QuestLog window class or registration.

Lua owns the active/completed filter, main/side lists, selection, read marks,
details, tracking and abandon controls. Selection follows stable quest IDs,
not row positions. Ordinary dismissal and reload retain the selected ID and
filter; world changes retain the instance, while player changes and interface
teardown destroy it. List queries refresh periodically; selected details use
independent text, portrait and reward invalidation tokens.

Rewards use a real native ItemGrid with ItemBag capacity **5**, independently
of its two visible slots. Native stacking and tooltips remain in use. Objective
portraits use the generic drawable setter with horizontal mirroring.
Quest state, eligibility, ordering, rewards and abandonment remain native
player-domain operations.

### Bundled quest dialogs

`/interface/asterbound/questdialogs/pending.lua` schedules the canonical
`"QuestDialog"` dynamic registration on `"ModalWindow"`, independently of the
quest log. It waits for an in-world player and an unoccupied modal layer, then
selects queued administrator previews, failed quests, completed quests and
offers in that order.

Lua selects the original short/Objective-portrait default configuration or the
quest template's nested `guiConfigs.newQuest`, `questComplete` or `questFailed`
override. It reuses `paneLayout` and named widgets, native reward ItemBag
capacity **5**, money labels and player-aware item tooltips. Objective portraits
are mirrored horizontally. `onShowSound` plays once per presentation, not on
script reload.
Window bounds explicitly retain the legacy background dimensions: maximum
header/body/footer width and summed height. Off-panel placeholder controls do
not enlarge the root or displace centering; authored anchors remain unchanged.

Requests use native process-local decision tokens rather than IDs alone.
Acknowledgment follows successful display. Accept/decline/cancel is committed
on actual dismissal; script reload and capability rebinding do not cancel an
offer. World/player expiration destroys the world-scoped instance and retires
its request. Repeated or stale decisions cannot act on a replacement quest.

For completed quests, named `close`, `btnAccept` and `btnDecline` actions resolve
and immediately load the completion cinematic before closing. Escape and
external dismissal do not play it. Administrator previews do not mutate quest
state or run completion cinematics. There are no native quest-dialog window
classes or separate presentation poller.

### Bundled pause menu

The canonical `"EscapeDialog"` registration loads
`/interface/asterbound/escapemenu/escapemenu.config` as a session-scoped
`"ModalWindow"`. It is created on first open and retained across ordinary
dismissal/reopening; interface teardown destroys it. There is no native pause
menu constructor, enum registration or Escape-toggle branch.

Lua owns the three buttons, version label and pause selection. Return dismisses
the menu; Options dismisses it before opening the registered `"Options"` pane;
Save and Quit requests `interface.returnToTitle()` before dismissing.
[`sb.buildInfo()`](utility.md#json-sbbuildinfo) supplies the base Starbound `version`, Asterbound `productVersion`, bundled `assetVersion`, `product` and `architecture`.

The feature-owned layout builder reuses `/interface.config:escapeDialog`,
including modded widgets. Unless an explicit wrapper size is supplied, it
derives the legacy root size from background header/body/footer dimensions
before display, avoiding child-union changes to centering.

`InterfaceEscapeMenu` uses the manager's global toggle and a local close action.
The local action lets a remapped opening key close its own modal even when that
key is not also `GuiClose`. Native focused widgets
and modal handling take priority, so the event that closes a window does not
also reopen the pause menu. The controller's Lua `pauseRequested()` selects
actual displayed `"EscapeDialog"` or `"Options"` instances, not checkpoint
state and not all modal windows. The client applies that result through its
existing local-server pause path; UI updates continue while the world is paused.

### Bundled world-name HUD

The canonical `"PlanetText"` registration loads
`/interface/asterbound/planettext/planettext.config` as an automatically opened,
session-scoped `"Hud"`. Lua owns its countdown, name formatting, visibility
and final fade. The native initializer, timer and presentation updater are
removed. The named `planetText` label and cached pane/widget handles survive
world/player transitions and script reload while the interface exists.

The original state transitions are preserved: when no cinematic is active,
the float32 timer advances before display work and hides at zero. A cinematic
pauses it while the player is attached; only an active cinematic with an
off-world player resets and hides it. A world change alone does not restart
the timer. Nonempty `world.name()` values update the label; missing/empty names
retain any previous text while the remaining time continues to fade.

The HUD reads `planetNameTime`, `planetNameFadeTime`, `planetNameFormatString`,
`planetTextStyle` and `planetTextOffset` from `/interface.config`. Formatting
uses strict `sb.formatTags` with `worldName`. It retains the native centered,
zero-sized, unclipped root and full Label text style. Position preserves the
legacy integer scalar operation: configured offset multiplied by
`trunc(float32(2 / interface.scale()))`, not componentwise floating scaling.
The HUD is noninteractive; hiding or reloading does not replay its countdown.

### Bundled quest tracker

The canonical `"QuestTracker"` dynamic registration loads
`/interface/asterbound/questtracker/questtracker.config` as an automatically opened,
session-scoped `"Hud"`. The public identifier keeps its original spelling, but
there is no native tracker window class or native registration.

Lua reads one `player.currentQuestView()` per update and owns objective
formatting, collapsed first-incomplete selection, expanded layout, progress
frames and compass animation. Expanded mode lists every objective; when all
are complete, collapsed mode shows the first. The compass preserves the native
float32 spring and advances per update, not by `dt`.

The original named widgets and negative-coordinate layout remain available.
Only a left click on the active background toggles expansion; progress and
compass extensions do not consume unrelated pointer input. Generic drawing
offset and scissoring helpers allow these extensions outside the zero-sized
anchored root, without HUD-specific host fields.

With no effective quest the pane and its children are hidden, not destroyed.
The instance keeps ticking, so a new eligible quest reappears automatically.
Expansion and compass state survive reload and player/world transitions;
current player services are centrally rebound. Cached pane/widget handles
remain usable through these transitions. `isDisplayed()` reports retained
display membership; use `isVisible()` or child activity to inspect visibility.
Interface teardown ends the instance and its handles.

### Low-level instance bridge

* `interface.createPane(id, config, layer)` constructs an undisplayed script
  pane from resolved JSON. `id` must be unused and not a reserved native ID.
  `layer` uses native names such as `"Hud"`, `"Window"` or `"ModalWindow"`.
* `interface.destroyPane(id)` dismisses and releases a dynamic instance.
  It does not destroy native registered panes. Instance storage ends here.
* `interface.callPane(id, method, arguments?)` synchronously invokes a Lua
  method on an existing script instance; JSON array arguments are spread
  positionally. Returns its JSON result or `nil` for a missing method.
  It neither opens nor creates instances.
* `interface.sendPaneEvent(id, event, payload)` enqueues a manager event.
  Prefer `ui.send` and the other helpers to constructing controller requests.
* `interface.paneIdAvailable(id)` checks whether an ID is permitted for Lua ownership (nonempty and not reserved/native); an existing dynamic instance may still use that ID.
* `interface.paneInputTargets()` returns dynamic IDs in current native input
  priority, including capture, layers, click-to-raise and modal barriers.
  The Lua manager uses this order rather than registration order for actions.
* `interface.paneLayerOccupied(layer)` reports whether the native layer has
  a top pane, including native windows. Layer names use the same spelling as
  `createPane`; invalid names raise an error. It does not create an instance.
* `interface.session()` returns `hasPlayer`, `inWorld`, `playerGeneration` and
  `worldGeneration`. Generations identify changes, not world or player IDs.
  Use `hasPlayer`/`inWorld` to test availability: unavailable session service
  groups are explicitly empty tables, not inherited old callbacks or `nil`.

Use the Lua manager for normal registration/lifetime changes; the low-level
bridge exists for allocation, native stack access and synchronous queries.
