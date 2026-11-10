# Error Handling

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

Saved-data recovery scripts are declared under `errorHandlers` in a mod's `_metadata`. Supported keys are `item`, `object`, `npc`, `player`, `monster` and `vehicle`; each value is an array of script paths.

For example, this metadata fragment registers an item recovery script:

```json
"errorHandlers": {
  "item": ["/scripts/example/itemRecovery.lua"]
}
```

These scripts have access to `root` and `sb`. They define `error(diskStore, e)`, where `diskStore` is the stored data to repair and `e` is the exception string.

Handlers are collected in asset-source order, then in each declared script array's order. A handler must return changed stored data to trigger another load attempt. Returning `nil` or unchanged data stops recovery; a handler failure propagates. If the changed data still fails to load, the next handler receives that data and the new exception.

If none of the scripts handled the load errors fully, then the error will be thrown as normal.
