# Lua Stub Files

This directory contains Lua stub files with EmmyLua/LuaLS annotations for OpenStarbound's Lua API.

## Purpose

These stub files provide:
- IDE autocompletion support for OpenStarbound's Lua APIs
- Type checking and parameter validation
- Function documentation directly in your editor
- Better development experience when writing mods

## Usage

### In this repository

Open the repository root in an editor using a Lua Language Server (LuaLS) version supporting Lua 5.5, including Zed or VS Code's [Lua extension](https://marketplace.visualstudio.com/items?itemName=sumneko.lua). The root `.luarc.json` configures Lua 5.5, OpenStarbound's nonstandard operators and Starbound's `/path/to/script.lua` asset imports. Stubs are indexed automatically; do not execute or `require` them.

Only known engine entrypoints and JSON-configured UI callbacks are listed as diagnostic globals. Unused callback parameters are explicitly named with an `_` prefix and excluded from unused-local reports. Undefined globals, missing fields and argument type checks remain enabled.

### From another mod workspace

Add the entire stub directory, including `openstarbound/`, to that workspace's `.luarc.json`:

```json
{
  "runtime.version": "Lua 5.5",
  "workspace.library": ["/path/to/OpenStarbound/doc/lua/stubs"]
}
```

Configure asset import paths relative to your mod's own asset root. The repository's asset root is `assets/asterbound`; external mods should not copy that path unchanged.

### Command-line diagnostics

From the repository root, with `lua-language-server` installed:

```sh
lua-language-server --check=. --checklevel=Hint --logpath=/tmp/openstarbound-luals
```

On Nix, prefix the command with `nix shell nixpkgs#lua-language-server --command`.

## Contributing

Update the corresponding stub and Markdown API documentation when changing an engine API. Check signatures against the C++ callback registrations and Lua converters, not just the prose: optional arguments, variadic arguments and multiple return values must use their Lua representations.

## Notes

- `types.lua` defines shared value and userdata types; `runtime.lua` describes injected globals and the subset of commercial asset helpers used by the bundled scripts.
- Annotations use LuaLS syntax: `T?`, `T[]`, unions, tuple tables and multiple returns, rather than C++ `Maybe<T>`, `List<T>` or `Tuple<T...>` spellings.
- JSON input types remain constrained to JSON values. Scripts annotate known asset schemas and command argument contracts at their use sites; arbitrary JSON is not globally treated as `any`.
- Base and OpenStarbound extension files describe one API table, not separate runtime implementations.
- These declarations cover multiple script contexts. An API's presence in completion does not make it available in every context; consult its documentation.
