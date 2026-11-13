-- The installer prefixes this script with the published translations keyed by
-- their English lines in the source-pinned legacy tenant. Only matching live
-- lines change, so removed dialog and unrelated mods' replacements stay intact.
function patch(data)
  local config = data.scriptConfig
  local dialog = config and config.dialog
  local converse = dialog and dialog.converse
  local default = converse and converse.default
  local lines = default and default.default
  if type(lines) == "table" then
    for index, original in ipairs(lines) do
      if translatedByOriginal[original] then
        lines[index] = translatedByOriginal[original]
      end
    end
  end
  return data
end
