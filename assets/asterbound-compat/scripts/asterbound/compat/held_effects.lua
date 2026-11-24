require "/scripts/asterbound/compat/finite_effects.lua"

-- For guarded scripts that add an ephemeral effect on entry to a state and
-- remove it on exit. Keep immunity, dispels and effect.expire() working; a
-- persistent category would bypass those semantics. Each producer owns a set.
function asterboundHeldEffects(savedNames)
  local active = savedNames or {}
  local remaining = 0
  -- Monster AI can stop updating for a one-second knockout before death hooks.
  local interval, lifetime = 0.25, 2
  return {
    start = function(name)
      asterboundRenewEffect(name, nil, lifetime)
      active[name] = status.uniqueStatusEffectActive(name) or nil
    end,
    resumeLegacy = function(name)
      if asterboundRemoveNonfiniteEffect(name) then
        status.addEphemeralEffect(name, lifetime)
        active[name] = status.uniqueStatusEffectActive(name) or nil
      end
    end,
    stop = function(name)
      active[name] = nil
      status.removeEphemeralEffect(name)
    end,
    update = function(dt)
      if not next(active) then return end
      remaining = remaining - dt
      if remaining > 0 then return end
      remaining = interval
      for name in pairs(active) do active[name] = false end
      for _, entry in ipairs(status.activeUniqueStatusEffectSummary()) do
        local name, fraction = entry[1], entry[2]
        if active[name] ~= nil and fraction and fraction > 0 then
          active[name] = true
          status.addEphemeralEffect(name, lifetime)
        end
      end
      for name, seen in pairs(active) do
        if not seen then active[name] = nil end
      end
    end
  }
end
