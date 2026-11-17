-- Used only by fingerprinted, continuously renewed RPG Growth effects.
-- Their summary values are duration fractions, not seconds. An old infinite
-- duration/maxDuration produces a nonfinite fraction and must be removed before
-- adding the authored finite default; a shorter add alone cannot repair it.
local checked = {}

function asterboundRpgRenewEffect(name, sourceEntityId)
  if not checked[name] then
    for _, entry in ipairs(status.activeUniqueStatusEffectSummary()) do
      local fraction = entry[2]
      if entry[1] == name and fraction and
          (fraction ~= fraction or fraction == math.huge or fraction == -math.huge) then
        status.removeEphemeralEffect(name)
        break
      end
    end
    checked[name] = true
  end
  status.addEphemeralEffect(name, nil, sourceEntityId)
end
