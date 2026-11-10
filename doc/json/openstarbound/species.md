# species

[JSON reference](../README.md#openstarbound-compatible-extensions) · [Base format](../species.md) · [Modding guide](../../modding.md)

This page documents extensions to the [base species format](../species.md).


## `buildScripts`
An optional array of paths to Lua scripts used to dynamically change the player/NPC's humanoidConfig based on their identity and humanoidParameters, much like item build scripts modify an item's resulting config.

See [`assets/asterbound/humanoid/opensb/build.lua`](../../../assets/asterbound/humanoid/opensb/build.lua) for an example.

`build(JsonObject identity, JsonObject humanoidParameters, JsonObject humanoidConfig, Variant<JsonObject, nil> npcHumanoidConfig)` is invoked and must return a valid `JsonObject humanoidConfig` or the humanoid will fail to initialize correctly.

It is run whenever the player/npc initializes, changes species or imagePath in their identity, or the player/npc callback `refreshHumanoidParameters()` is used.

- `identity` is the specific identity of the player/npc it contains their customization data
- `humanoidParameters` are expected to be merged on top of `humanoidConfig`, they can also contain arbitrary data the build script can use to decide how to modify the output.
- `humanoidConfig` is the default humanoid config for the current species.
- `npcHumanoidConfig` is an optional argument that will be supplied when a NPC variant has its own unique config, such as the story NPCs. In all other cases it will be `nil`

## `createScripts`
An optional array of paths to Lua scripts used to create humanoidIdentity, humanoidParameters and equipment during character creation.

See [`assets/asterbound/humanoid/opensb/create.lua`](../../../assets/asterbound/humanoid/opensb/create.lua) for an example.
