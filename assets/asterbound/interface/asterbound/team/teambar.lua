require "/interface/asterbound/ui/api.lua"
require "/interface/asterbound/ui/geometry.lua"

local style
local cache
local rows

local function add(a, b)
  return {a[1] + b[1], a[2] + b[2]}
end

local float = UiGeometry.float32

local function changed(key, value, apply)
  if cache[key] ~= value then
    cache[key] = value
    apply(value)
  end
end

local function resource(name)
  return assert(status.resourcePercentage(name), "Missing required player resource '" .. name .. "'")
end

function init()
  style = root.assetJson("/interface/windowconfig/teambar.config")
  style.foodBarFlashTime = float(style.foodBarFlashTime)
  cache = {}
  rows = {}
  pane.setScissoringEnabled(false)
  update(0)
end

function update(dt)
  local session = interface.session()
  if not session.hasPlayer then
    pane.hide()
    return
  end
  pane.show()
  -- The real PortraitWidget holds the live actor, including its animation and
  -- equipment. No player diskStore/portrait reconstruction enters the HUD loop.
  if cache.playerGeneration ~= session.playerGeneration or cache.worldGeneration ~= session.worldGeneration then
    widget.setPortraitEntity("portrait", interface.portraitEntity())
    cache.playerGeneration, cache.worldGeneration = session.playerGeneration, session.worldGeneration
  end
  changed("health", resource("health"), function(v) widget.setProgress("healthBar", v) end)
  changed("energy", resource("energy"), function(v) widget.setProgress("energyBar", v) end)
  changed("name", player.name(), function(v) widget.setText("name", v) end)
  local hunger = player.hungerEnabled()
  changed("hunger", hunger, function(v) widget.setVisible("foodBar", v) end)
  if hunger then
    local food = resource("food")
    changed("food", food, function(v) widget.setProgress("foodBar", v) end)
    if not cache.foodLowThreshold then cache.foodLowThreshold = float(root.assetJson("/player.config:foodLowThreshold")) end
    local overlay = ""
    if food <= cache.foodLowThreshold and sb.monotonicTime() % (style.foodBarFlashTime * 2) < style.foodBarFlashTime then
      overlay = style.foodBarFlashOverlay
    end
    changed("foodOverlay", overlay, function(v) widget.setProgressOverlay("foodBar", v) end)
  end
  local locked = status.resourceLocked("energy")
  local regen = locked and 0 or resource("energyRegenBlock")
  if cache.locked ~= locked or cache.regen ~= regen then
    cache.locked, cache.regen = locked, regen
    if locked then
      widget.setProgressColor("energyBar", style.energyBarUnusableColor)
    else
      widget.setProgressColor("energyBar", style.energyBarColor, style.energyBarRegenMixColor, regen)
    end
  end

  local invitationActive = interface.paneDisplayed("TeamInvitation")
  if not invitationActive and team.hasInvitationPending() then ui.open("TeamInvitation") end
  local leader = team.isTeamLeader()
  changed("leader", leader, function(v) widget.setVisible("leader", v) end)
  if team.currentTeam() and not leader and interface.paneDisplayed("TeamInvite") then ui.close("TeamInvite") end

  local members = team.members()
  local index, y = 0, 0
  for _, member in ipairs(members) do
    if not member.self then
      local path = "list." .. index
      local row = rows[index]
      if not widget.getPosition(path) then
        widget.addChild("list", {type = "container", children = style.entry}, tostring(index))
      end
      if not row then row = {}; rows[index] = row end
      y = y - style.memberSize
      widget.setPosition(path, {0, y})
      widget.setVisible(path, true)
      widget.setData(path, member.uuid)
      widget.setData(path .. ".showMemberMenu", {uuid = member.uuid, row = index})
      widget.setVisible(path .. ".leader", member.leader)
      if row.uuid ~= member.uuid or row.portraitRevision ~= member.portraitRevision then
        local portrait = assert(team.memberPortrait(member.uuid), "Missing team member portrait")
        widget.setImageDrawables(path .. ".portrait", portrait.portrait, {style.memberPortraitScale, style.memberPortraitScale})
        row.uuid, row.portraitRevision = member.uuid, portrait.revision
      end
      local onWorld = member.onWorld and session.inWorld
      widget.setVisible(path .. ".compass", onWorld)
      widget.setVisible(path .. ".compassoffworld", not onWorld)
      if onWorld then
        local position = world.entityPosition(member.entity) or member.position
        local direction = world.distance(position, player.position())
        widget.setImageRotation(path .. ".compass", float(math.atan(direction[2], direction[1])) - math.pi / 2)
      end
      widget.setProgress(path .. ".healthBar", member.healthPercentage)
      widget.setProgress(path .. ".energyBar", member.energyPercentage)
      y = y - style.memberSpacing
      index = index + 1
    end
  end
  local nextRow = index
  while widget.getPosition("list." .. nextRow) do
    widget.setVisible("list." .. nextRow, false)
    nextRow = nextRow + 1
  end
  local offset = add(widget.getPosition("list"), {0, y})
  for _, name in ipairs({"inviteButton", "noInviteImage"}) do
    widget.setPosition(name, {offset[1], offset[2] - widget.getSize(name)[2]})
  end
  local couldInvite = (not team.currentTeam() or leader) and #members < root.getConfiguration("maxTeamSize")
  widget.setVisible("inviteButton", couldInvite)
  widget.setVisible("noInviteImage", not couldInvite)
  widget.setButtonEnabled("inviteButton", not invitationActive)
end

function inviteButton()
  if not interface.paneDisplayed("TeamInvite") then ui.open("TeamInvite") end
end

function showSelfMenu()
  if team.isMemberOfTeam() and not interface.paneDisplayed("TeamMemberMenu") then
    local height = camera.screenSize()[2] / interface.scale()
    ui.send("TeamMemberMenu", "open", {uuid = player.uniqueId(), position = add(style.selfMenuOffset, {0, math.floor(height)})})
  end
end

function showMemberMenu(name, data)
  if interface.paneDisplayed("TeamMemberMenu") then return end
  local row = "list." .. data.row
  local position = add(pane.getScreenPosition(), widget.getPosition("list"))
  position = add(position, widget.getPosition(row))
  position = add(position, widget.getPosition(row .. ".showMemberMenu"))
  ui.send("TeamMemberMenu", "open", {uuid = data.uuid, position = add(position, style.memberMenuOffset)})
end
