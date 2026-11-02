---@meta

--- player API

--- Whether this player entity is attached to a world, not client-world readiness.
---@return boolean
function player.inWorld() end

--- Serializes the player to Json the same way Starbound does for disk storage and returns it. ---
---@return Json
function player.save() end

--- Reloads the player from a Json **save**. This will reset active ScriptPanes and scripts running on the player. ---
---@param save Json
function player.load(save) end

--- Returns the player's name. ---
---@return string
function player.name() end

--- Sets the player's name. ---
---@param name string
function player.setName(name) end

--- Returns the player's description. ---
---@return string
function player.description() end

--- Sets the player's description. The new description will not be networked until the player warps or respawns. ---
---@param description string
function player.setDescription(description) end

--- Requests a presentation-only movement animation for this simulation frame.
--- Refresh in generic update with updateDelta=1; nil revokes, last call wins.
--- Native physics/state are unchanged. Anchors/lounge, dead, teleport and tech
--- parent states take precedence. Existing dances and item poses/facing remain.
--- Walk/run backwards direction uses velocity in the rotated body's local X.
--- Repeated requests retain cycle progress; cycle speed is the humanoid default.
--- Transient, not saved; uses existing vanilla/legacy player-state and moving-
--- direction fields without changing local native state/direction or physics.
--- Facing/velocity/rotation retain native replication; no custom animation key.
---@param state? "idle"|"walk"|"run"|"jump"|"fall"
function player.controlMovementState(state) end

--- Refreshes the active humanoid parameters by re-building the current humanoid instance. This must be called when certain values in the humanoid config would be changed by a parameter, such as the animationConfig and movementParameters. This is implicitly called whenever the current humanoid identity's species or imagePath changes. This has to network the humanoid identity and humanoid parameters as intialization arguments for the new instance, so only use it when necessary. ---
function player.refreshHumanoidParameters() end

--- Sets a parameter that overwrites a value in the humanoid config. ---
---@param key string
---@param value? Json
function player.setHumanoidParameter(key, value) end

--- Replaces the current table of humanoid parameters that overwrite values in the humanoid config. ---
---@param parameters JsonObject
function player.setHumanoidParameters(parameters) end

--- Gets a humanoid parameter. ---
---@param key string
---@return Json
function player.getHumanoidParameter(key) end

--- Gets a table of all the humanoid parameters. ---
---@return JsonObject
function player.getHumanoidParameters() end

--- Gets the active humanoid config. If withOverrides is true, returns it with the overrides applied by armor. The humanoid config controls how the player is animated, as well as their movement properties. ---
---@param withOverrides boolean
---@return JsonObject
function player.humanoidConfig(withOverrides) end

--- Sets the player's species. Must be a valid species. Implicitly calls `player.refreshHumanoidParameters()` if species is changed. ---
---@param species string
function player.setSpecies(species) end

--- Sets the player's gender. ---
---@param gender string
function player.setGender(gender) end

--- If the player has a custom humanoid image path set, returns it. otherwise, returns `nil`.
---@return string?
function player.imagePath() end

--- Sets the player's image path. Specify `nil` to remove the image path. Implicitly calls `player.refreshHumanoidParameters()` if imagePath is changed. ---
---@param imagePath? string
function player.setImagePath(imagePath) end

--- Returns the player's personality as a `table` containing a `String` idle, `String` armIdle, `Vec2F` headOffset and `Vec2F` armOffset. ---
---@return Personality
function player.personality() end

--- Sets the player's personality. The **personality** must be a `table` containing at least one value as returned by `player.personality()`. ---
---@param personality Personality
function player.setPersonality(personality) end

--- Returns the player's body directives. ---
---@return string
function player.bodyDirectives() end

--- Sets the player's body directives. ---
---@param bodyDirectives string
function player.setBodyDirectives(bodyDirectives) end

--- Returns the player's emote directives. ---
---@return string
function player.emoteDirectives() end

--- Sets the player's emote directives. ---
---@param emoteDirectives string
function player.setEmoteDirectives(emoteDirectives) end

