# Lua API reference

[Project overview](../../README.md) · [Modding guide](../modding.md) · [JSON reference](../json/README.md) · [Editor stubs](stubs/README.md)

Start with the API for the script's host, then follow its related tables. A table's presence in this reference or in editor completion does **not** make it available in every script context. Each page describes its own availability and behavior.

Asterbound uses Lua 5.5.1 with additional operators and helpers; see the [Lua migration notes](../modding.md#lua-551-migration) before porting older scripts. The reference is organized into base API pages and OpenStarbound-compatible extensions. This organization is not a guarantee that every documented call works in retail Starbound: some base pages also describe extended behavior.

The `openstarbound/` documentation paths and existing `opensb` API/resource identifiers are compatibility contracts, not the current product name. They remain unchanged.

## Base API

### Script services and shared tables

| Table or topic | Reference |
| --- | --- |
| Asset and game-data queries | [root](root.md) |
| Utility functions | [sb](utility.md) |
| Configuration parameters | [config](config.md) |
| Entity messaging | [message](message.md) |
| Script update rate | [script](updatablescript.md) |
| Administrative command checks | [CommandProcessor](commandprocessor.md) |

### Entities and gameplay

| Table or topic | Reference |
| --- | --- |
| World queries and actions | [world](world.md) |
| Common entity functions | [entity](entity.md) |
| Player | [player](player.md) |
| Player companions | [playerCompanions](playercompanions.md) |
| NPC | [npc](npc.md) |
| Monster | [monster](monster.md) |
| Object | [object](object.md) |
| Vehicle | [vehicle](vehicle.md) |
| Projectile | [projectile](projectile.md) |
| Stagehand | [stagehand](stagehand.md) |
| Quest | [quest](quest.md) |
| Tech | [tech](tech.md) |
| Item | [item](item.md) |
| Active item | [activeItem](activeitem.md) |
| Movement controller | [mcontroller](movementcontroller.md) |
| Actor movement controller | [actor mcontroller](actormovementcontroller.md) |
| Object physics regions | [physics](physics.md) |
| Status controller | [status](statuscontroller.md) |
| Status effect | [effect](statuseffect.md) |

### Animation and interfaces

| Table or topic | Reference |
| --- | --- |
| Networked animator | [animator](animator.md) |
| Local animation and drawing | [localAnimator](localanimator.md) |
| Animation configuration | [animationConfig](scriptedanimator.md) |
| Active-item animation | [activeItemAnimation](activeitemanimation.md) |
| Object animation | [objectAnimator](objectanimator.md) |
| Sky, ship and celestial data | [celestial](celestial.md) |
| Scripted panes | [pane](scriptpane.md) |
| Container panes | [pane](containerpane.md) |
| Widgets and Canvas | [widget](widget.md) |

## OpenStarbound-compatible extensions

These pages document additional or changed APIs available in Asterbound. When a base page exists, read it alongside the extension page. Do not assume these extensions exist in retail Starbound or that a client-only mod can supply server-side behavior; see [compatibility boundaries](../modding.md#asterbound-compatibility-identity).

### Assets, runtime and messaging

| Table or topic | Reference |
| --- | --- |
| Asset-loading scripts and Lua asset patches | [assets](openstarbound/assets.md) |
| Asset and tile queries | [root](openstarbound/root.md) |
| Build information, formatting, promises and binding lifetimes | [sb](openstarbound/utility.md) |
| Additional global functions (`loadstring`) | [Global functions](openstarbound/unsorted.md) |
| Additional data types (`AudioInstance`) | [Data types](openstarbound/datatypes.md) |
| Entity userdata | [Entity userdata](openstarbound/entityuserdata.md) |
| Script contexts and lifecycle hooks | [Hooks](openstarbound/hooks.md) |
| Asset-load error handlers | [Error handling](openstarbound/errorHandling.md) |
| Scriptable threads | [threads](openstarbound/threads.md) |
| Asynchronous HTTP requests | [http](openstarbound/http.md) |
| Universe/world administration | [universe](openstarbound/universe.md) |
| World extensions | [world](openstarbound/world.md) |

### Gameplay, animation and interfaces

| Table or topic | Reference |
| --- | --- |
| Player | [player](openstarbound/player.md) |
| NPC | [npc](openstarbound/npc.md) |
| Active item | [activeItem](openstarbound/activeitem.md) |
| Status effect | [effect](openstarbound/effect.md) |
| Scripted item drops | [itemDrop](openstarbound/itemdrop.md) |
| Networked animator | [animator](openstarbound/animator.md) |
| Local animator | [localAnimator](openstarbound/localanimator.md) |
| Animation configuration | [animationConfig](openstarbound/scriptedanimator.md) |
| Main interface and managed panes | [interface](openstarbound/interface.md) |
| Scripted-pane runtime | [pane](openstarbound/scriptpane.md) |
| Widgets | [widget](openstarbound/widget.md) |
| Chat | [chat](openstarbound/chat.md) |
| Camera | [camera](openstarbound/camera.md) |
| User input | [input](openstarbound/input.md) |
| Renderer and shaders | [renderer](openstarbound/renderer.md) |
| System clipboard | [clipboard](openstarbound/clipboard.md) |
| Songbook | [songbook](openstarbound/songbook.md) |

## Authoring and troubleshooting

- [Editor setup and LuaLS stubs](stubs/README.md): annotations for completion and diagnostics, not runtime implementations.
- [JSON resource reference](../json/README.md): resource fields used with these APIs.
- [Mod compatibility and diagnostics](../modding.md): Lua migration, bundled repairs, real dependencies, provider attribution and script failure isolation.
- [Independent simulation and rendering](../engine-notes.md#independent-simulation-and-rendering): update/render timing considerations.
