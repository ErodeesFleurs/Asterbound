require "/interface/asterbound/ui/geometry.lua"

local canvas, config, sizes, entries, textStyle
local screen, scale, paintStyle, dirty, previousCount
local renderViews, previousSlots

local float, viewportBounds = UiGeometry.float32, UiGeometry.bounds

local function multiply(a, b)
  return {float(a[1] * b), float(a[2] * b)}
end

local function add(a, b)
  return {float(a[1] + b[1]), float(a[2] + b[2])}
end

specialHealthPresentation = {}

function specialHealthPresentation.interpolate(current, health, maximum, fillTime, dt)
  local target = float(health / maximum)
  local speed = float(1 / float(fillTime))
  local difference = float(target - current)
  local step = float(speed * float(dt))
  if math.abs(difference) < step then return target end
  -- std::copysign preserves a negative zero difference as well.
  local sign = difference < 0 or (difference == 0 and 1 / difference < 0)
  return float(current + (sign and -step or step))
end

function specialHealthPresentation.layout(width, interfaceScale, count, barConfig)
  local spacing = float(barConfig.multibarSpacing or float(barConfig.backgroundOffset[1] * -2))
  local hSpacing = float(spacing * interfaceScale)
  local totalWidth = float(hSpacing * count)
  local edge = float((barConfig.multibarScreenEdgeOffset or 20) * interfaceScale)
  local maximum = math.min(totalWidth, float(width - edge))
  local hScale = float(maximum / totalWidth)
  local backgroundOffset = multiply(barConfig.backgroundOffset, interfaceScale)
  backgroundOffset[1] = float(backgroundOffset[1] * hScale)
  local fillOffset = multiply(barConfig.fillOffset, interfaceScale)
  fillOffset[1] = float(fillOffset[1] * hScale)
  local nameOffset = multiply(barConfig.nameOffset, interfaceScale)
  nameOffset[1] = float(nameOffset[1] * hScale)
  return {
    spacing = hSpacing,
    hScale = hScale,
    allOffset = float(float(-hSpacing * 0.5) * (count - 1)),
    center = float(width / 2),
    backgroundOffset = backgroundOffset,
    fillOffset = fillOffset,
    nameOffset = nameOffset
  }
end

local function refreshLayout()
  local nextScreen, nextScale = camera.screenSize(), interface.scale()
  if screen and screen[1] == nextScreen[1] and screen[2] == nextScreen[2] and scale == nextScale then return end
  screen, scale, dirty = nextScreen, nextScale, true
  local bounds = viewportBounds(screen, scale)
  pane.setSize(bounds)
  widget.setSize("specialHealth", bounds)
  paintStyle = sb.jsonMerge(textStyle, {fontSize = (math.modf(float((textStyle.fontSize or 8) * scale)))})
end

function init()
  canvas = widget.bindCanvas("specialHealth")
  pane.setScissoringEnabled(false)
  widget.setScissoringEnabled("specialHealth", false)
  config = root.assetJson("/interface.config:specialDamageBar")
  for _, key in ipairs({"backgroundOffset", "fillOffset", "nameOffset"}) do
    for index, value in ipairs(config[key]) do config[key][index] = float(value) end
  end
  config.fillTime = float(config.fillTime)
  if config.multibarSpacing then config.multibarSpacing = float(config.multibarSpacing) end
  if config.multibarScreenEdgeOffset then config.multibarScreenEdgeOffset = float(config.multibarScreenEdgeOffset) end
  sizes = {background = root.imageSize(config.background), fill = root.imageSize(config.fill)}
  -- A present TextStyle replaces the earlier legacy nameColor/nameSize setters,
  -- including their defaults. It is not a partial merge with those setters.
  textStyle = config.nameStyle or {fontSize = config.nameSize, color = config.nameColor}
  if type(textStyle) == "string" then textStyle = {font = textStyle} end
  storage.specialHealth = storage.specialHealth or {}
  entries = storage.specialHealth
  screen = nil
  renderViews, previousSlots, previousCount = {}, {}, 0
  refreshLayout()
