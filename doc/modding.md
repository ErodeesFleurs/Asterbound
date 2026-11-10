# Mod compatibility and diagnostics

[Project overview](../README.md) · [Lua API index](lua/README.md) · [JSON reference](json/README.md) · [Editor setup](lua/stubs/README.md)

Many StarExtensions mods remain compatible; StarExtensions itself is deprecated. Start with the compatibility boundary for your target engine, migrate affected Lua scripts, then check bundled repairs and real mod dependencies. For render-hook timing and presentation behavior, see [independent simulation and rendering](engine-notes.md#independent-simulation-and-rendering).

- [Compatibility identity and limits](#asterbound-compatibility-identity)
- [Lua 5.5.1 migration](#lua-551-migration)
- [Bundled compatibility repairs](#bundled-mod-compatibility)
- [Required third-party dependencies](#required-third-party-dependencies)
- [Error diagnostics](#mod-error-diagnostics)
- [Script failure isolation](#script-failure-isolation)

## Asterbound compatibility identity

Asterbound is the current product; OpenStarbound is the upstream and compatibility API/protocol source. Branding, network compatibility, matching assets and save compatibility are separate concerns.

| Boundary | Retained contract |
| --- | --- |
| Network versions | Starbound protocol `747`, OpenStarbound extension protocol `18`, stream version `18` |
| Handshake | `brand = "Asterbound"`; existing `openProtocolVersion`, compression negotiation, packet IDs and field layouts |
| Mod/resource identity | `opensb_base`, `opensb_compat`, existing virtual asset paths and entity IDs |
| Script/input identity | Existing `OpenStarbound` script-context names and the `opensb` input group |
| Public protocol APIs | `clientOpenProtocolVersion` / `serverOpenProtocolVersion` |
| Save identifiers | `SBVJ01` save header and `World4` world identifier |

The existing handshake does not negotiate a common minimum version. Accepting a version number is not proof of compatibility with a future implementation. New Asterbound-only assets should use their own namespace rather than rename resources referenced by existing mods, saves or remote clients. The `doc/lua/openstarbound/` and `doc/json/openstarbound/` reference paths also remain stable.

Brand artwork and scripts change the asset digest. A server requiring identical assets can reject an otherwise wire-compatible client. Do not disable this policy globally or advertise digest equality; deploy matching assets, or explicitly allow the difference on both endpoints when appropriate.

### Scope of interoperability evidence

The rebrand was smoke-tested on Linux with preserved local OpenStarbound binaries from source `90296aed875c749bdc465eb8b0ce1d74faa511a9`: real TCP in both directions, None and Zstd stream modes, world entry, reconnection and chat round trips. Both brands also connected concurrently to Asterbound. These checks used owned base assets and explicitly allowed differing extension assets. They are not a full extended-world/gameplay matrix, a test of every upstream release, native Windows/macOS validation, or proof of Steam/Discord interoperability.

## Lua 5.5.1 migration

Compound assignment, safe navigation, `math.round` and `math.copysign` remain available. They are runtime extensions, not a guarantee of support in other engines or a system Lua interpreter.

| Migration point | Action or behavior |
| --- | --- |
| Reserved `global` | Rename the identifier or access the global field with `_ENV["global"]`. |
| Read-only loop variables | Numeric loop variables and the first generic-loop variable cannot be assigned. Use a local copy when the body needs to change a value. |
| Lua 5.3 bytecode | Rebuild it for the target runtime; do not reuse old bytecode. |
| Finalizers and random numbers | Finalizer errors become warnings; `math.random` sequences may change. |
| Text templates | Use `<name>` and `<<` for a literal `<`, not brace-format placeholders. |

Asterbound repairs vanilla `/scripts/actions/flying.lua` and `/stagehands/coordinator/npccombat.lua` in memory, preserving their fractional sample bounds. Mod scripts still need migration; the vanilla repairs do not migrate arbitrary scripts.

Use the [Lua API index](lua/README.md) to find the script host and related tables, and the [stub setup guide](lua/stubs/README.md) for LuaLS. Editor completion is not proof that an API exists in a particular runtime context.

## Bundled mod compatibility

### Loading the bundled sources

The physical names and stable metadata names serve different purposes:

| Repository source | Distribution pack | Metadata `name` |
| --- | --- | --- |
| `assets/asterbound` | `asterbound.pak` | `opensb_base` |
| `assets/asterbound-compat` | `asterbound-compat.pak` | `opensb_compat` |

The compatibility source contains optional fixes and does not require any third-party mod to be installed. Distributions ship it beside the main pack; Nix packages install it as a sibling directory. Normal asset-directory scanning loads it automatically. Custom boot configurations using explicit `assetSources` must include it and use the current physical names. Do not keep duplicate old/new bundles in the same scanned directory.

The virtual paths `/scripts/opensb/`, `/interface/opensb/` and other existing resource paths remain unchanged. A physical directory or pack rename is not a reason to rename mod references.

### Guarded repairs

These are provider/version/content-guarded compatibility fixes, not global suppression of invalid patches or Lua errors. Unknown versions, changed content and foreign providers are not blindly patched. No installed mod files or saves are rewritten; restart the game after installing the compatibility source.

For [StarCore Simplified Chinese](https://steamcommunity.com/sharedfiles/filedetails/?id=807695810) (`SChinese`, version `V108_241106_09`), the source removes four unused imports from `/scripts/replaceTags.lua` in memory, breaking `util.lua → replaceTags.lua → util.lua`. The replacement function body, utility functions and translations remain unchanged; the engine still rejects cyclic `require` calls. This repair applies only when the effective script provider is that known SChinese version and its import header matches the [verified upstream release](https://github.com/sffxzzp/Starbound-Chinese/releases/tag/UTC-241105-1953). Other providers, unknown versions and unrecognized headers are left untouched.

Additional guarded repairs cover:

| Mod or area | Repair and limits |
| --- | --- |
| Arcana 1.4.2.9 / 1.4.2.10 | Its `dump` helper uses a local display key instead of assigning to Lua 5.5's read-only loop variable. Deployment and relic behavior are retained. |
| Known translation/UI and weather patches | Preserve translated fields and registered patch order while adapting changed schemas. SChinese's complete matter-manipulator translation follows QuickbarMini's active `.original.config` pane; Project45 loot changes follow the relocated treasure pools. |
| Neia | Repair projectile image fields and register its bundled spear-throw ability. Existing spear-throw mappings supplied by another mod are preserved. See the real dependency below. |
| Draconis | Repair staff emitters and the pet-spawner path. |
| PropPack | Repair the bundled upper-bed callback and empty drop pool. |
| Halloween | Repair malformed path/art references; the absent `pumpkinfrag` physics definition is not guessed. Its two unresolved projectile references remain an upstream defect. |
| Brewology / docking bay | Repair frame aliases. |
| Titancorp 2.4.4 / 2.4.5 | Repair the byte-identical duplicate projectile and powered food-maker integration. Verified object, recipe and implementation fingerprints guard the repair; authored recipes and power behavior are retained. |
| The Forge: Blade Dance 0.7.1 | Make the five custom-weapon recipes' global carrier-image paths absolute without replacing or changing their encoded artwork. Already-saved custom item descriptors are not migrated. |
| Advanced Teleporter Interface 1.1.1 | Retain concise inspection warnings and keep complete caught diagnostics in Debug logs instead of embedding native stacks in every warning. |

Rebuild the engine and compatibility source together when building from source: source-owned patch/load-script replacement uses the [current Assets APIs](lua/openstarbound/assets.md).

## Required third-party dependencies

The compatibility pack does **not** download, install or fabricate dependencies.

| Affected mods | Required base mod | Asset source to check in the log |
| --- | --- | --- |
| Voyage addons | [Voyage](https://steamcommunity.com/sharedfiles/filedetails/?id=2957477338) | `MWHArcanaAddon` |
| Neia harpoons | [Elithian Races Mod](https://steamcommunity.com/sharedfiles/filedetails/?id=850109963) | `Elithian_Races_Mod` |

Voyage's missing items, builders and NPC scripts are not fabricated. Neia's harpoon physics preset and return sound are absent from Neia itself; the compatibility source does not redistribute the missing audio or substitute unrelated sounds. Installing Elithian also installs its gameplay content.

After installing dependencies, restart and check the actual asset-source names. A translation or addon with a similar name is not the base mod. Alternatively, disable the addons requiring the missing base rather than manufacturing its items or scripts.

## Mod error diagnostics

### Reproducing and collecting logs

1. Reproduce with separate test storage, mods and logs, not a production character or universe. Follow the [runtime setup instructions](../README.md) for your distribution and confirm which asset sources were loaded.
2. Check the first diagnostic and its target, patch and script providers before removing mods. A provider identifies the executing code or resource, not necessarily the cause of a conflict.
3. Enable Debug logging when the concise summary is insufficient. For installed commands on `PATH`:

   ```sh
   asterbound -loglevel debug
   # Or run the dedicated server:
   asterbound_server -loglevel debug
   ```

   For a local build, use the executable path from the [build guide](building.md) and append the same option. Use your isolated runtime configuration with either command.
4. Keep the complete relevant diagnostic, including Debug details and the loaded asset-source list. Do not report only the last cascading error.

| Option | Effect |
| --- | --- |
| `-loglevel debug` | Includes complete original exception chains, Lua tracebacks and native stacks in Debug records. |
| `-verbose` | Enables stdout logging; does **not** enable Debug. |
| `-quiet` | Disables stdout logging, not the configured log file. |

### What provider information means

Lua runtime errors retain their original traceback and append a deduplicated `Lua source providers` summary for scripts on the failed stack, including coroutine failures. Engine-loaded scripts use their actual loaded provider: the mod's `friendlyName`, internal `name`, and directory or `.pak` path. Syntax and indexed-script read failures also include provider context. Missing or invalid optional metadata falls back to the source path. Chunk names remain unchanged; provider snapshots are recorded during loading, not by scanning mods every frame.

JSON and image Lua patch errors identify the target asset, patch path and exact executing patch provider, even when another mod overrides the same virtual patch path. Asset `onLoad`/`postLoad` failures also include provider metadata. Diagnostics do not track configuration changes field by field. Dynamically generated code without a registered provider retains its original error information.

Database per-file validation adds the concrete asset path and provider where available, including valid JSON with missing or invalid fields. More specific nested asset and patch attribution takes precedence. Recoverable entries still log and skip or use their existing fallback; diagnostic context does not make them fatal.

### Summaries, full details and failure limits

Error and warning logs use concise, reason-first summaries. Lua syntax/runtime failures, Lua patch syntax/runtime failures and resource failures have separate categories, with execution phase, live script locations and providers where available. Typed causes survive native callbacks and ordinary Lua rethrows. Repeated wrapper contexts and identical provider labels do not obscure the original reason; distinct call sites remain visible.

Full diagnostic rendering is deferred until a Debug sink is enabled; ordinary errors and warnings do not resolve native stacks. Final fatal dialogs and stderr summaries stay concise when the owning logger has reported the error. Failures before a logger is available, or while writing the log itself, retain full emergency diagnostics on stderr. Bootstrap failures before log settings are applied cannot be controlled by `-loglevel`. Error objects and programmatic full diagnostics are unchanged.

Native errors capture stack locations at their origin but defer symbol resolution until full details are requested. Copies share immutable diagnostic payloads; short and full renderings are cached independently and support concurrent readers. Captured addresses and module-relative locations remain in full details when symbols or a loaded module are no longer available. Related cleanup failures retain both diagnostics without changing the primary error's classification.

Native callback errors keep their string-valued Lua representation; built-in `error`/`pcall` behavior is unchanged. Summary transport is identity-bound and bounded. Deliberately rewritten errors are treated as new errors unless a complete known payload with a verified live Lua location prefix is preserved. This avoids guessing a mod or reusing a previous failure's attribution.

## Script failure isolation

Ordinary Lua load, initialization, update, message-handler and shutdown-hook errors stay local to gameplay script hosts: techs, active items, companions, deployment, entities, scripted windows and world/universe contexts. The failed session still logs its diagnostic and is disabled until reinitialization; `eval` failures retain their usable context. Hosts continue their existing empty-result fallback and remaining native work instead of escalating a script error into a lost game session. An NPC with a failed script can be removed normally, and a failed entity message can receive an unhandled-message response without disconnecting its sender.

This is **not** a main-loop catch-all. Asset acquisition, host setup, script-storage persistence, callback cleanup, thread reclamation and logging failures still propagate. If script execution and cleanup both fail, both diagnostics are retained and the infrastructure failure takes precedence for recovery decisions. Explicit host failures preserve their non-recoverable identity through native callbacks and unchanged Lua error rethrows; ordinary game-API and argument errors keep their normal Lua behavior. Built-in `pcall` and `error` are not replaced.

Native member callbacks registered from a script run inside the issuing component's execution boundary, even when the callback targets a different pane. Registration only borrows the issuer; it does not extend the component's lifetime. Session retirement revokes callbacks before the issuer's storage is released. Retired session callbacks remain inert, and a callback failure disables its issuer rather than the target pane.
