# species

[JSON reference](README.md#base-formats) · [Extensions](openstarbound/species.md) · [Modding guide](../modding.md)

Files with the `.species` extension define a humanoid species for players and NPCs.

## `humanoidConfig`
The path to the default humanoid config for the species, defaults to `/humanoid.config` when not defined.

## `humanoidOverrides`
Is a JsonObject that gets merged on top of the config file, which results in the default humanoid config for the species.