--- Sets the player's hair group, type, and directives. ---
---@param hairGroup? string
---@param hairType? string
---@param hairDirectives? string
function player.setHair(hairGroup, hairType, hairDirectives) end

--- Returns the player's hair group. ---
---@return string
function player.hairGroup() end

--- Sets the player's hair group. ---
---@param hairGroup string
function player.setHairGroup(hairGroup) end

--- Returns the player's hair type. ---
---@return string
function player.hairType() end

--- Sets the player's hair type. ---
---@param hairType string
function player.setHairType(hairType) end

--- Returns the player's hair directives. ---
---@return string
function player.hairDirectives() end

--- Sets the player's hair directives. ---
---@param hairDirectives string
function player.setHairDirectives(hairDirectives) end

--- Returns the player's facial hair type. Same as player.facialHairType? ---
---@return string
function player.facialHair() end

--- Sets the player's facial hair group, type, and directives. ---
---@param facialHairGroup? string
---@param facialHairType? string
---@param facialHairDirectives? string
function player.setFacialHair(facialHairGroup, facialHairType, facialHairDirectives) end

--- Returns the player's facial hair type. ---
---@return string
function player.facialHairType() end

--- Sets the player's facial hair type. ---
---@param facialHairType string
function player.setFacialHairType(facialHairType) end

--- Returns the player's facial hair group. ---
---@return string
function player.facialHairGroup() end

--- Sets the player's facial hair group. ---
---@param facialHairGroup string
function player.setFacialHairGroup(facialHairGroup) end

--- Returns the player's facial hair directives. ---
---@return string
function player.facialHairDirectives() end

--- Sets the player's facial hair directives. ---
---@param facialHairDirectives string
function player.setFacialHairDirectives(facialHairDirectives) end

--- Returns the player's facial mask group. ---
---@return string
function player.facialMask() end

--- Sets the player's facial mask group, type, and directives. ---
---@param facialMaskGroup? string
---@param facialMaskType? string
---@param facialMaskDirectives? string
function player.setFacialMask(facialMaskGroup, facialMaskType, facialMaskDirectives) end

--- Returns the player's facial mask directives. ---
---@return string
function player.facialMaskDirectives() end

--- Sets the player's facial mask directives. ---
---@param facialMaskDirectives string
function player.setFacialMaskDirectives(facialMaskDirectives) end

--- Returns the player's mode. ---
---@return PlayerMode
function player.mode() end

--- Sets the player's mode. **mode** must be either `"casual"`, `"survival"` or `"hardcore"`. ---
---@param mode string
function player.setMode(mode) end

--- Returns the player's favorite color. It is used for the beam shown when wiring, placing, and highlighting with beam-tools (Matter Manipulator). ---
---@return Color
function player.favoriteColor() end

--- Sets the player's favorite color. **color** can have an optional fourth value for transparency. ---
---@param color Color
function player.setFavoriteColor(color) end

--- Returns the player's aim position. ---
---@return Vec2F
function player.aimPosition() end

--- Makes the player do an emote with the default cooldown unless a **cooldown** is specified. ---
---@param emote string
---@param cooldown? number
function player.emote(emote, cooldown) end

--- Returns the player's current emote and the seconds left in it. ---
---@return string, number
function player.currentEmote() end

--- Returns whether the player's armor in the specified slot is visible. Returns `nil` if the slot doesn't exist. ---
---@param slotName string
---@return boolean?
function player.armorVisible(slotName) end

--- Sets whether the player's armor in the specified slot is visible. ---
---@param slotName string
---@param visible boolean
function player.setArmorVisible(slotName, visible) end

--- Active 1-based group and total groups.
---@return unsigned group
---@return unsigned groups
function player.actionBarGroup() end

--- Sets the player's active action bar. ---
---@param barId unsigned
function player.setActionBarGroup(barId) end

--- 1-based custom index, essential tool name, or nil for empty hands.
---@return (unsigned|EssentialItem)?
function player.selectedActionBarSlot() end

