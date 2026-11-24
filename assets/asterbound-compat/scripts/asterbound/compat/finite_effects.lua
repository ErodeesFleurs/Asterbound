-- A shorter add does not replace a previously infinite duration/maxDuration.
-- Repair only the requested effect, on its first renewal in this context.
local checked = {}

function asterboundRemoveNonfiniteEffect(name)
  for _, entry in ipairs(status.activeUniqueStatusEffectSummary()) do
    local fraction = entry[2]
    if entry[1] == name and fraction and
        (fraction ~= fraction or fraction == math.huge or fraction == -math.huge) then
      status.removeEphemeralEffect(name)
      return true
    end
  end
  return false
end

function asterboundRenewEffect(name, sourceEntityId, duration)
  if not checked[name] then
    asterboundRemoveNonfiniteEffect(name)
    checked[name] = true
  end
  status.addEphemeralEffect(name, duration, sourceEntityId)
end
