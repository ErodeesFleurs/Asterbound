require "/interface/asterbound/ui/geometry.lua"

local canvas, config, sizes, textStyle, state
local screen, scale, paintStyle, dirty
local portraitRevision, portrait, portraitBounds
local previousId, previousName, previousBlocks, previousRevision

local float, viewportBounds = UiGeometry.float32, UiGeometry.bounds

local function trunc(value)
  return (math.modf(value))
end

local function add(a, b)
  return {float(a[1] + b[1]), float(a[2] + b[2])}
end

local function multiply(a, b)
  return {float(a[1] * b), float(a[2] * b)}
end

local function hostile(view)
  return view and view.type == "Default" and (view.team.type == "enemy" or view.team.type == "pvp")
end

targetHealthPresentation = {}

function targetHealthPresentation.blocks(health, maximum, chunks)
  local value = float(float(health / maximum) * chunks)
  return value < 0 and math.ceil(value - 0.5) or math.floor(value + 0.5)
end

function targetHealthPresentation.fit(bounds, maximum)
  local width, height = float(bounds[3] - bounds[1]), float(bounds[4] - bounds[2])
  return math.max(1, math.ceil(math.max(float(width / maximum[1]), float(height / maximum[2]))))
end

function targetHealthPresentation.scissor(rect, position, interfaceScale)
  local inverse = float(1 / interfaceScale)
  local result = {}
  for index = 1, 4 do
    local physical = float(float(rect[index] * interfaceScale) + position[index % 2 == 1 and 1 or 2])
    -- Legacy: RectI(physicalRect / scale), followed by the GUI's second
    -- RectI(interfaceRect * scale). Both conversions truncate toward zero.
    result[index] = trunc(float(trunc(float(physical * inverse)) * interfaceScale))
  end
  return result
end

local function refreshLayout()
  local nextScreen, nextScale = camera.screenSize(), interface.scale()
  if screen and screen[1] == nextScreen[1] and screen[2] == nextScreen[2] and scale == nextScale then return end
  screen, scale, dirty = nextScreen, nextScale, true
  local bounds = viewportBounds(screen, scale)
  pane.setSize(bounds)
  widget.setSize("targetHealth", bounds)
  paintStyle = sb.jsonMerge(textStyle, {fontSize = trunc(float((textStyle.fontSize or 8) * scale))})
end

function init()
  canvas = widget.bindCanvas("targetHealth")
  pane.setScissoringEnabled(false)
  widget.setScissoringEnabled("targetHealth", false)
  config = root.assetJson("/interface.config:monsterHealth")
  for _, key in ipairs({"offset", "nameTextOffset", "progressBarOffset", "portraitOffset", "portraitScissorRect"}) do
    for index, value in ipairs(config[key]) do config[key][index] = float(value) end
  end
  config.showTime, config.portraitScale = float(config.showTime), float(config.portraitScale)
  textStyle = root.assetJson("/interface.config:textStyle")
  if type(textStyle) == "string" then textStyle = {font = textStyle} end
  textStyle = textStyle or {}
  sizes = {
    container = root.imageSize(config.container),
    empty = root.imageSize(config.progressEmpty),
    filled = root.imageSize(config.progressFilled)
  }
  storage.targetHealth = storage.targetHealth or {remaining = float(config.showTime), portraitScale = 0}
  state = storage.targetHealth
  screen, portraitRevision, portrait, portraitBounds = nil, nil, nil, nil
  previousId, previousName, previousBlocks, previousRevision = nil, nil, nil, nil
  refreshLayout()
end

