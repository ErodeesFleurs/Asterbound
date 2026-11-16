---@meta

--- root API

--- Returns the variant index starting from 0 for the material variant used at the specified **tilePosition**. **layer** must be either `"foreground"`, `"background"` or `"platform"`, and **matVariants** must be the amount of variants the material has, which you can use the vanilla `root.materialConfig` function to fetch.
---@param tilePosition table
---@param layer string
---@param variants integer
function root.materialVariant(tilePosition, layer, variants) end

--- Works like `root.materialVariant` but for matmods.
---@param tilePosition table
---@param layer string
---@param variants integer
function root.modVariant(tilePosition, layer, variants) end

--- Returns an array containing all assets with the specified file extension. By the way, here's a list of every file extension the game does Special Things™ for when loading assets. - Items: `item`, `liqitem`, `matitem`, `miningtool`, `flashlight`, `wiretool`, `beamaxe`, `tillingtool`, `painttool`, `harvestingtool`, `head`, `chest`, `legs`, `back`, `currency`, `consumable`, `blueprint`, `inspectiontool`, `instrument`, `thrownitem`, `unlock`, `activeitem`, `augment` - Materials: `material`, `matmod` - Liquids: `liquid` - NPCs: `npctype` - Tenants: `tenant` - Objects: `object` - Vehicles: `vehicle` - Monsters: `monstertype`, `monsterpart`, `monsterskill`, `monstercolors` - Plants: `modularstem`, `modularfoliage`, `grass`, `bush` - Projectiles: `projectile` - Particles: `particle` - Name Gen: `namesource` - AI Missions: `aimission` - Quests: `questtemplate` - Radio Messages: `radiomessages` - Spawn Types: `spawntypes` - Species: `species` - Stagehand: `stagehand` - Behaviors: `nodes`, `behavior` - Biomes: `biome`, `weather` - Terrain: `terrain` - Treasure: `treasurepools`, `treasurechests` - Codex Entries: `codex` - Collections: `collection` - Statistics: `event`, `achievement` - Status Effects: `statuseffect` - Functions: `functions`, `2functions`, `configfunctions` - Tech: `tech` - Damage: `damage` - Dances: `dance` - Effect Sources: `effectsource` - Command Macros: `macros` - Recipes: `recipe` ---
---@param extension string
---@return string[]
function root.assetsByExtension(extension) end

--- Returns binary-safe raw bytes. Embedded paths require allowRemote=true and safe.allowRemoteAssets=true.
--- Default opt-in is false, including cache hits. Never execute untrusted bytes.
---@param path string
---@param allowRemote? boolean
---@return string
function root.assetData(path, allowRemote) end

--- Returns the asset source path of an asset, or nil if the asset doesn't exist. If you specify getPatches as true then also returns the patches for the asset as an array, each element containing the source path and patch path in indexes 1 and 2 respectively. ---
---@param path string
---@param getPatches boolean
---@return string, LuaTable?
function root.assetOrigin(path, getPatches) end

--- Without metadata: Returns an array with all the asset source paths. With metadata: Returns a table, key/value being source path/metadata. ---
---@param withMetadata? boolean
---@return LuaTable
function root.assetSourcePaths(withMetadata) end

--- Returns the metadata of an asset source. ---
---@param path string
---@return Json
function root.assetSourceMetadata(path) end

--- Returns an image. ---
---@param image string
---@return Image
function root.assetImage(image) end

--- Returns an array containing a `file` (the frames file used for the image) and `frames` (the frame data of the image). ---
---@param imagePath string
---@return Json
function root.assetFrames(imagePath) end

--- Returns a list of asset sources which patch the specified asset and the paths to those patches. ---
---@param asset string
---@return JsonArray
function root.assetPatches(asset) end

--- Gets a configuration value in `/storage/starbound.config`. ---
---@param key string
---@return Json
function root.getConfiguration(key) end

--- Gets a configuration value in `/storage/starbound.config` by path. *Both getters will error if you try to get `title`, as that can contain the player's saved server login.* ---
---@param path string
---@return Json
function root.getConfigurationPath(path) end

