# Mod compatibility and diagnostics

[Project overview](../README.md) · [Lua API](lua/openstarbound) · [JSON extensions](json/openstarbound)

Many StarExtensions mods remain compatible; StarExtensions itself is deprecated. For render-hook timing and presentation behavior, see [independent simulation and rendering](engine-notes.md#independent-simulation-and-rendering).

## Lua 5.5.1 migration

Compound assignment, safe navigation, `math.round` and `math.copysign` remain available.

- `global` is reserved; rename it or use `_ENV["global"]`.
- Numeric loop variables and the first generic-loop variable are read-only. OSB repairs vanilla `/scripts/actions/flying.lua` and `/stagehands/coordinator/npccombat.lua` in memory, preserving their fractional sample bounds; mod scripts still need migration.
- Rebuild Lua 5.3 bytecode. Finalizer errors become warnings; `math.random` sequences may change.
- Text templates use `<name>` and `<<` for a literal `<`, not brace-format placeholders.

## Bundled mod compatibility

`assets/opensb-compat` is a separate, optional-fix asset source, shipped as `opensb-compat.pak` beside `opensb.pak` in distributions and as a sibling directory in Nix packages. Normal asset-directory scanning loads it automatically; custom boot configurations using explicit `assetSources` must include it. It does not require any third-party mod to be installed.

For [StarCore Simplified Chinese](https://steamcommunity.com/sharedfiles/filedetails/?id=807695810) (`SChinese`, version `V108_241106_09`), it removes four unused imports from `/scripts/replaceTags.lua` in memory, breaking `util.lua → replaceTags.lua → util.lua`. The replacement function body, utility functions and translations remain unchanged; the engine still rejects cyclic `require` calls. The repair applies only when the effective script provider is that known SChinese version and its import header matches the [verified upstream release](https://github.com/sffxzzp/Starbound-Chinese/releases/tag/UTC-241105-1953). Other providers, unknown versions and unrecognized headers are left untouched. No installed mod files or saves are rewritten; restart the game after installing the compatibility source.

Additional provider/version/content-guarded repairs cover:

- Arcana 1.4.2.9/1.4.2.10's `dump` helper: use a local display key instead of assigning to Lua 5.5's read-only loop variable; deployment and relic behavior are retained.
- Known translation/UI and weather patches: preserve translated fields and registered patch order while adapting changed schemas. SChinese's complete matter-manipulator translation follows QuickbarMini's active `.original.config` pane; Project45 loot changes follow the relocated treasure pools.
- Neia projectile image fields and registration of its bundled spear-throw ability, Draconis staff emitters and pet-spawner path, PropPack's bundled upper-bed callback and empty drop pool, Halloween's malformed path/art references, and Brewology/docking-bay frame aliases. Existing spear-throw ability mappings supplied by another mod are preserved.
- Titancorp 2.4.4/2.4.5's byte-identical duplicate projectile and powered food-maker integration. Verified object, recipe and implementation fingerprints guard the repair; authored recipes and power behavior are retained. Unknown versions, changed content and foreign providers are not blindly patched.
- The Forge: Blade Dance 0.7.1's five custom-weapon recipes: make their global carrier-image paths absolute without replacing or changing the encoded artwork. Already-saved custom item descriptors are not migrated.
- Advanced Teleporter Interface 1.1.1: retain concise inspection warnings and keep complete caught diagnostics in Debug logs instead of embedding native stacks in every warning.

Rebuild the engine and compatibility source together: source-owned patch/load-script replacement uses the [current Assets APIs](lua/openstarbound/assets.md). These are targeted compatibility fixes, not global suppression of invalid patches or Lua errors.

Voyage addons still require the real [Voyage base mod](https://steamcommunity.com/sharedfiles/filedetails/?id=2957477338); missing items, builders and NPC scripts are not fabricated. Halloween's absent `pumpkinfrag` physics definition is intentionally not guessed, so its two unresolved projectile references remain an upstream defect.

Neia's harpoons also require the real [Elithian Races Mod](https://steamcommunity.com/sharedfiles/filedetails/?id=850109963): their physics preset and return sound are absent from Neia itself. The compatibility source does not redistribute the missing audio or substitute unrelated sounds; installing Elithian also installs its gameplay content.

The compatibility pack does not download or install these dependencies. After installing them, restart and check for asset sources named `MWHArcanaAddon` (Voyage) and `Elithian_Races_Mod` in the log; a translation or addon with a similar name is not the base mod. Alternatively, disable the addons requiring the missing base rather than manufacturing its items or scripts.

## Mod error diagnostics

Lua runtime errors retain their original traceback and append a deduplicated `Lua source providers` summary identifying the scripts on the failed stack, including coroutine failures. Engine-loaded scripts use their actual loaded asset provider: the mod's `friendlyName`, internal `name`, and directory or `.pak` path. Syntax and indexed-script read failures also include provider context. Missing or invalid optional metadata falls back to the source path. Chunk names remain unchanged; provider snapshots are recorded during loading, not by scanning mods every frame.

JSON and image Lua patch errors identify the target asset, patch path, and the exact executing patch provider, even when another mod overrides the same virtual patch path. Asset `onLoad`/`postLoad` failures also include provider metadata. These diagnostics identify code and patch providers, not necessarily the mod that caused a conflict; they do not track configuration changes field by field. Dynamically generated code without a registered asset provider retains its original error information.

Error and warning logs present concise, reason-first summaries: Lua syntax/runtime failures, Lua patch syntax/runtime failures, and resource failures have separate categories, with execution phase, live script locations, and providers where available. Typed causes are retained through native callbacks and ordinary Lua rethrows; repeated wrapper contexts and identical provider labels do not obscure the original reason. Distinct call sites remain visible.

Start the client or server with `-loglevel debug` to include the complete original exception chains, Lua tracebacks and native stacks in Debug records. Full diagnostic rendering is deferred until a Debug sink is enabled; ordinary errors and warnings do not resolve native stacks. `-verbose` only enables stdout logging and does **not** enable Debug; `-quiet` disables stdout, not the configured log file. Final fatal dialogs and stderr summaries stay concise when the owning logger has reported the error. Failures before a logger is available, or while writing the log itself, retain full emergency diagnostics on stderr; bootstrap failures before log settings are applied cannot be controlled by `-loglevel`. Error objects and programmatic full diagnostics are unchanged.

Database per-file validation also adds the concrete asset path and provider where available, including valid JSON with missing or invalid fields. More specific nested asset and patch attribution takes precedence. Recoverable entries still log and skip or use their existing fallback; adding diagnostic context does not make those failures fatal.

Native errors capture stack locations at their origin, but defer symbol resolution until full details are requested. Error copies share immutable diagnostic payloads; short and full renderings are cached independently and support concurrent readers. Captured addresses and module-relative locations remain in full details when symbols or a loaded module are no longer available. Related cleanup failures retain both diagnostics without changing the primary error's classification.

Native callback errors keep their string-valued Lua representation, and built-in `error`/`pcall` behavior is unchanged. Summary transport is identity-bound and bounded; errors deliberately rewritten by a script are treated as new errors unless a complete known payload with a verified live Lua location prefix is preserved. This avoids guessing a mod or reusing a previous failure's attribution.