--- Sets the player's selected action bar slot. ---
---@param slot? integer|string
function player.setSelectedActionBarSlot(slot) end

---@class ActionBarItemView
---@field handle? NativeItemHandle Opaque actual shared native Item; never persist in JSON/storage.
---@field identity? integer First 1-based snapshot entry sharing this native item pointer.
---@field twoHanded boolean
---@field secondaryDrawables boolean

---@class ActionBarView
---@field revision integer Process-unique across callback rebinding.
---@field indexes integer
---@field groups integer
---@field group integer 1-based.
---@field selected? integer|string 1-based custom slot or essential name; absent for empty hands.
---@field items ActionBarItemView[] Primary/secondary pairs, four essentials, swap; empty slots still have entries.

--- Nil only for unchanged supplied revision. Live native item counts/durability need not invalidate it.
---@param knownRevision? integer
---@return ActionBarView?
function player.actionBarView(knownRevision) end

--- Native swap link/return operation; empty swap selects index, two-handed redirects primary.
--- Does not promise cursor empty/rollback on inventory-full clear failure.
---@param index integer 1-based custom slot, validated.
---@param primary boolean
function player.linkActionBarSwap(index, primary) end

--- Native link clear; secondary half of two-handed primary clears primary. No UI visibility gate.
---@param index integer 1-based custom slot, validated.
---@param primary boolean
function player.clearActionBarLink(index, primary) end

--- Loaded game-mode hunger setting.
---@return boolean
function player.hungerEnabled() end

--- Actual Player position even while detached.
---@return Vec2F
function player.position() end

---@return EntityId?
function player.lastDamagedTarget() end

--- Native elapsed damage timer, in seconds.
---@return number
function player.timeSinceLastGaveDamage() end

--- Destructive flag poll, not an interruption request; bundled radio is sole intended consumer.
---@return boolean
function player.interruptRadioMessage() end

---@class RadioMessageView
---@field messageId string
---@field text string
---@field senderName string
---@field portraitImage string
---@field portraitFrames integer
---@field portraitSpeed number
---@field textSpeed number
---@field persistTime number
---@field chatterSound string

--- Destructive resolved native dequeue; nil when empty. Unique accounting commits here, no completion ack.
--- Do not compete with bundled radio or drain into a second queue.
---@return RadioMessageView?
function player.pullPendingRadioMessage() end

--- Sets the player's damage team. This must be called every frame to override the current damage team that the server has given the player (normally controlled by /pvp) ---
---@param team DamageTeam
function player.setDamageTeam(team) end

--- Makes the player say a string. ---
---@param line string
function player.say(line) end

--- Returns the specific humanoid identity of the player, containing information such as hair style and idle pose. ---
---@return JsonObject
function player.humanoidIdentity() end

--- Sets the specific humanoid identity of the player. Implicitly calls `player.refreshHumanoidParameters()` if species or imagePath is changed. ---
---@param humanoidIdentity Json
function player.setHumanoidIdentity(humanoidIdentity) end

--- Returns the contents of the specified itemSlot. ---
---@param itemSlot ItemSlot
---@return ItemDescriptor
function player.item(itemSlot) end

--- Puts the specified item into the specified itemSlot. Item slots in item bags are structured like so: `{String bagName, int slot}` ---
---@param itemSlot ItemSlot
---@param item ItemDescriptorInput
function player.setItem(itemSlot, item) end

--- Returns the size of an item bag. ---
---@param itemBagName string
---@return number
function player.itemBagSize(itemBagName) end

--- Returns whether the specified item can enter the specified item bag. ---
---@param itemBagName string
---@param item ItemDescriptorInput
---@return boolean
function player.itemAllowedInBag(itemBagName, item) end

--- Returns the contents of the specified action bar link slot's specified hand. ---
---@param slot number
---@param hand string
---@return ActionBarLink
function player.actionBarSlotLink(slot, hand) end

--- Links the specified slot's hand to the specified itemSlot. ---
---@param slot number
---@param hand string
---@param itemSlot? ItemSlot
---@return boolean
function player.setActionBarSlotLink(slot, hand, itemSlot) end

