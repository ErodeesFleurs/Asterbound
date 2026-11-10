local canvas, offset, shift, darken
local metadata, indicators, dirty

local function effectMetadata(name)
  local entry = metadata[name]
  if not entry then
    local config = root.effectConfig(name)
    entry = {icon = config.icon, label = config.label or ""}
    if entry.icon then entry.size = root.imageSize(entry.icon) end
    metadata[name] = entry
  end
  return entry
end

local function layout()
  if #indicators == 0 then
    pane.setAnchorOffset({0, 0})
    pane.setSize({0, 0})
    widget.setSize("icons", {0, 0})
    canvas:clear()
    return
  end

  -- Positions are relative to the top edge. Native TopLeft anchoring uses
  -- ceil(windowPixels / interfaceScale), matching the original icon placement.
  local minX, minY, maxX, maxY
  local x, y = offset[1], -offset[2]
  for _, indicator in ipairs(indicators) do
    indicator.position = {x, y}
    minX = math.min(minX or x, x)
    minY = math.min(minY or y, y)
    maxX = math.max(maxX or (x + indicator.size[1]), x + indicator.size[1])
    maxY = math.max(maxY or (y + indicator.size[2]), y + indicator.size[2])
    x, y = x + shift[1], y + shift[2]
  end
  for _, indicator in ipairs(indicators) do
    indicator.position[1] = indicator.position[1] - minX
    indicator.position[2] = indicator.position[2] - minY
  end
  local size = {maxX - minX, maxY - minY}
  pane.setAnchorOffset({minX, maxY})
  pane.setSize(size)
  widget.setSize("icons", size)
end

local function refresh()
  local count, changedLayout = 0, false
  for _, summary in ipairs(status.activeUniqueStatusEffectSummary()) do
    local name, duration = summary[1], summary[2]
    local effect = effectMetadata(name)
    if effect.icon then
      count = count + 1
      local indicator = indicators[count]
      if not indicator or indicator.name ~= name then
        indicator = {name = name, icon = effect.icon, label = effect.label, size = effect.size}
        indicators[count] = indicator
        changedLayout = true
      end
      -- Preserve native float32 multiplication before truncation, including
      -- non-power-of-two icon heights. nil has no mask; zero still masks row 0.
      local row
      if duration ~= nil then
        local rows = string.unpack("f", string.pack("f", duration * effect.size[2]))
        row = math.modf(rows)
      end
      if row ~= indicator.row or not indicator.image then
        indicator.row = row
        indicator.image = effect.icon
        if row ~= nil then
          indicator.image = indicator.image .. "?blendmult=" .. darken .. ";0;" .. string.format("%d", -row)
        end
        dirty = true
      end
    end
  end
  while #indicators > count do
    indicators[#indicators] = nil
    changedLayout = true
  end
  if changedLayout then layout(); dirty = true end
end

function init()
  canvas = widget.bindCanvas("icons")
  offset = root.assetJson("/interface.config:statusIconPos")
  shift = root.assetJson("/interface.config:statusIconShift")
  darken = root.assetJson("/interface.config:statusIconDarkenImage")
  metadata, indicators, dirty = {}, {}, true
  layout()
  refresh()
end

function update(dt)
  refresh()
end

function render()
  if not dirty then return end
  canvas:clear()
  for _, indicator in ipairs(indicators) do
    canvas:drawImage(indicator.image, indicator.position)
  end
  dirty = false
end

function createTooltip(screenPosition)
  local origin = pane.getScreenPosition()
  local x, y = screenPosition[1] - origin[1], screenPosition[2] - origin[2]
  for _, indicator in ipairs(indicators) do
    local position, size = indicator.position, indicator.size
    if x >= position[1] and y >= position[2]
        and x <= position[1] + size[1] and y <= position[2] + size[2] then
      if indicator.label ~= "" then return indicator.label end
    end
  end
end