function update(dt)
  refreshLayout()
  dt = float(dt)
  if state.remaining > 0 then
    state.remaining = math.max(float(state.remaining - dt), 0)
  elseif state.remaining < 0 then
    state.remaining = math.min(float(state.remaining + dt), 0)
  end
  if not interface.session().inWorld then
    if state.remaining == 0 then state.id = nil end
    return
  end
  local cursor = interface.cursorWorldPosition()
  local candidates = world.damageBarEntities({float(cursor[1] - 0.5), float(cursor[2] - 0.5),
      float(cursor[1] + 0.5), float(cursor[2] + 0.5)}, "Default")
  local nextId, nearest
  local playerId = world.mainPlayer()
  for _, candidate in ipairs(candidates) do
    if candidate.id ~= playerId and hostile(candidate) and world.lightLevel(candidate.position) > 0 then
      local difference = world.distance(candidate.position, cursor)
      local distance = float(math.sqrt(float(float(difference[1] * difference[1]) + float(difference[2] * difference[2]))))
      if not nearest or distance < nearest then nextId, nearest = candidate.id, distance end
    end
  end
  -- This fallback intentionally does not repeat the light query and cannot
  -- replace an already sticky target. Hovering the same ID does not reset time.
  if not nextId and not state.id then
    local damaged = player.lastDamagedTarget()
    if damaged and player.timeSinceLastGaveDamage() < float(float(config.showTime) / 2) then
      if hostile(world.entityDamageBar(damaged)) then nextId = damaged end
    end
  end
  if nextId and nextId ~= state.id then
    state.id, state.portraitScale, state.remaining = nextId, 0, float(config.showTime)
    portraitRevision, portrait, portraitBounds = nil, nil, nil
    dirty = true
  end
  if state.remaining == 0 then state.id = nil end
end

function render()
  refreshLayout()
  local view = interface.session().inWorld and state.id and state.remaining ~= 0 and world.entityDamageBar(state.id)
  if not view then
    state.id = nil
    if previousId or dirty then canvas:clear() end
    previousId, dirty = nil, false
    return
  end
  local nextPortrait = world.entityPortraitView(state.id, "Full", portraitRevision)
  if nextPortrait then
    portraitRevision = nextPortrait.revision
    if nextPortrait.drawables then
      portrait, portraitBounds = nextPortrait.drawables, nextPortrait.bounds
    end
  end
  local blocks = targetHealthPresentation.blocks(view.health, view.maxHealth, config.progressChunks)
  if not dirty and previousId == state.id and previousName == view.name and previousBlocks == blocks
      and previousRevision == portraitRevision then return end
  canvas:clear()
  local center = {float(screen[1] / 2), screen[2]}
  local background = add(center, multiply(config.offset, scale))
  -- The native container size multiplies Vec2U before converting to Vec2F:
  -- its scalar truncates first, unlike all the already-float offsets.
  local containerSize = multiply(sizes.container, trunc(scale))
  local bottom = {float(background[1] - float(containerSize[1] * 0.5)), float(background[2] - float(containerSize[2] * 0.5))}
  canvas:drawImageRect(config.container, {0, 0, sizes.container[1], sizes.container[2]},
      {bottom[1], bottom[2], float(bottom[1] + containerSize[1]), float(bottom[2] + containerSize[2])})
  canvas:drawText(view.name, {position = add(center, multiply(config.nameTextOffset, scale)),
      horizontalAnchor = "left", verticalAnchor = "bottom"}, paintStyle)
  local bar = add(center, multiply(config.progressBarOffset, scale))
  -- The original empty background adds the screen center a second time. Keep
  -- this authored/native geometry rather than silently fixing its visibility.
  local empty = add(center, bar)
  local emptySize = multiply(sizes.empty, scale)
  canvas:drawImageRect(config.progressEmpty, {0, 0, sizes.empty[1], sizes.empty[2]},
      {empty[1], empty[2], float(empty[1] + emptySize[1]), float(empty[2] + emptySize[2])})
  local step = float(sizes.filled[1] * scale)
  for index = 0, blocks - 1 do
    canvas:drawImage(config.progressFilled, {float(bar[1] + float(step * index)), bar[2]}, scale)
  end
  if portrait then
    local position = add(center, multiply(config.portraitOffset, scale))
    if state.portraitScale == 0 then
      state.portraitScale = targetHealthPresentation.fit(portraitBounds, config.portraitMaxSize)
    end
    local width = float(portraitBounds[3] - portraitBounds[1])
    local translation = {float(-portraitBounds[1] - float(width * 0.5)), -portraitBounds[2]}
    canvas:pushClipRect(targetHealthPresentation.scissor(config.portraitScissorRect, position, scale))
    canvas:drawDrawables(portrait, position, float(1 / state.portraitScale), translation, float(config.portraitScale * scale))
    canvas:popClipRect()
  end
  previousId, previousName, previousBlocks, previousRevision = state.id, view.name, blocks, portraitRevision
  dirty = false
end