--- Returns the player's interact radius. ---
---@return Float
function player.interactRadius() end

--- Sets the player's interact radius. This does not persist upon returning to the main menu. ---
---@param interactRadius Float
function player.setInteractRadius(interactRadius) end

--- Returns all the recipes the player can craft with their currently held items and currencies. ---
---@return JsonArray
function player.availableRecipes() end

--- Returns the player's current movement state. idle<br> walk<br> run<br> jump<br> fall<br> swim<br> swimIdle<br> lounge<br> crouch<br> teleportIn<br> teleportOut<br> ---
---@return string
function player.currentState() end

--- Returns an array, each entry being a table with `name`, `uuid`, `entity`, `healthPercentage` and `energyPercentage` ---
---@return Json[]
function player.teamMembers() end

--- Returns `true` if the player knows the specified codexId, and `false` otherwise. ---
---@param codexId string
---@return boolean
function player.isCodexKnown(codexId) end

--- Returns `true` if the player has read the specified codexId, and `false` otherwise. ---
---@param codexId string
---@return boolean
function player.isCodexRead(codexId) end

--- Marks the specified codexId as read by the player. Returns `true` if the codex is known by the player and was marked as unread and `false` otherwise. ---
---@param codexId string
---@return boolean
function player.markCodexRead(codexId) end

--- Marks the specified codexId as not read by the player. Returns `true` if the codex is known by the player and was marked as read and `false` otherwise. ---
---@param codexId string
---@return boolean
function player.markCodexUnread(codexId) end

--- Make the player learn the specified codexId. If markRead is `true`, then the codex will be marked as read by default. ---
---@param codexId string
---@param markRead? boolean
function player.learnCodex(codexId, markRead) end

--- Returns a JSON object of key-value pairs, which are the codex ID and read status respectively. ---
---@return Json
function player.getCodexes() end

--- Returns the title of the native first unread codex, or nil; use getNewCodexId for identity. ---
---@return string?
function player.getNewCodex() end

--- Returns a known player-held parsed book, or nil. Does not unlock or mark read; held content survives asset database reload.
---@param codexId string
---@return {id:string, species:string, title:string, description:string, icon:string, contentPages:string[]}|nil
function player.getCodex(codexId) end

--- Returns known IDs/read flags in native unread-first, then title order.
---@return {id:string, read:boolean}[]
function player.getCodexList() end

--- Returns the native first unread book's stable ID, or nil; does not acknowledge it.
---@return string?
function player.getNewCodexId() end

--- Calls a function in the specified quest script. ---
---@param questId string
---@param functionName string
---@param ... LuaValue
---@return LuaValue
function player.callQuest(questId, functionName, ...) end

--- Returns `true` if the player can turn in the specified quest. ---
---@param questId string
---@return Bool
function player.canTurnInQuest(questId) end

--- Returns the effective quest's persisted JSON, or nil. Use currentQuestView for lightweight live HUD data. ---
---@return Json?
function player.currentQuest() end

--- Returns the native effective quest view; absent optional fields are omitted, while numeric zero remains present.
---@return {id:string, objectives?:Json[], progress?:number, compassDirection?:number}|nil
function player.currentQuestView() end

--- Returns the manually tracked ID, not necessarily the effective on-world quest; does not guarantee eligibility. ---
---@return string?
function player.currentQuestId() end

--- Makes the player perform a dance. Pass `nil` to stop dancing. ---
---@param dance? string
function player.dance(dance) end

--- Returns the effects animator callbacks table for the player. ---
---@return LuaTable
function player.effectsAnimator() end

--- Returns the player's nametag. ---
---@return string
function player.nametag() end

--- Returns quest information as JSON. ---
---@param questId string
---@return Json
function player.quest(questId) end

--- Returns a list of all quest IDs the player has. ---
---@return StringList
function player.questIds() end

