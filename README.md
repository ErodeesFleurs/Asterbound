# Asterbound

Asterbound is an independently maintained fork of OpenStarbound, derived from [Starbound 1.4.4](https://archive.org/details/starbound_source_code). It adds engine fixes, Lua-driven interfaces and rendering improvements while retaining OpenStarbound's protocol and mod compatibility contracts.

Upstream OpenStarbound repository: <https://github.com/OpenStarbound/OpenStarbound>.

**You must own Starbound.** Commercial game assets are not included.

## Installation

1. Obtain an available [release](https://github.com/ErodeesFleurs/Asterbound/releases), a [development build](https://github.com/ErodeesFleurs/Asterbound/actions), or [build from source](doc/building.md). Install separately from Starbound and OpenStarbound.
2. Copy `assets/packed.pak` from your owned Starbound installation into the new installation's `assets/` directory. Nix users instead set `ASTERBOUND_ASSETS` as shown below.
3. Import saves and local mods explicitly; never let two running installations write the same storage. See [runtime paths and data import](doc/building.md#runtime-paths-and-data-import). Keep Steam running to load subscribed Workshop mods when Steam integration is enabled.

Development artifacts are not stable releases. Keep backups before importing saves.

## Features

- Multithreaded lighting, extended colors and additive point lights.
- Lua-managed interfaces, asset/image patches, custom keybindings and status HUDs.
- Positional Opus voice chat, character swapping, multiple fonts and instrument volume control.
- Inventory recovery, mod compatibility fixes and client-side tile prediction. Custom tile collision requires Asterbound or a compatible OpenStarbound server.
- Independent simulation and rendering, configurable FPS limits and interpolated world presentation.

Use `/help` in game or read the [command reference](assets/asterbound/help.config.patch).

## Building

On Linux with Nix, run from the repository root:

```sh
nix build
ASTERBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/asterbound
```

Native builds require C++23, CMake, Ninja and the pinned xclang toolchain. See [requirements and platform instructions](doc/building.md#requirements); GCC, Apple Clang and MSVC builds are unsupported.

## Documentation

| Task | Guide |
|---|---|
| Install, build, configure SDKs or import saves | [Build and runtime guide](doc/building.md) |
| Run tests, sanitizers or benchmarks | [Testing guide](doc/testing.md) |
| Work on C++, Lua embedding or brand artwork | [Development guide](doc/development.md) |
| Understand networking and simulation/render ownership | [Engine notes](doc/engine-notes.md) |
| Migrate or diagnose mods | [Mod compatibility and diagnostics](doc/modding.md) |
| Find script APIs and their available contexts | [Lua API index](doc/lua/README.md) |
| Find asset configuration extensions | [JSON reference index](doc/json/README.md) |

## Compatibility and provenance

The wire baseline is Starbound protocol **747**, OpenStarbound extension protocol **18**, and stream version **18**. Physical asset directories and packages use Asterbound names; existing `opensb` resource identities, public Lua names and save-format identifiers remain compatibility contracts. See [compatibility scope and verified boundaries](doc/modding.md#asterbound-compatibility-identity); this is not a guarantee for every older or future OpenStarbound release.

[`version.json`](version.json) records product, bundled-asset and base-game versions separately. Bundled assets must match the engine's asset version.

Original attribution and copyright notices are retained. [Third-party notices](doc/OPENSOURCE.md) are not a blanket license for the game source or assets; renaming does not grant redistribution rights.

## Community

[Issues and contributions](https://github.com/ErodeesFleurs/Asterbound/issues) are welcome.
