-- Loaded only by the fingerprint-guarded RPG Growth AI 1.1 repair.
-- Discovery may lag one extra logic tick; movement, prediction and timers do not.
local discoveryInterval = 1 / 30
local queryOptions = {includedTypes = {"projectile"}}
local originalInit = rpg_initAI
local originalUpdate = rpg_updateAI

function rpg_initAI()
  originalInit()
  self.asterboundRpgAi = {elapsed = 0, ticks = 0, members = {}}
end

function rpg_updateAI(dt)
  local discovery = self.asterboundRpgAi
  discovery.elapsed = discovery.elapsed + dt
  discovery.ticks = discovery.ticks + 1
  originalUpdate(dt)
end

function asterboundRpgProjectileCandidates()
  local discovery = self.asterboundRpgAi
  local position = mcontroller.position()
  local displacement = discovery.position and world.distance(position, discovery.position)
  -- A two-tile move invalidates the neighborhood, including across world wrap.
  local moved = displacement and displacement[1] * displacement[1] + displacement[2] * displacement[2] >= 4
  local reset = moved or discovery.ownerId ~= self.rpg_Id
  if not discovery.ids or reset or discovery.ticks >= 2 or discovery.elapsed >= discoveryInterval - 1e-9 then
    discovery.ids = world.entityQuery(position, 30, queryOptions)
    discovery.position = position
    discovery.ownerId = self.rpg_Id
    discovery.elapsed = 0
    discovery.ticks = 0

    -- Reuse the membership table; do not retain histories for departed live IDs.
    local members = discovery.members
    for id in pairs(members) do members[id] = nil end
    for _, id in ipairs(discovery.ids) do members[id] = true end
    for id in pairs(self.rpg_enemyProjectiles) do
      if reset or not members[id] then self.rpg_enemyProjectiles[id] = nil end
    end
  end
  return discovery.ids
end
