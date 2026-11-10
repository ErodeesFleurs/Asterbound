# JSON resource reference

[Project overview](../../README.md) · [Modding guide](../modding.md) · [Lua API](../lua/README.md)

This directory documents selected resource formats and fields, not a complete schema for every Starbound asset. Base pages describe shared formats; the `openstarbound/` pages describe extensions available in Asterbound. Those existing directory names remain stable for links and compatibility.

Resource paths such as `/humanoid/opensb/build.lua` are virtual asset paths. Repository paths such as `assets/asterbound/humanoid/opensb/build.lua` refer to files on disk; the physical Asterbound directory name does not replace the virtual `opensb` namespace.

## Base formats

| Format | Reference | Scope |
| --- | --- | --- |
| Animation | [animation](animation.md) | Format overview; see extensions for animator features |
| Species | [species](species.md) | Default humanoid configuration and overrides |

## OpenStarbound-compatible extensions

These fields are not a promise of retail Starbound support. Consult each page's restrictions and the [mod compatibility guide](../modding.md#asterbound-compatibility-identity), including client/server requirements.

| Format or resource | Reference | Scope |
| --- | --- | --- |
| Animation | [animation](openstarbound/animation.md) | Animator format version, includes, transformations, tags and flipped images |
| Species | [species](openstarbound/species.md) | Humanoid build and character-creation scripts |
| Humanoid configuration | [humanoidConfig](openstarbound/humanoidConfig.md) | Animator setup, states, attachment points and animation scripts |
| Armor items | [Armor item parameters](openstarbound/armorItem.md) | Cosmetic status effects, animation tags and appearance controls |
| Shared item fields | [Shared item parameters](openstarbound/items.md) | Rarity border and tooltip label |
| Objects | [object](openstarbound/object.md) | Retaining object parameters in dropped items |

## Related Lua APIs

- [Asset-loading and patch scripts](../lua/openstarbound/assets.md)
- [Networked animator extensions](../lua/openstarbound/animator.md)
- [Animation-script extensions](../lua/openstarbound/scriptedanimator.md)
- [Player extensions](../lua/openstarbound/player.md) and [NPC extensions](../lua/openstarbound/npc.md)
- [Full Lua API index](../lua/README.md)
