# Asterbound

Asterbound is an independently maintained fork of [OpenStarbound](https://github.com/OpenStarbound/OpenStarbound), derived from the [Starbound 1.4.4 source code](https://archive.org/details/starbound_source_code). It retains OpenStarbound's networking and mod compatibility contracts while using its own application and distribution identity.

**You must own Starbound.** Commercial game assets are not included.

## Installation

1. Build from this repository or obtain a release from its [releases page](https://github.com/ErodeesFleurs/OpenStarbound/releases). Install separately from Starbound and OpenStarbound.
2. Copy `assets/packed.pak` from your owned Starbound installation into Asterbound's `assets/` directory.
3. Explicitly copy saves and local mods into the new installation; do not share writable storage with a running OpenStarbound instance. Nix users can use `asterbound-import-openstarbound` to copy into an empty runtime. Keep Steam open to load subscribed Workshop mods.

[Build workflows](https://github.com/ErodeesFleurs/OpenStarbound/actions) publish development artifacts; these are not a promise of a stable release.

## Features

- Multithreaded, extended-color lighting and additive point lights.
- Lua asset/image patches, Lua-managed chat and status-effect HUDs, scripted windows and custom keybindings.
- Positional Opus voice chat, character swapping, multiple fonts and instrument volume control.
- Improved mod compatibility, inventory recovery, world storage and client-side tile prediction. Custom tile collision requires an OpenStarbound server.
- Independent simulation and rendering, with configurable FPS limits and interpolated world presentation.

Use `/help` for commands or browse the [command reference](assets/asterbound/help.config.patch).

## Documentation

| Guide | Contents |
|---|---|
| [Building](doc/building.md) | Requirements, Nix, native CMake presets, cross-builds and SDKs |
| [Tests and benchmarks](doc/testing.md) | CTest, isolated asset-backed tests, sanitizers and benchmarks |
| [Development](doc/development.md) | Tooling, C++ conventions, threading, JSON, rendering and Lua embedding |
| [Mod compatibility and diagnostics](doc/modding.md) | Lua 5.5 migration, bundled compatibility fixes, dependencies and error logs |
| [Engine notes](doc/engine-notes.md) | Optimization details, networking, simulation/rendering ownership and presentation |
| [Lua API](doc/lua/openstarbound) / [JSON extensions](doc/json/openstarbound) | Scripting and asset references |

## Building

Requires **C++23**, **CMake 3.28+**, **Ninja 1.11+** and **xclang 23.1.3.6**. GCC, Apple Clang and MSVC builds are unsupported. See the [build guide](doc/building.md) for platform setup and packaging.

On Linux with Nix, from the repository root:

```sh
nix build --cores 16 --max-jobs 1
ASTERBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/asterbound
```

Nix supports x86_64 and ARM64. Writable data defaults to `$XDG_DATA_HOME/asterbound` (`~/.local/share/asterbound`); see [Nix options and runtime paths](doc/building.md#nix-on-linux) for overrides and package variants.

## Community

[Issues and contributions](https://github.com/ErodeesFleurs/OpenStarbound/issues) are welcome. The repository URL retains its existing path; the product name is Asterbound.

## Compatibility and provenance

The current wire baseline is Starbound protocol **747**, OpenStarbound extension protocol **18**, and stream version **18**. Existing `opensb` asset identities, public Lua API names and save-format identifiers remain compatibility contracts, not product branding. This does not guarantee compatibility with every older or future OpenStarbound release.

Product, bundled-asset and base-game versions are independently recorded in [`version.json`](version.json). Bundled assets must match the engine's asset version.

Original attribution and copyright notices are retained. [`doc/OPENSOURCE.md`](doc/OPENSOURCE.md) documents third-party components; it is not a blanket license for the game source or assets. Renaming does not grant redistribution rights. Commercial `packed.pak` is never included.