---@class QuestLogEntry
---@field id string
---@field title string
---@field main boolean
---@field unread boolean
---@field tracked boolean Effective on-world-or-manual selection.
---@field currentWorld boolean
---@field portraitRevision integer

--- Native ordered, server-eligible active/completed show-in-log quests.
---@param completed boolean
---@return {entries:QuestLogEntry[], current?:string}
function player.questLogEntries(completed) end

---@class QuestLogDetails
---@field active boolean
---@field tracked boolean
---@field canAbandon boolean
---@field revision integer
---@field portraitRevision integer
---@field rewardsRevision integer
---@field title? string Omitted when text revision is unchanged.
---@field text? string
---@field portraitTitle? string Empty when the invalidated portrait is absent.
---@field portrait? Json[]
---@field rewards? Json[] Empty clears previous item rewards.

--- Live control flags plus independent text/portrait/reward deltas; tokens are process-local.
---@param questId string
---@param knownRevision? integer
---@param knownPortraitRevision? integer
---@param knownRewardsRevision? integer
---@return QuestLogDetails?
function player.questLogDetails(questId, knownRevision, knownPortraitRevision, knownRewardsRevision) end

--- Acknowledges the native read flag; unknown IDs are ignored.
---@param questId string
function player.markQuestRead(questId) end

--- False for unknown/inactive/denied quests; native errors propagate without a rollback guarantee.
---@param questId string
---@return boolean
function player.abandonQuest(questId) end

---@class QuestDialogView
---@field id string
---@field token integer Opaque process-local exact-quest/state decision lease.
---@field kind "failed"|"complete"|"offer"
---@field guiConfig? string Template override; Lua chooses default layouts.
---@field title string
---@field text string
---@field portrait Json[]
---@field portraitTitle string
---@field objectivePortrait Json[] Offer only; otherwise empty.
---@field objectivePortraitTitle string
---@field rewards Json[]
---@field money integer
---@field canDecline boolean
---@field preview boolean

--- Acquires/reuses a native pending decision lease without acknowledging it.
---@param kind "preview"|"failed"|"complete"|"offer"
---@return QuestDialogView?
function player.pendingQuestDialog(kind) end

--- Rejects replaced quests and state cycles; acknowledged ephemeral quests may outlive the map entry.
---@param token integer
---@return boolean
function player.questDialogValid(token) end

--- Acknowledges a successfully displayed request; invalid tokens return false.
---@param token integer
---@return boolean
function player.acknowledgeQuestDialog(token) end

--- Resolves completion cinema species/gender tags and JSON without playing it; native errors propagate.
---@param token integer
---@return Json?
function player.questDialogCinema(token) end

--- Retires before executing a decision; invalid/repeated tokens return false, native errors do not guarantee rollback.
---@param token integer
---@param decision "accept"|"decline"|"cancel"|"close"
---@return boolean
function player.finishQuestDialog(token, decision) end

--- Returns the objectives for the specified quest. ---
---@param questId string
---@return Json
function player.questObjectives(questId) end

--- Returns native drawables for the named portrait, or nil when absent.
---@param questId string
---@param portraitName string
---@return Json[]?
function player.questPortrait(questId, portraitName) end

--- Returns the state of the specified quest. ---
---@param questId string
---@return string
function player.questState(questId) end

--- Removes a teleport bookmark. Returns `true` if successful. ---
---@param bookmarkConfig Json
---@return Bool
function player.removeTeleportBookmark(bookmarkConfig) end

--- Returns a list of server quest IDs. ---
---@return StringList
function player.serverQuestIds() end

--- Sets an animation parameter for the player's humanoid animator. ---
---@param parameterName string
---@param value Json
function player.setAnimationParameter(parameterName, value) end

--- Sets the player's nametag. ---
---@param nametag? string
function player.setNametag(nametag) end

--- Sets the currently tracked quest. ---
---@param questId string
function player.setTrackedQuest(questId) end

--- Makes the player stop lounging. ---
function player.stopLounging() end

--- Returns the ID of the currently tracked quest.
---@return string
function player.trackedQuestId() end