end

function update(dt)
  refreshLayout()
  if not interface.session().inWorld then
    for index = #entries, 1, -1 do entries[index] = nil end
    return
  end
  local live = {}
  local index = 1
  while index <= #entries do
    local entry = entries[index]
    local view = world.entityDamageBar(entry.id)
    if view and view.type == "Special" then
      entry.fill = specialHealthPresentation.interpolate(entry.fill, view.health, view.maxHealth, config.fillTime, dt)
      live[entry.id] = true
      index = index + 1
    else
      table.remove(entries, index)
    end
  end
  local maximum = config.maxCount or 10
  if #entries < maximum and player.inWorld() then
    local candidates = world.damageBarEntities(nil, "Special")
    local position = player.position()
    for index, candidate in ipairs(candidates) do
      candidate.difference = world.distance(candidate.position, position)
      candidate.order = index
    end
    -- Native sorted Vec2F differences lexicographically, NOT by distance.
    -- Preserve discovery order for equal keys; live entries keep stable slots.
    table.sort(candidates, function(a, b)
      local x, y = a.difference, b.difference
      if x[1] ~= y[1] then return x[1] < y[1] end
      if x[2] ~= y[2] then return x[2] < y[2] end
      return a.order < b.order
    end)
    for _, candidate in ipairs(candidates) do
      if not live[candidate.id] then
        entries[#entries + 1] = {id = candidate.id, fill = 0}
        live[candidate.id] = true
        if #entries >= maximum then break end
      end
    end
  end
end

function render()
  refreshLayout()
  if not interface.session().inWorld then
    canvas:clear()
    dirty = true
    return
  end
  local count = #entries
  dirty = dirty or count ~= previousCount
  for index, entry in ipairs(entries) do
    local view = world.entityDamageBar(entry.id)
    renderViews[index] = view or false
    local previous = previousSlots[index]
    if not previous or previous.id ~= entry.id or previous.fill ~= entry.fill
        or previous.name ~= (view and view.name) or previous.present ~= (not not view) then
      dirty = true
      previousSlots[index] = {id = entry.id, fill = entry.fill, name = view and view.name, present = not not view}
    end
  end
  for index = count + 1, previousCount do
    renderViews[index], previousSlots[index] = nil, nil
  end
  if not dirty then return end
  previousCount, dirty = count, false
  canvas:clear()
  if count == 0 then return end
  local layout = specialHealthPresentation.layout(screen[1], scale, count, config)
  local backgroundSize = multiply(sizes.background, scale)
  backgroundSize[1] = float(backgroundSize[1] * layout.hScale)
  for index, entry in ipairs(entries) do
    -- Count every entry, including one removed between update and render. A
    -- missing entity leaves its slot empty until the NEXT update removes it.
    -- Neither lighting nor offscreen position filters a special bar.
    local view = renderViews[index]
    if view then
      local x = float(layout.center + float(float(layout.allOffset + float(layout.spacing * (index - 1))) * layout.hScale))
      local bottom = {x, 0}
      local background = add(bottom, layout.backgroundOffset)
      canvas:drawImageRect(config.background, {0, 0, sizes.background[1], sizes.background[2]},
          {background[1], background[2], float(background[1] + backgroundSize[1]), float(background[2] + backgroundSize[2])})
      local fillSize = {float(float(config.fillWidth * entry.fill) * layout.hScale), sizes.fill[2]}
      fillSize = multiply(fillSize, scale)
      local fill = add(bottom, layout.fillOffset)
      canvas:drawImageRect(config.fill, {0, 0, sizes.fill[1], sizes.fill[2]},
          {fill[1], fill[2], float(fill[1] + fillSize[1]), float(fill[2] + fillSize[2])})
      canvas:drawText(view.name, {position = add(bottom, layout.nameOffset), horizontalAnchor = "mid", verticalAnchor = "bottom"}, paintStyle)
    end
  end
end
