---@meta

--- world API

--- Returns whether the script is running on the server or client. ---
---@return boolean
function world.isServer() end

--- Returns whether the script is running on the server or client. ---
---@return boolean
function world.isClient() end

--- Actual client/server WorldTemplate name; nil without a template, possibly empty for unnamed worlds.
---@return string?
function world.name() end

---@class DamageBarDiscovery
---@field id EntityId
---@field type string
---@field position Vec2F
---@field team DamageTeam

---@class DamageBarView: DamageBarDiscovery
---@field health number
---@field maxHealth number
---@field name string

--- Unsorted native fuzzy-region query or complete known interest set; no UI filtering/ordering policy.
---@param region? RectF
---@param typeName? "Default"|"Special"|"None"
---@return DamageBarDiscovery[]
function world.damageBarEntities(region, typeName) end

--- Nil for absent/non-DamageBarEntity; domain errors propagate.
---@param id EntityId
---@return DamageBarView?
function world.entityDamageBar(id) end

---@class EntityPortraitView
---@field revision integer Callback-local; reset known token on init/rebind.
---@field drawables? Drawable[] Omitted when unchanged; empty array explicitly clears.
---@field bounds? RectF Cropped native bounds, omitted when unchanged.

--- Nil only if missing/nonportrait; unchanged returns revision-only table, with live native animation.
---@param id EntityId
---@param mode string
---@param knownRevision? integer
---@return EntityPortraitView?
function world.entityPortraitView(id, mode, knownRevision) end

---@class EntitySnapshot
---@field schema "asterbound.entitySnapshot"
---@field schemaVersion integer
---@field type string Native entity type.
---@field entityId EntityId Transient observation ID, not an import identity.
---@field position Vec2F
---@field authority "master"|"replica"
---@field complete boolean Authoritative single-entity persistence only; never promises a full remote player save.
---@field format "disk"|"net"
---@field data Json
---@field warnings? string[]

--- Synchronous local-world snapshot; absent entity returns nil, serialization failures error.
--- Other players are allowed, but replicas may lack inventory/quests/scripts and report incomplete.
--- Projectiles/plant drops use engine-version-specific network data, not a portable disk save.
---@param id EntityId
---@return EntitySnapshot?
function world.entitySnapshot(id) end

--- Attempts a wire connection between the two objects positioned at `outputPosition` and `inputPosition`. The `outputIndex` and `inputIndex` parameters specify which output and input nodes to connect. --- The following additional world bindings are available only for scripts running on the client. ---
---@param outputPosition Vec2I
---@param outputIndex number
---@param inputPosition Vec2I
---@param inputIndex number
function world.wire(outputPosition, outputIndex, inputPosition, inputIndex) end

--- Returns the entity ID of the player hosting the world. ---
---@return EntityId
function world.mainPlayer() end

--- Returns the current cursor aim position of the specified entity. ---
---@param entityId EntityId
---@return Vec2F
function world.entityAimPosition(entityId) end

--- Client-only: whether the client world is started, not whether a player entity is attached.
---@return boolean
function world.inWorld() end

--- Sets the amount of time to persist a ephemeral world when it is inactive. ---
---@param expiryTime number
function world.setExpiryTime(expiryTime) end

--- Returns a `String` representation of the world's id. ---
---@return string
function world.id() end

--- Calls a function in the specified world script context. ---
---@param contextName string
---@param functionName string
---@param ... LuaValue
---@return LuaValue
function world.callScriptContext(contextName, functionName, ...) end

--- Sends a raw network packet to the specified client that is currently connected to this world. The `packetType` must be the name of a known packet in `Star::PacketTypeNames` (for example `"EntityInteractResult"`) and `packetData` must contain the JSON representation expected by `Star::createPacket` for that packet. Returns `true` when the packet is queued successfully. ---
---@param clientId ConnectionId
---@param packetType string
---@param packetData Json
---@return boolean
function world.sendPacket(clientId, packetType, packetData) end

--- Attempts to replace existing materials with the specified material in the specified positions and layer. Positions with no preexisting material will be skipped. Returns `true` if the placement succeeds and `false` otherwise. ---
---@param positions Vec2I[]
---@param layerName string
---@param materialName string
---@param hueShift? number
---@param enableDrops boolean
---@return boolean
function world.replaceMaterials(positions, layerName, materialName, hueShift, enableDrops) end

--- Identical to world.replaceMaterials but applies to tiles in a circular radius around the specified center point.
---@param center Vec2F
---@param radius number
---@param layerName string
---@param materialName string
---@param hueShift? number
---@param enableDrops boolean
---@return boolean
function world.replaceMaterialArea(center, radius, layerName, materialName, hueShift, enableDrops) end
