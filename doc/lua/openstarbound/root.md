# Root

[Lua API](../README.md#openstarbound-compatible-extensions) · [Base API](../root.md) · [Modding guide](../../modding.md)

The `root` table now contains extra asset bindings and bindings to return the tile variant that is used for material and matmods at any position.

---

#### `unsigned` root.materialVariant(`Vec2U` tilePosition, `String` layer, `unsigned` variants)

Returns the variant index starting from 0 for the material variant used at the specified **tilePosition**. **layer** must be either `"foreground"`, `"background"` or `"platform"`, and **variants** must be the amount of variants the material has, which you can use the vanilla `root.materialConfig` function to fetch.

#### `unsigned` root.modVariant(`Vec2U` tilePosition, `String` layer, `unsigned` variants)

Works like `root.materialVariant` but for matmods.

---

#### `String[]` root.assetsByExtension(`String` extension)

Returns an array containing all assets with the specified file extension.

By the way, here's a list of every file extension the game does Special Things™ for when loading assets.

<details><summary><b>File Extensions</b></summary>

- Items: `item`, `liqitem`, `matitem`, `miningtool`, `flashlight`, `wiretool`, `beamaxe`, `tillingtool`, `painttool`, `harvestingtool`, `head`, `chest`, `legs`, `back`, `currency`, `consumable`, `blueprint`, `inspectiontool`, `instrument`, `thrownitem`, `unlock`, `activeitem`, `augment`
- Materials: `material`, `matmod`
- Liquids: `liquid`
- NPCs: `npctype`
- Tenants: `tenant`
- Objects: `object`
- Vehicles: `vehicle`
- Monsters: `monstertype`, `monsterpart`, `monsterskill`, `monstercolors`
- Plants: `modularstem`, `modularfoliage`, `grass`, `bush`
- Projectiles: `projectile`
- Particles: `particle`
- Name Gen: `namesource`
- AI Missions: `aimission`
- Quests: `questtemplate`
- Radio Messages: `radiomessages`
- Spawn Types: `spawntypes`
- Species: `species`
- Stagehand: `stagehand`
- Behaviors: `nodes`, `behavior`
- Biomes: `biome`, `weather`
- Terrain: `terrain`
- Treasure: `treasurepools`, `treasurechests`
- Codex Entries: `codex`
- Collections: `collection`
- Statistics: `event`, `achievement`
- Status Effects: `statuseffect`
- Functions: `functions`, `2functions`, `configfunctions`
- Tech: `tech`
- Damage: `damage`
- Dances: `dance`
- Effect Sources: `effectsource`
- Command Macros: `macros`
- Recipes: `recipe`
</details>

---

#### `String` root.assetData(`String` path)

Returns the raw data of an asset.

---

#### `String, Maybe<LuaTable>` root.assetOrigin(`String` path, [`bool` getPatches])

Returns the asset source path of an asset, or nil if the asset doesn't exist. If you specify getPatches as true then also returns the patches for the asset as an array, each element containing the source path and patch path in indexes 1 and 2 respectively.

---

#### `LuaTable` root.assetSourcePaths([`bool` withMetadata])

Without metadata: Returns an array with all the asset source paths.
With metadata: Returns a table, key/value being source path/metadata.

---

#### `Json` root.assetSourceMetadata(`String` path)

Returns the metadata of an asset source.

---

#### `Image` root.assetImage(`String` image)

Returns an image.

---

#### `Json` root.assetFrames(`String` imagePath)

Returns an array containing a `file` (the frames file used for the image) and `frames` (the frame data of the image).

---

#### `JsonArray` root.assetPatches(`String` asset)

Returns a list of asset sources which patch the specified asset and the paths to those patches.

---

#### `Json` root.getConfiguration(`String` key)

Gets a configuration value in `/storage/starbound.config`.

---

#### `Json` root.getConfigurationPath(`String` path)

Gets a configuration value in `/storage/starbound.config` by path.

*Both getters will error if you try to get `title`, as that can contain the player's saved server login.*

---

#### `Json` root.setConfiguration(`String` key, `Json` value)

Sets a configuration value in `/storage/starbound.config`.

---

#### `Json` root.setConfigurationPath(`String` path,  `Json` value)

Sets a configuration value in `/storage/starbound.config` by path.

*Both setters will error if you try to set `safeScripts`, as that can break Starbound's sandbox.*

---

### Mod storage

Persistent JSON key/value storage, available with `safeScripts` enabled. This is **not a sandbox between mods**.

- **Source:** without `mod`, access uses the asset provider of the directly calling script, including shared helpers and overridden scripts. Code created by `loadstring` has no default namespace. An explicit `mod` selects the first loaded source whose `_metadata.name` matches; unknown names error. Both forms resolve the same source identity.
- **Identity:** the namespace uses metadata `name`, or the source basename as fallback. If loaded sources resolve to the same natural namespace (for example duplicate unnamed basenames), the first keeps it and others receive a disambiguated identity, cached for this Root's lifetime. This does not bypass asset loading's duplicate-mod-name filtering.
- **Scope:** all scripts, characters, worlds and threads in one Root share the store, including its built-in server. Independent installations/processes must use separate `storageDirectory` paths; sharing a directory is not supported. For character/world data, prefix keys with an appropriate stable ID, for example `player.uniqueId() .. ".settings"`.
- **Values:** normal Lua-to-JSON conversion applies; functions and other unsupported values error. `nil` deletes a key. Tables are copied into/out of storage; mutating a returned table does not save it. Use `jarray()` for an empty array. Non-finite numbers are rejected; file integers must fit signed 64-bit range. Stored values may nest at most 255 containers (256 including the namespace object).
- **Limit:** each mod's complete serialized UTF-8 JSON object, including escaped keys, punctuation and nested values, may occupy at most **4 MiB (4,194,304 bytes)**. A rejected set leaves the stored data unchanged.
- **Persistence:** changes are batched about two seconds after the first pending change and flushed at orderly shutdown. A crash can lose pending changes. Explicit flush waits for all mods' pending writes, including in-flight writes; failure raises an error and retains pending changes for retry. Background write failures are logged.
- **Files:** `<storageDirectory>/modstorage/mod-<lowercase SHA256 of namespace UTF-8>.json`, plain JSON. Read/edit only while the application is closed. Unreadable, oversized or malformed files error rather than becoming empty stores; they are not automatically overwritten. Old OpenStarbound mod-storage files are **not imported automatically**.

#### `Json` root.getModStorage(`String` key, [`String` mod])

Returns a copy of the stored value, or `nil` if absent.

#### `void` root.setModStorage(`String` key, `Json` value, [`String` mod])

Stores a value, or removes the key for `nil`. An unchanged value does not schedule another write.

#### `JsonArray` root.modStorageKeys([`String` mod])

Returns the stored keys in no particular order.

#### `void` root.clearModStorage([`String` mod])

Removes all keys in the selected mod's store.

#### `void` root.flushModStorage()

A global write barrier for **all mods**, not just the caller; takes no mod argument. Returns only after pending writes complete, or errors on failure. Source resolution, initialization, file access and JSON errors are propagated by the storage APIs.

---

#### `JsonArray` root.allRecipes(`Maybe<StringSet>` filter)

Returns all recipes.

Optionally apply a filter for recipe groups.

---

#### `String` root.itemFile(`String` itemName)

Returns the asset file path for the specified item.

---

#### `JsonObject` root.speciesConfig(`String` species)

Returns the config for the species.

---

#### `LuaTupleReturn<Json, JsonObject, JsonObject>` root.generateHumanoidIdentity(`String` species, `Maybe<uint64_t>` seed, `Maybe<String>` gender)

Generates a random humanoid identity for the given species. Optionally supply a seed and or predetermined gender.

Additionaly supplies a humanoidParameters object, as well as an object of the clothing that would be chosen with those choices

humanoidParameters will have the `choices` value set to an array of the indexes of the choices made.

---

#### `LuaTupleReturn<Json, JsonObject, JsonObject>` root.createHumanoid(`String` name, `String` speciesChoice, `size_t` genderChoice, `size_t` bodyColorChoice, `size_t` alty, `size_t` hairChoice, `size_t` heady, `size_t` shirtChoice, `size_t` shirtColor, `size_t` pantsChoice, `size_t` pantsColor, `size_t` personality, `LuaVariadic<LuaValue>` ext)

Generates a the identity for the given choices on the character creation screen for the given species.

Additionaly supplies a humanoidParameters object, as well as an object of the clothing that would be chosen with those choices

humanoidParameters will have the `choices` value set to an array of the indexes of the choices made.

Additional arguments **ext** are not used by the retail character creation, but will be passed to a species' character creation script if it is defined.

---

#### `JsonObject` root.effectConfig(`String` effect)

Returns the config for the status effect.

---

#### `Json` root.materialConfig(`MaterialId` materialId)

Returns a JSON object containing the `path` and base `config` for the specified material by its ID if it is a real material, or `nil` if it is a metamaterial or invalid.

---

#### `Json` root.modConfig(`ModId` modId)

Returns a JSON object containing the `path` and base `config` for the specified mod by its ID if it is a real mod, or `nil` if it is a metamod or invalid.

---

#### `Json` root.monsterConfig(`String` typeName)

Returns the base config for the monsterType.

---

#### `Json` root.biomeConfig(`String` typeName)

Returns a json object of the config, name, and path of the given biome.

---

#### `Json` root.treeFoliageConfig(`String` typeName)

Returns a json object of the config, and path for the given foliage.

---

#### `Json` root.treeStemConfig(`String` typeName)

Returns a json object of the config, and path for the given stem.
