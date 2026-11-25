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

-- Used by the guarded effect's init. A replacement has a finite duration,
-- so its own init takes the normal path instead of replacing itself again.
function asterboundResumeOwnEffect()
  local duration = effect.duration()
  if duration and (duration ~= duration or duration == math.huge or duration == -math.huge) then
    local name = effect.name()
    status.removeEphemeralEffect(name)
    status.addEphemeralEffect(name)
    return true
  end
  return false
end

-- Keep the live instance on its authored default without reviving an effect
-- that just called expire() or turning a persistent instance into a lease.
function asterboundRenewOwnEffect(renewBelow)
  local duration = effect.duration()
  if duration and duration > 0 and duration <= renewBelow then
    status.addEphemeralEffect(effect.name())
  end
end
