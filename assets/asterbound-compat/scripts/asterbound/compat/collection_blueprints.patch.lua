-- These recipes were always available in the source-pinned legacy inline
-- stations. Unlock only their genuine outputs when moving into the modern,
-- blueprint-filtered database UI; do not unlock unrelated station recipes.
function patch(data)
  local tier = data.defaultBlueprints.tier1
  local known = {}
  for _, blueprint in ipairs(tier) do
    if type(blueprint) == "table" and blueprint.item
        and (not blueprint.parameters or next(blueprint.parameters) == nil) then
      known[blueprint.item] = true
    end
  end
  for _, item in ipairs(assets.json("/scripts/asterbound/compat/collection_blueprints.config")) do
    if not known[item] then
      tier[#tier + 1] = {item = item}
      known[item] = true
    end
  end
  return data
end
