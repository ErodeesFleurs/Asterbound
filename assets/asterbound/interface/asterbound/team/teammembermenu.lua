local style
local canBeam

local function selectedMember()
  for _, member in ipairs(team.members()) do
    if member.uuid == storage.memberUuid then return member end
  end
end

local function refresh(member)
  local leader = team.isTeamLeader()
  widget.setButtonEnabled("beamToShip", canBeam)
  widget.setButtonEnabled("makeLeader", leader and not member.self)
  widget.setButtonEnabled("removeFromTeam", leader or member.self)
  widget.setText("removeFromTeam", member.self and style.removeSelfText or style.removeOtherText)
end

function init()
  style = root.assetJson("/interface/windowconfig/teammembermenu.config")
  canBeam = false
  if storage.memberUuid then
    update(0)
  else
    pane.hide()
  end
end

function onEvent(event, payload)
  if event ~= "open" or storage.memberUuid then return end
  storage.memberUuid = payload.uuid
  -- Native PaneManager centered the first unanchored display after open() had
  -- assigned its coordinates. Later displays retain the raw requested position.
  if storage.hasOpened or pane.anchor() ~= "none" then pane.setPosition(payload.position) end
  storage.hasOpened = true
  local member = selectedMember()
  if member then widget.setText("name", member.name) end
  -- The original open() starts disabled and only the subsequent update grants
  -- travel. Native interface.travel checks the live conditions again on click.
  canBeam = false
  if member then refresh(member) end
  pane.show()
end

function update(dt)
  if not storage.memberUuid then return end
  local member = selectedMember()
  if not member then
    pane.dismiss()
    return
  end
  canBeam = member.canTravel and interface.travelState().teamShip
  refresh(member)
end

function beamToShip()
  if canBeam then interface.travel("teamShip", storage.memberUuid) end
  pane.dismiss()
end

function makeLeader()
  team.makeLeader(storage.memberUuid)
  pane.dismiss()
end

function removeFromTeam()
  team.removeFromTeam(storage.memberUuid)
  pane.dismiss()
end

function close()
  pane.dismiss()
end

function onInput(event)
  if storage.memberUuid and event.type == "buttonDown" and (event.button == 0 or event.button == 2) then
    local p, size, mouse = pane.getScreenPosition(), pane.getSize(), event.screenPosition
    if mouse[1] < p[1] or mouse[2] < p[2] or mouse[1] > p[1] + size[1] or mouse[2] > p[2] + size[2] then
      pane.dismiss()
    end
  end
  return false
end

function dismissed()
  storage.memberUuid = nil
end
