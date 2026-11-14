require "/interface/asterbound/ui/geometry.lua"

local style, previous, visible, layoutExpanded
local frameHeight, progressPresent, compassPresent, progressValue, compassRotation
local widgets = {"imgFrame", "imgFrameExpanded", "lblQuestObjectiveList", "imgProgressFrame",
    "questProgress", "imgCompassFrame", "imgCompass"}

local float = UiGeometry.float32

local function objectivesEqual(a, b)
  if a == nil or b == nil then return a == b end
  if #a ~= #b then return false end
  for index, objective in ipairs(a) do
    if objective[1] ~= b[index][1] or objective[2] ~= b[index][2] then return false end
  end
  return true
end

local function formatObjective(objective)
  return sb.replaceTags(objective[2] and style.completeObjectiveTemplate or style.incompleteObjectiveTemplate,
      {objective = objective[1]})
end

local function layout(view)
  local objectives = view.objectives
  if not previous or layoutExpanded ~= storage.expanded
      or not objectivesEqual(objectives, previous.objectives) then
    if objectives == nil then
      widget.setVisible("lblQuestObjectiveList", false)
      widget.setText("lblQuestObjectiveList", "")
    elseif #objectives == 0 then
      -- Native empty lists hide the label but retain its measured height;
      -- an absent list clears it. Expanded backgrounds use that native height.
      widget.setVisible("lblQuestObjectiveList", false)
    else
      widget.setVisible("lblQuestObjectiveList", true)
      local text
      if storage.expanded then
        local lines = {}
        for index, objective in ipairs(objectives) do lines[index] = formatObjective(objective) end
        text = table.concat(lines)
      else
        local selected = objectives[1]
        for _, objective in ipairs(objectives) do
          if not objective[2] then selected = objective; break end
        end
        text = formatObjective(selected)
      end
      widget.setText("lblQuestObjectiveList", text)
    end
  end

  local height = storage.expanded and math.max(style.expandedFrameMinHeight,
      widget.getSize("lblQuestObjectiveList")[2] + style.expandedFramePadding * 2)
      or widget.getSize("imgFrame")[2]
  local hasProgress = view.progress ~= nil
  local hasCompass = view.compassDirection ~= nil
  if not previous or layoutExpanded ~= storage.expanded or frameHeight ~= height
      or progressPresent ~= hasProgress or compassPresent ~= hasCompass then
    widget.setVisible("imgFrame", not storage.expanded)
    widget.setVisible("imgFrameExpanded", storage.expanded)
    if storage.expanded then
      local size = widget.getSize("imgFrameExpanded")
      widget.setSize("imgFrameExpanded", {size[1], height})
      local position = widget.getPosition("imgFrameExpanded")
      widget.setPosition("imgFrameExpanded", {position[1], -height})
    end
    widget.setVisible("questProgress", hasProgress)
    widget.setVisible("imgProgressFrame", hasProgress)
    if hasProgress then
      widget.setImage("imgProgressFrame", storage.expanded and style.expandedProgressFrameImage or style.progressFrameImage)
      widget.setDrawingOffset("questProgress", {0, -height})
      widget.setDrawingOffset("imgProgressFrame", {0, -height})
    end
    widget.setVisible("imgCompass", hasCompass)
    widget.setVisible("imgCompassFrame", hasCompass)
    if hasCompass then
      widget.setImage("imgCompassFrame", (storage.expanded or hasProgress)
          and style.expandedCompassFrameImage or style.compassFrameImage)
    end
    frameHeight, progressPresent, compassPresent = height, hasProgress, hasCompass
    layoutExpanded = storage.expanded
  end
  if hasProgress and view.progress ~= progressValue then
    widget.setProgress("questProgress", view.progress)
    progressValue = view.progress
  end
end

local function compass(target)
  if target == nil then return end
  -- The native spring advances once per update, not by dt. Preserve float32
  -- intermediates, including the double-pi shortest-angle correction.
  local direction, speed = storage.compassDirection, storage.compassSpeed
  local tau = float(math.pi) * 2
  local diff = float(target - direction)
  diff = float(diff - float(tau * math.floor(float(diff / tau))))
  if math.abs(diff) < style.compassAcceleration and math.abs(speed) < style.compassAcceleration then
    speed, direction = 0, target
  else
    if math.abs(diff) > math.pi then diff = float(diff - (diff < 0 and -2 or 2) * math.pi) end
    local ratio = float(math.abs(diff) / math.pi)
    local change = float(style.compassAcceleration * ratio)
    speed = float(speed + (diff < 0 and -change or change))
    speed = float(speed - float(speed * style.compassFriction))
    direction = float(direction + speed)
  end
  storage.compassDirection, storage.compassSpeed = direction, speed
  if compassRotation ~= direction then
    widget.setImageRotation("imgCompass", direction)
    compassRotation = direction
  end
end

local function refresh(advance)
  local view = interface.session().hasPlayer and player.currentQuestView() or nil
  if not view then
    if visible ~= false then
      for _, name in ipairs(widgets) do widget.setVisible(name, false) end
      pane.hide()
      visible, previous = false, nil
      progressValue = nil
    end
    return
  end
  if not visible then pane.show(); visible = true end
  layout(view)
  if advance then
    compass(view.compassDirection)
  elseif view.compassDirection ~= nil then
    -- Reinitialization restores the image without taking an extra spring step.
    widget.setImageRotation("imgCompass", storage.compassDirection)
    compassRotation = storage.compassDirection
  end
  previous = view
end

function init()
  style = root.assetJson("/interface/questtracker/questtracker.config")
  style.compassAcceleration = float(style.compassAcceleration)
  style.compassFriction = float(style.compassFriction)
  storage.expanded = storage.expanded or false
  storage.compassDirection = storage.compassDirection or 0
  storage.compassSpeed = storage.compassSpeed or 0
  previous, visible, layoutExpanded = nil, nil, nil
  frameHeight, progressPresent, compassPresent, progressValue = nil, nil, nil, nil
  compassRotation = nil
  pane.setScissoringEnabled(false)
  widget.setScissoringEnabled("imgProgressFrame", false)
  refresh(false)
end

function update()
  refresh(true)
end

function onInput(event)
  if visible and event.type == "buttonDown" and event.button == 0
      and widget.inMember(storage.expanded and "imgFrameExpanded" or "imgFrame", event.screenPosition) then
    storage.expanded = not storage.expanded
    layout(previous)
    return true
  end
  return false
end
