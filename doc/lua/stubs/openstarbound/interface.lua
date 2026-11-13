---@meta

--- interface API
---@class interface
interface = {}

--- Queues a message popup at the bottom of the screen with an optional **cooldown** and **springState**. ---
---@param message string
---@param cooldown? number
---@param springState? number
function interface.queueMessage(message, cooldown, springState) end

--- Sets the HUD's visibility. ---
---@param visible boolean
function interface.setHudVisible(visible) end

--- Returns the HUD's visibility. ---
---@return boolean
function interface.hudVisible() end

--- Binds native registered IDs or dynamic Lua pane IDs; returns nil without an instance.
---@param paneName string
---@return LuaCallbacks?
function interface.bindRegisteredPane(paneName) end

--- Displays a registered pane. ---
---@param paneName string
function interface.displayRegisteredPane(paneName) end

--- Queries lifecycle display state without opening/binding callbacks; false without an instance.
--- A hidden pane that has not been dismissed still reports true.
---@param paneName string
---@return boolean
function interface.paneDisplayed(paneName) end

--- Existing instance is displayed and drawing-visible; does not create or bind it.
---@param paneName string
---@return boolean
function interface.paneVisible(paneName) end

--- Native toggle path or Lua manager ordinary toggle (honors retain).
---@param paneName string
function interface.togglePane(paneName) end

---@return {beamUp:boolean, beamDown:boolean, deploy:boolean, teamShip:boolean}
function interface.travelState() end

--- Revalidates native eligibility and submits normal travel; false if disallowed, not an arrival acknowledgment.
--- teamShip needs a current reachable member; malformed UUID strings/unknown operations raise errors.
---@param operation "beamUp"|"beamDown"|"deploy"|"teamShip"
---@param memberUuid? string
---@return boolean
function interface.travel(operation, memberUuid) end

--- Owning opaque native PortraitEntity userdata; no ID selects local Player even while detached.
---@param entityId? EntityId
---@return NativePortraitHandle?
function interface.portraitEntity(entityId) end

--- Cached native mouse-move geometry in physical pixels, not a fresh click position.
---@return {position:Vec2F, roundedPosition:Vec2I, size:Vec2I, scale:number}
function interface.cursorView() end

--- Native cached mouse-move cursor converted through the world camera.
---@return Vec2F
function interface.cursorWorldPosition() end

--- Reads Inventory ItemGrid new-item markers without clearing/acknowledging them.
---@return boolean
function interface.inventoryChanged() end

--- Binds the canvas widget on the main interface with the specified name as userdata for easy access. The `CanvasWidget` has the same methods as described in `widget.md`. - **ignoreInterfaceScale** is used to ignore the current interface scaling and bind the canvas with the screen size. ---
---@param name string
---@param ignoreInterfaceScale? boolean
---@return CanvasWidget
function interface.bindCanvas(name, ignoreInterfaceScale) end

--- Returns the scale used for interfaces.
---@return number
function interface.scale() end

--- Internal native item-tooltip fallback service; uses the current UI player.
--- Engine-produced opaque handles only, not item descriptors or createTooltip hook results.
---@param item NativeItemHandle
---@return NativePaneHandle
function interface.itemTooltip(item) end

--- Immediately loads a resolved client UI cinematic; native errors propagate, unlike deferred player playback.
---@param definition Json
function interface.playCinematic(definition) end

--- Whether the client cinematic overlay has not completed; independent of player/world attachment.
---@return boolean
function interface.cinematicActive() end

--- Requests the normal client return-to-title flow; native saving/disconnect/teardown are preserved.
--- Does not terminate the application. Requires a live game interface.
function interface.returnToTitle() end

--- Constructs an undisplayed generic script pane; Lua manages its lifetime.
---@param id string
---@param config Json
---@param layer string
function interface.createPane(id, config, layer) end

---@param id string
function interface.destroyPane(id) end

--- Calls an existing script pane; does not open it.
---@param id string
---@param method string
---@param arguments? Json[]
---@return Json?
function interface.callPane(id, method, arguments) end

--- Enqueues an event; prefer the shared ui Lua helpers for instance policy.
---@param id string
---@param event string
---@param payload Json
function interface.sendPaneEvent(id, event, payload) end

--- Checks reserved/native naming, not whether a dynamic instance already exists.
---@param id string
---@return boolean
function interface.paneIdAvailable(id) end

--- Returns dynamic pane IDs in native capture/layer/stack/modal input priority.
---@return string[]
function interface.paneInputTargets() end

--- Reports a top pane on the named native layer, including native windows; invalid layer names raise an error.
---@param layer string
---@return boolean
function interface.paneLayerOccupied(layer) end

---@return {hasPlayer:boolean, inWorld:boolean, playerGeneration:integer, worldGeneration:integer}
function interface.session() end

--- Available to client UI scripts; metadata is distinct from instance storage.
---@class client
client = {}

---@class ClientMetadata
---@field get fun(fallback?:Json):Json
---@field set fun(value:Json)

---@param namespace string
---@return ClientMetadata
function client.storage(namespace) end

--- Loaded by require "/interface/asterbound/ui/api.lua"; requests are asynchronous.
---@class ui
ui = {}

---@param id string
---@param definition {config:Json|string, layer?:string, scope?:string, open?:boolean, retain?:boolean, order?:number, actions?:table<string,string>, globalActions?:table<string,string>, events?:table<string,string>}
function ui.register(id, definition) end

---@param id string
function ui.open(id) end

--- Ordinary dismissal when displayed (honoring retain), otherwise opens the registered instance.
---@param id string
function ui.toggle(id) end

---@param id string
function ui.close(id) end

---@param id string
---@param event string
---@param payload Json
function ui.send(id, event, payload) end

--- Centrally rebound native TeamClient service; membership/RPC permissions remain native.
---@class team
team = {}

---@return boolean
function team.isMemberOfTeam() end

---@return string?
function team.currentTeam() end

---@param playerUuid? string
---@return boolean
function team.isTeamLeader(playerUuid) end

---@class TeamMemberView
---@field uuid string
---@field name string
---@field entity EntityId
---@field healthPercentage number
---@field energyPercentage number
---@field position Vec2F
---@field onWorld boolean
---@field canTravel boolean Member warp mode is not None; local travel must still be checked.
---@field leader boolean
---@field self boolean
---@field portraitRevision integer

---@return TeamMemberView[]
function team.members() end

--- Nil for unknown member OR unchanged revision; omit token for full view, empty portrait clears.
---@param uuid string
---@param knownRevision? integer
---@return {revision:integer, portrait:Json[]}?
function team.memberPortrait(uuid, knownRevision) end

--- Nondestructive pending query.
---@return boolean
function team.hasInvitationPending() end

--- Destructively clears pending flag, without accepting/declining.
---@return {uuid:string, name:string}?
function team.pullInvitation() end

--- Submits RPC with actual inviter UUID; no synchronous membership confirmation.
---@param inviterUuid string
function team.acceptInvitation(inviterUuid) end

---@param name string
function team.invitePlayer(name) end

--- No-op without local leader permission; remote result remains native.
---@param uuid string
function team.makeLeader(uuid) end

--- No-op unless local leader or removing self.
---@param uuid string
function team.removeFromTeam(uuid) end