--- Sets a configuration value in `/storage/starbound.config`. ---
---@param key string
---@param value Json
---@return Json
function root.setConfiguration(key, value) end

--- Sets a configuration value in `/storage/starbound.config` by path. *Both setters will error if you try to set `safeScripts`, as that can break Starbound's sandbox.* ---
---@param path string
---@param value Json
---@return Json
function root.setConfigurationPath(path, value) end

--- Persistent JSON storage shared by all scripts in one Root/storageDirectory, including its built-in server.
--- Works with safeScripts enabled; not a sandbox between mods. Use separate storage directories for independent processes/installations.
--- Defaults to the directly calling asset script's provider (including helpers/overrides); loadstring has no default namespace.
--- Optional mod selects the first loaded source with matching _metadata.name; unknown names error.
--- Returns a copy, or nil when absent; prefix keys with character/world IDs when that scope is needed.
---@param key string
---@param mod? string
---@return Json
function root.getModStorage(key, mod) end

--- Stores a JSON value copy; nil deletes. Unsupported Lua values error; use jarray() for an empty array.
--- The full serialized UTF-8 JSON object (escaped keys and punctuation included) is limited to 4,194,304 bytes per mod.
--- Values allow at most 255 nested containers (256 including the namespace object); non-finite numbers error and file integers must fit int64.
--- Rejected sets leave data unchanged. Writes batch about two seconds after the first pending change and at orderly shutdown.
--- Files are modstorage/mod-<lowercase SHA256 of namespace UTF-8>.json under storageDirectory; old OpenStarbound files are not imported.
--- Unreadable, oversized or malformed files error instead of being silently emptied or overwritten.
---@param key string
---@param value Json
---@param mod? string
function root.setModStorage(key, value, mod) end

--- Returns all keys in the selected mod's store in no particular order. Uses the same source/name selection as getModStorage.
---@param mod? string
---@return string[]
function root.modStorageKeys(mod) end

--- Removes all keys in the selected mod's store. Uses the same source/name selection as getModStorage.
---@param mod? string
function root.clearModStorage(mod) end

--- Global barrier: waits for all mods' pending/in-flight writes, not only the caller's. Takes no mod argument.
--- Failures error and retain pending changes for retry; background failures are logged. A crash may lose unflushed changes.
function root.flushModStorage() end

--- Returns all recipes. ---
---@return JsonArray
function root.allRecipes() end

--- Returns the asset file path for the specified item. ---
---@param itemName string
---@return string
function root.itemFile(itemName) end

--- Returns the config for the species. ---
---@param species string
---@return JsonObject
function root.speciesConfig(species) end

--- Generates a random humanoid identity for the given species. Optionally supply a seed and or predetermined gender. Additionaly supplies a humanoidParameters object, as well as an object of the clothing that would be chosen with those choices humanoidParameters will have the `choices` value set to an array of the indexes of the choices made. ---
---@param species string
---@param seed? uint64_t?
---@param gender? String?
---@return Json, JsonObject, JsonObject
function root.generateHumanoidIdentity(species, seed, gender) end

--- Generates a the identity for the given choices on the character creation screen for the given species. Additionaly supplies a humanoidParameters object, as well as an object of the clothing that would be chosen with those choices humanoidParameters will have the `choices` value set to an array of the indexes of the choices made. Additional arguments **ext** are not used by the retail character creation, but will be passed to a species' character creation script if it is defined. ---
---@param name string
---@param speciesChoice string
---@param genderChoice size_t
---@param bodyColorChoice size_t
---@param alty size_t
---@param hairChoice size_t
---@param heady size_t
---@param shirtChoice size_t
---@param shirtColor size_t
---@param pantsChoice size_t
---@param pantsColor size_t
---@param personality size_t
---@param ... LuaValue
---@return Json, JsonObject, JsonObject
function root.createHumanoid(name, speciesChoice, genderChoice, bodyColorChoice, alty, hairChoice, heady, shirtChoice, shirtColor, pantsChoice, pantsColor, personality, ...) end

--- Returns the config for the status effect.
---@param effect string
---@return JsonObject
function root.effectConfig(effect) end
