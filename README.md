# OpenStarbound

OpenStarbound is a fork of the [Starbound 1.4.4 source code](https://archive.org/details/starbound_source_code), with bug fixes, new features and performance improvements.

**You must own Starbound.** Commercial game assets are not included.

## Installation

1. Download the [latest release](https://github.com/OpenStarbound/OpenStarbound/releases/latest) and install or extract it separately from vanilla Starbound.
2. Copy `assets/packed.pak` from your Starbound installation into OpenStarbound's `assets/` directory.
3. Optionally copy `storage/` for saves/settings, `mods/` for local mods and `assets/user/` for instrument songs. Keep Steam open to load subscribed Workshop mods.

[Nightly builds](https://github.com/OpenStarbound/OpenStarbound/actions?query=branch%3Amain) may be less stable.

## Features

- Multithreaded, extended-color lighting and additive point lights.
- Lua asset/image patches, Lua-managed chat and status-effect HUDs, scripted windows and custom keybindings.
- Positional Opus voice chat, character swapping, multiple fonts and instrument volume control.
- Improved mod compatibility, inventory recovery, world storage and client-side tile prediction. Custom tile collision requires an OpenStarbound server.
- Independent simulation and rendering, with configurable FPS limits and interpolated world presentation.

Use `/help` for commands or browse the [command reference](assets/opensb/help.config.patch).

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
OPENSTARBOUND_ASSETS=/path/to/Starbound/assets ./result/bin/starbound
```

Nix supports x86_64 and ARM64. Writable data defaults to `$XDG_DATA_HOME/openstarbound` (`~/.local/share/openstarbound`); see [Nix options and runtime paths](doc/building.md#nix-on-linux) for overrides and package variants.

## Community

[Issues and contributions](https://github.com/OpenStarbound/OpenStarbound/issues) are welcome.

<details>
<summary>Discord invite</summary>

Reverse the text in this image to obtain the invite code (spam prevention):

<img width="380" height="70" alt="Reversed Discord invite code" src="https://github.com/user-attachments/assets/9db11a89-0c32-4034-8555-68b54e5918a8" />

</details>
