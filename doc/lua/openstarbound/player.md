# Player

[Lua API](../README.md#openstarbound-compatible-extensions) · [Base API](../player.md) · [Modding guide](../../modding.md)

The `player` table contains additional functions to save/load, access and modify the player's identity, mode, aim, emote and more.

---

#### `Bool` player.inWorld()

Returns whether this player entity is attached to a world. This is the exact
actor state, not the client-world readiness reported by `world.inWorld()` or
`interface.session().inWorld`.

---

#### `Json` player.save()

Serializes the player to Json the same way Starbound does for disk storage and returns it.

---

#### `void` player.load(`Json` save)

Reloads the player from a Json **save**. This will reset active ScriptPanes and scripts running on the player.

---

#### `String` player.name()

Returns the player's name.

---

#### `void` player.setName(`String` name)

Sets the player's name.

---

#### `String` player.description()

Returns the player's description.

---

#### `void` player.setDescription(`String` description)

Sets the player's description. The new description will not be networked until the player warps or respawns.

---

#### `void` player.controlMovementState(`Maybe<String>` state)

Requests a presentation-only humanoid movement state: `"idle"`, `"walk"`, `"run"`,
`"jump"`, `"fall"` or `"duck"`. Other names (including `"crouch"`) are rejected.
Passing `nil` revokes the current request. The last call in a frame wins; this is
not a per-script ownership stack.

Refresh the request in a generic player script's `update` on every simulation
frame (`script.setUpdateDelta(1)`). Requests are cleared before the next player
update; calls in `init`, `postUpdate`, or messages before that update do not carry
over into it. Pausing freezes the current presentation with the simulation.
World exit/uninitialization clears the request, and it is never saved.

The request does not change velocity, collision, `onGround`, the movement
controller's state, or the player's native movement state and movement effects.
In particular, `"duck"` only selects the crouching animation: it does not set
`mcontroller.crouching()` or shrink the collision polygon.
Lounge/anchored, dead and teleporting players ignore it; a tech's explicit parent
animation state retains precedence. Existing dances, held-item arm poses and
item-controlled facing retain their normal behavior.

The requested state drives the actual humanoid animation, for both legacy and
animator-based humanoids, at its configured normal cycle speed. Refreshing the
same state does not restart its cycle. Facing still comes from the movement
controller (use `mcontroller.controlFace` with a body-local left/right direction)
and held items; the backwards walk/run cycle compares that facing with velocity
projected onto the body's rotated local X axis, rather than world X. Body
rotation still comes from `mcontroller.rotation()`.

Outbound replication reuses the vanilla player movement-state field and the
movement controller's existing moving-direction field, including legacy protocol
0, in both full and incremental updates. `"duck"` explicitly maps to the native
player `Crouch` wire state. Only their network presentation values are overridden:
local native player state and `mcontroller.movingDirection()` remain unchanged.
The moving-direction value is the same body-local projection used for local backwards playback;
facing, position, velocity, body rotation and physics flags retain their normal
network values. A tech's parent state is not changed and no tech is equipped.
Stopping requests restores both fields to their native values on that update.

These fields have no OpenStarbound compatibility-version gate; rotation already
uses the native angle-interpolated, 0.01-radian fixed-point field. No fields,
humanoid groups, custom animation parameters or persistent properties are added.
This is a source-level legacy protocol contract, not proof of rendering in a
vanilla executable. Remote cycles use their normal local animation clocks, not a
synchronized cycle phase, and remote native effects may follow the transmitted
presentation state rather than the controlling player's physical state.

---

#### `void` player.refreshHumanoidParameters()

Refreshes the active humanoid parameters by re-building the current humanoid instance.

This must be called when certain values in the humanoid config would be changed by a parameter, such as the animationConfig and movementParameters.

This is implicitly called whenever the current humanoid identity's species or imagePath changes.

This has to network the humanoid identity and humanoid parameters as intialization arguments for the new instance, so only use it when necessary.

---

#### `void` player.setHumanoidParameter(`String` key `Json` value)

Sets a parameter that overwrites a value in the humanoid config.

---

#### `void` player.setHumanoidParameters(`JsonObject` parameters)

Replaces the current table of humanoid parameters that overwrite values in the humanoid config.

---

#### `Maybe<Json>` player.getHumanoidParameter(`String` key)

Gets a humanoid parameter.

---

#### `JsonObject` player.getHumanoidParameters()

Gets a table of all the humanoid parameters.

---

#### `JsonObject` player.humanoidConfig(`bool` withOverrides)

Gets the active humanoid config.

If withOverrides is true, returns it with the overrides applied by armor.

The humanoid config controls how the player is animated, as well as their movement properties.

---

#### `void` player.setSpecies(`String` species)

Sets the player's species. Must be a valid species.

Implicitly calls `player.refreshHumanoidParameters()` if species is changed.

---

#### `void` player.setGender(`String` gender)

Sets the player's gender.

---

#### `String` player.imagePath()

If the player has a custom humanoid image path set, returns it. otherwise, returns `nil`.

#### `void` player.setImagePath(`String` imagePath)

Sets the player's image path. Specify `nil` to remove the image path.

Implicitly calls `player.refreshHumanoidParameters()` if imagePath is changed.

---

#### `Personality` player.personality()

Returns the player's personality as a `table` containing a `String` idle, `String` armIdle, `Vec2F` headOffset and `Vec2F` armOffset.

---

#### `void` player.setPersonality(`Personality` personality)

Sets the player's personality. The **personality** must be a `table` containing at least one value as returned by `player.personality()`.

---

#### `String` player.bodyDirectives()

Returns the player's body directives.

---

#### `void` player.setBodyDirectives(`String` bodyDirectives)

Sets the player's body directives.

---

#### `String` player.emoteDirectives()

Returns the player's emote directives.

---

#### `void` player.setEmoteDirectives(`String` emoteDirectives)

Sets the player's emote directives.

---

#### `void` player.setHair(`String` hairGroup, `String` hairType, `String` hairDirectives)

Sets the player's hair group, type, and directives.

---

#### `String` player.hairGroup()

Returns the player's hair group.

---

#### `void` player.setHairGroup(`String` hairGroup)

Sets the player's hair group.

---

#### `String` player.hairType()

Returns the player's hair type.

---

#### `void` player.setHairType(`String` hairType)

Sets the player's hair type.

---

#### `String` player.hairDirectives()

Returns the player's hair directives.

---

#### `void` player.setHairDirectives(`String` hairDirectives)

Sets the player's hair directives.

---

#### `String` player.facialHair()

Returns the player's facial hair type. Same as player.facialHairType?

---

#### `void` player.setFacialHair(`String` facialHairGroup, `String` facialHairType, `String` facialHairDirectives)

Sets the player's facial hair group, type, and directives.

---

#### `String` player.facialHairType()

Returns the player's facial hair type.

---

#### `void` player.setFacialHairType(`String` facialHairType)

Sets the player's facial hair type.

---

#### `String` player.facialHairGroup()

Returns the player's facial hair group.

---

#### `void` player.setFacialHairGroup(`String` facialHairGroup)

Sets the player's facial hair group.

---

#### `String` player.facialHairDirectives()

Returns the player's facial hair directives.

---

#### `void` player.setFacialHairDirectives(`String` facialHairDirectives)

Sets the player's facial hair directives.

---

#### `String` player.facialMask()

Returns the player's facial mask group.

---

#### `void` player.setFacialMask(`String` facialMaskGroup, `String` facialMaskType, `String` facialMaskDirectives)

Sets the player's facial mask group, type, and directives.

---

#### `String` player.facialMaskDirectives()

Returns the player's facial mask directives.

---

#### `void` player.setFacialMaskDirectives(`String` facialMaskDirectives)

Sets the player's facial mask directives.

---

#### `PlayerMode` player.mode()

Returns the player's mode.

---

#### `void` player.setMode(`String` mode)

Sets the player's mode. **mode** must be either `"casual"`, `"survival"` or `"hardcore"`.

---

#### `Color` player.favoriteColor()

Returns the player's favorite color.
It is used for the beam shown when wiring, placing, and highlighting with beam-tools (Matter Manipulator).

---

#### `void` player.setFavoriteColor(`Color` color)

Sets the player's favorite color. **color** can have an optional fourth value for transparency.

---

#### `Vec2F` player.aimPosition()

Returns the player's aim position.

---

#### `void` player.emote(`String` emote, [`float` cooldown])

Makes the player do an emote with the default cooldown unless a **cooldown** is specified.

---

#### `String, float` player.currentEmote()

Returns the player's current emote and the seconds left in it.

---

#### `Maybe<bool>` player.armorVisible(`String` slotName)

Returns whether the player's armor in the specified slot is visible. Returns `nil` if the slot doesn't exist.

---

#### `void` player.setArmorVisible(`String` slotName, `bool` visible)

Sets whether the player's armor in the specified slot is visible.

---

#### `unsigned, unsigned` player.actionBarGroup()

Returns the active action-bar group (1-based) and the total number of groups.

---

#### `void` player.setActionBarGroup(`unsigned` barId)

Sets the player's active action bar.

---

#### `Maybe<Variant<unsigned, EssentialItem>>` player.selectedActionBarSlot()

Returns a 1-based custom slot index, an essential tool name
(`beamaxe`, `wiretool`, `painttool`, `inspectiontool`), or nil for empty hands.

---

#### `void` player.setSelectedActionBarSlot([`Variant<unsigned, EssentialItem>` slot])

Selects a custom slot or essential tool; nil selects empty hands.

---
#### `Maybe<LuaTable>` player.actionBarView([`uint64` knownRevision])

Returns nil only when the supplied revision is unchanged. Otherwise returns
`{revision, indexes, groups, group, selected?, items}`. Group and custom
selection indices are 1-based; absent `selected` means empty hands. Essential
selection uses the names documented above. Revision tokens are process-unique
across callback rebinding, not persistent save data.

`items` is dense: primary/secondary for each custom index, then the four
essential tools in the order above, then the swap-slot item. Each entry has
`twoHanded` and `secondaryDrawables` booleans, even when empty. Occupied entries
also have an opaque owning Item userdata `handle` and a snapshot-local
`identity`: the first 1-based entry sharing the same native item pointer.
These handles are not item descriptors or JSON and cannot enter `storage`.
Pass them directly to `widget.setItemSlotHandle` or return them from
`tooltipItem`. Count/durability remain live on the native item and need not
invalidate this link/selection/flag snapshot.

#### `void` player.linkActionBarSwap(`unsigned` index, `bool` primary)

Uses a 1-based custom index and actual native swap item. With a swap item,
links it and performs native clear/return-to-inventory processing; a two-handed
item redirects to primary. With no swap item, selects that custom index.
Inventory-full clear failure does not promise an empty cursor or rollback.
Out-of-range indices raise errors.

#### `void` player.clearActionBarLink(`unsigned` index, `bool` primary)

Clears the native link for a 1-based custom index; the displayed secondary
half of a two-handed primary clears primary. Out-of-range indices raise errors.
This domain operation has no Inventory-window gate; the bundled UI applies it.

#### `bool` player.hungerEnabled()

Returns hunger enablement from the player's loaded game-mode configuration.

#### `Vec2F` player.position()

Returns the actual Player position, including while detached from the world.

#### `Maybe<EntityId>` player.lastDamagedTarget()

Returns the native last-damaged target ID, or nil for no target. A returned ID
does not guarantee the entity still exists.

#### `float` player.timeSinceLastGaveDamage()

Returns the player's native elapsed damage timer in seconds.

#### `bool` player.interruptRadioMessage()

Destructively polls and clears the native radio-interrupt flag. This does not
request interruption. The bundled radio instance is the sole intended consumer.

#### `Maybe<Json>` player.pullPendingRadioMessage()

Destructively dequeues the next resolved native radio message, or returns nil
when empty. Returns `{messageId, text, senderName, portraitImage, portraitFrames,
portraitSpeed, textSpeed, persistTime, chatterSound}`. The portrait image may
contain `<frame>`; frame count is an integer, speeds/timing are native numeric
presentation values. Tutorial/species/AI resolution, delayed queue insertion,
unique filtering and important-message prepend/interrupt ordering remain native.

Unique messages are marked played **at dequeue**, even if subsequently
interrupted or the interface closes. There is no completion acknowledgment.
Do not install a competing consumer or drain into a second queue; existing
`player.radioMessage(message, delay?)` remains the producer.

---


#### `void` player.setDamageTeam(`DamageTeam` team)

Sets the player's damage team. This must be called every frame to override the current damage team that the server has given the player (normally controlled by /pvp)

---

#### `void` player.say(`String` line)

Makes the player say a string.

---

#### `Json` player.humanoidIdentity()

Returns the specific humanoid identity of the player, containing information such as hair style and idle pose.

---

#### `void` player.setHumanoidIdentity(`Json` humanoidIdentity)

Sets the specific humanoid identity of the player.

Implicitly calls `player.refreshHumanoidParameters()` if species or imagePath is changed.

---

#### `ItemDescriptor` player.item(`ItemSlot` itemSlot)

Returns the contents of the specified itemSlot.

---

#### `void` player.setItem(`ItemSlot` itemSlot, `ItemDescriptor` item)

Puts the specified item into the specified itemSlot.
Item slots in item bags are structured like so: `{String bagName, int slot}`

---

#### `int` player.itemBagSize(`String` itemBagName)

Returns the size of an item bag.

---

#### `bool` player.itemAllowedInBag(`String` itemBagName, `ItemDescriptor` item)

Returns whether the specified item can enter the specified item bag.

---

#### `ActionBarLink` player.actionBarSlotLink(`int` slot, `String` hand)

Returns the contents of the specified action bar link slot's specified hand.

---

#### `bool` player.setActionBarSlotLink(`int` slot, `String` hand, `ItemSlot` itemSlot)

Links the specified slot's hand to the specified itemSlot.

---

#### `Float` player.interactRadius()

Returns the player's interact radius.

---

#### `void` player.setInteractRadius(`Float` interactRadius)

Sets the player's interact radius. This does not persist upon returning to the main menu.

---

#### `JsonArray` player.availableRecipes()

Returns all the recipes the player can craft with their currently held items and currencies.

---

#### `String` player.currentState()

Returns the player's current movement state.

<details><summary><b>Player States</b></summary>
idle<br>
walk<br>
run<br>
jump<br>
fall<br>
swim<br>
swimIdle<br>
lounge<br>
crouch<br>
teleportIn<br>
teleportOut<br>
</details>

---

#### `List<Json>` player.teamMembers()

Returns an array, each entry being a table with `name`, `uuid`, `entity`, `healthPercentage` and `energyPercentage`

---

#### `bool` player.isCodexKnown(`String` codexId)

Returns `true` if the player knows the specified codexId, and `false` otherwise.

---

#### `bool` player.isCodexRead(`String` codexId)

Returns `true` if the player has read the specified codexId, and `false` otherwise.

---

#### `bool` player.markCodexRead(`String` codexId)

Marks the specified codexId as read by the player. Returns `true` if the codex is known by the player and was marked as unread and `false` otherwise.

---

#### `bool` player.markCodexUnread(`String` codexId)

Marks the specified codexId as not read by the player. Returns `true` if the codex is known by the player and was marked as read and `false` otherwise.

---

#### `void` player.learnCodex(`String` codexId, [`bool` markRead])

Make the player learn the specified codexId. If markRead is `true`, then the codex will be marked as read by default.

---

#### `Json` player.getCodexes()

Returns a JSON object of key-value pairs, which are the codex ID and read status respectively.

---

#### `String` player.getNewCodex()

Returns the **title** of the native first unread codex, or `nil`. This compatibility API does not return an ID and does not promise newest-first ordering. Use `getNewCodexId` when identifying a book.

---

#### `Json` player.getCodex(`String` codexId)

Returns a known book as `{id, species, title, description, icon, contentPages}`, or `nil` for an unknown ID. The icon includes the database's default and relative-path resolution. Reading this view does not unlock the book or mark it read.

Content comes from the parsed Codex object held by the player, matching native reader behavior. Asset reload does not replace already held book content; this is not a fresh lookup into the reloaded asset database.

---

#### `Json[]` player.getCodexList()

Returns known books as `{id, read}` entries in native reader order: unread before read, then by title. Equal titles are not unique identities; use `id` for selection. This query does not mark books read.

---

#### `String` player.getNewCodexId()

Returns the stable ID of the native first unread codex, or `nil`. It uses the same unread choice as `getNewCodex`, without substituting its title. Call `markCodexRead` to acknowledge reading.

---

#### `Maybe<LuaValue>` player.callQuest(`String` questId, `String` functionName, `LuaVariadic` args)

Calls a function in the specified quest script.

---

#### `Bool` player.canTurnInQuest(`String` questId)

Returns `true` if the player can turn in the specified quest.

---

#### `Maybe<Json>` player.currentQuest()

Returns the effective current quest's persisted JSON representation, or `nil` if none is eligible. This is not a lightweight live HUD view; use `currentQuestView` for objectives, progress and compass state.

---

#### `Maybe<Json>` player.currentQuestView()

Returns `nil` when the native quest manager has no eligible current quest, otherwise a compact table:

* `id`: the effective quest ID.
* `objectives`: optional live objective array, normally entries `{text, complete}`. An empty array remains distinct from an absent field.
* `progress`: optional native progress fraction; `0` is a present value.
* `compassDirection`: optional native direction in radians; `0` is a present value.

Absent optional fields are omitted. Selection uses `QuestManager.currentQuest()`, including its on-world preference and active/show-in-log eligibility, rather than reconstructing selection from the manually tracked ID. This query does not serialize rewards, portraits, script storage or other quest save data, and does not alter tracking or quest state.

---

#### `Maybe<String>` player.currentQuestId()

Returns the manually tracked quest ID, or `nil` when none is selected. It is not necessarily the effective on-world quest, and a returned ID does not itself guarantee active/show-in-log eligibility. Use `currentQuestView().id` after checking for `nil` when displaying the effective quest.

---

#### `void` player.dance(`String` dance)

Makes the player perform a dance. Pass `nil` to stop dancing.

---

#### `LuaTable` player.effectsAnimator()

Returns the effects animator callbacks table for the player.

---

#### `String` player.nametag()

Returns the player's nametag.

---

#### `Json` player.quest(`String` questId)

Returns quest information as JSON.

---

#### `StringList` player.questIds()

Returns a list of all quest IDs the player has.

---

#### `Json` player.questLogEntries(`Bool` completed)

Returns `{entries, current?}`. `entries` contains native-ordered active or
completed, show-in-log quests eligible for the current server. Each row has
`id`, `title`, `main`, `unread`, `tracked`, `currentWorld` and
`portraitRevision`. `current` is the optional effective current quest ID.
`tracked` uses the manager's effective on-world-or-manual selection, not just
the manually tracked ID returned by `currentQuestId()`.

---

#### `Maybe<Json>` player.questLogDetails(`String` questId, [`Integer` knownRevision], [`Integer` knownPortraitRevision], [`Integer` knownRewardsRevision])

Returns `nil` for an unknown quest. Otherwise always returns `active`,
`tracked`, `canAbandon`, `revision`, `portraitRevision` and `rewardsRevision`.
The three revision arguments independently suppress unchanged categories:

* `revision`: `title` and `text`.
* `portraitRevision`: Objective `portraitTitle` and `portrait` drawables.
* `rewardsRevision`: item `rewards` descriptors, including an empty array.

Omit a known token to request that category. An invalidated absent portrait
returns `portraitTitle = ""` and no `portrait`; clear any previous image.
Control flags remain live even when all categories are unchanged. Tokens are
opaque, process-local invalidation values, not persisted IDs or timestamps;
same-ID native quest replacement invalidates cached categories.

---

#### `void` player.markQuestRead(`String` questId)

Acknowledges the native quest read flag. Unknown IDs are ignored.

---

#### `Bool` player.abandonQuest(`String` questId)

Returns `false` for unknown, inactive or non-abandonable quests. Otherwise
invokes native abandonment and returns `true`. Native errors propagate;
failure does not guarantee rollback of a quest script's state transition.

---

#### `Maybe<Json>` player.pendingQuestDialog(`String` kind)

Acquires a native decision lease for `"failed"`, `"complete"` or `"offer"`,
using native server eligibility and ordering, or returns `nil`. `"preview"`
queries queued administrator previews. Repeated queries for the same pending
quest/kind reuse its token; acquiring a view does not acknowledge it.

The view contains `id`, `token`, `kind`, optional `guiConfig` template override,
`title`, `text`, `portrait`, `portraitTitle`, `objectivePortrait`,
`objectivePortraitTitle`, `rewards`, `money`, `canDecline` and `preview`.
`kind` is `"failed"`, `"complete"` or `"offer"` even for a preview.
Objective fields describe an offer; other kinds return an empty Objective
portrait/title. Lua chooses default layouts, not this domain query.

Tokens are opaque process-local integers capturing the exact quest allocation
and state transition. Do not persist them across game sessions or quest-manager
loading. A state cycle back to Offer does not revive an old token.

---

#### `Bool` player.questDialogValid(`Integer` token)

Tests whether a lease still belongs to its captured quest/state. Same-ID
replacement, state transitions and manager loading invalidate old requests.
An acknowledged ephemeral completion/failure quest may remain valid after its
removal from the ordinary quest map, until the presentation finishes.

---

#### `Bool` player.acknowledgeQuestDialog(`Integer` token)

Acknowledge only after successful presentation. Returns `false` for an invalid
token; repeated acknowledgment of a valid token is harmless. Completed/failed
quests have their native pending-dialog flag cleared. Offers remain pending
until a decision; previews never acknowledge a player's quest.

---

#### `Maybe<Json>` player.questDialogCinema(`Integer` token)

Resolves a valid completion request's cinematic, substituting the current
player's `<species>` and `<gender>` tags and fetching its JSON definition.
Returns `nil` for invalid requests, previews, other kinds or absent cinematics.
Asset errors propagate. This query does not play anything; client UI scripts
can pass the result to `interface.playCinematic`.

---

#### `Bool` player.finishQuestDialog(`Integer` token, `String` decision)

Retires a lease exactly once. `decision` is `"accept"`, `"decline"`, `"cancel"`
or `"close"`; unknown values raise an error. Returns `false` for unknown/stale
tokens. For an offer, accept starts it, allowed decline declines it, and
cancel/close or denied decline cancels it. Other kinds and previews only
retire their lease. This operation does not play cinematics or replace
acknowledgment.

The lease retires before invoking quest scripts, preventing reentrant/repeated
decisions. Native errors propagate without a rollback guarantee. Leaving the
world cancels outstanding offer decisions and retires the remaining leases.

---

#### `Json` player.questObjectives(`String` questId)

Returns the objectives for the specified quest.

---

#### `Maybe<JsonArray>` player.questPortrait(`String` questId, `String` portraitName)

Returns native drawables for the named quest portrait, such as `"Objective"`
or `"QuestStarted"`, or `nil` when absent.

---

#### `String` player.questState(`String` questId)

Returns the state of the specified quest.

---

#### `Bool` player.removeTeleportBookmark(`Json` bookmarkConfig)

Removes a teleport bookmark. Returns `true` if successful.

---

#### `StringList` player.serverQuestIds()

Returns a list of server quest IDs.

---

#### `void` player.setAnimationParameter(`String` parameterName, `Json` value)

Sets an animation parameter for the player's humanoid animator.

---

#### `void` player.setNametag(`String` nametag)

Sets the player's nametag.

---

#### `void` player.setTrackedQuest(`String` questId)

Sets the currently tracked quest.

---

#### `void` player.stopLounging()

Makes the player stop lounging.

---

#### `String` player.trackedQuestId()

Returns the ID of the currently tracked quest.

---

#### `void` player.setCameraFocusEntity([`EntityId` entity])

Sets that player's camera to be centered on the position of the specified entity, or recenters the camera on the player's position if no entity id is specified.
