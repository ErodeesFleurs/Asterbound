require "/interface/asterbound/ui/geometry.lua"

local canvas, config, style, fontSize, maximum, overflowText
local screen, scale

local float, viewportBounds = UiGeometry.float32, UiGeometry.bounds

local function layout()
  local size, nextScale = camera.screenSize(), interface.scale()
  if screen and screen[1] == size[1] and screen[2] == size[2] and scale == nextScale then return end
  screen, scale = size, nextScale
  local bounds = viewportBounds(size, scale)
  pane.setSize(bounds)
  widget.setSize("notifications", bounds)
  style.fontSize = math.floor(float(fontSize * scale))
end

function init()
  canvas = widget.bindCanvas("notifications")
  config = root.assetJson("/interface.config:message")
  config.showTime, config.hideTime = float(config.showTime), float(config.hideTime)
  config.windowSpring = float(config.windowSpring)
  maximum = root.assetJson("/interface.config:maxMessageCount")
  overflowText = root.assetJson("/interface.config:overflowMessageText")
  style = root.assetJson("/interface.config:textStyle") or {}
  if type(style) == "string" then style = {font = style} end
  fontSize = style.fontSize or 8
  storage.messages = storage.messages or jarray()
  storage.overflowCount = storage.overflowCount or 0
  screen = nil
  layout()
end

function push(message)
  storage.messages[#storage.messages + 1] = {
    text = message.text,
    cooldown = float(message.cooldown or config.showTime),
    spring = float(message.spring or 0)
  }
end

function pickup(item)
  for _, message in ipairs(storage.messages) do
    if message.item and root.itemDescriptorsMatch(message.item, item.descriptor, true) then
      message.count = message.count + item.count
      message.text = item.name .. " - " .. tostring(message.count)
      message.cooldown = config.showTime
      return
    end
  end
  storage.messages[#storage.messages + 1] = {
    item = item.descriptor, count = item.count,
    text = item.name .. " - " .. tostring(item.count),
    cooldown = config.showTime, spring = 0
  }
end

function update(dt)
  layout()
  local messages, overflow = storage.messages
  for _, message in ipairs(messages) do
    if message.overflow then overflow = message; break end
  end
  if not overflow then storage.overflowCount = 0 end
  local limit = maximum + (storage.overflowCount == 0 and 0 or 1)
  if #messages > limit then
    if not overflow then
      overflow = {overflow = true, spring = storage.overflowSpring or 0}
      table.insert(messages, 1, overflow)
    end
    storage.overflowCount = storage.overflowCount + 1
    overflow.text = overflowText:gsub("<count>", tostring(storage.overflowCount))
    overflow.cooldown = config.showTime
    for _, message in ipairs(messages) do
      overflow.cooldown = math.min(overflow.cooldown, message.cooldown or config.showTime)
    end
    for _, message in ipairs(messages) do
      if not message.overflow then message.cooldown = 0; break end
    end
  end
  dt = float(dt)
  for index = #messages, 1, -1 do
    local message = messages[index]
    message.cooldown = float(message.cooldown - dt)
    if message.cooldown < 0 then table.remove(messages, index) end
  end
end

function render()
  layout()
  canvas:clear()
  local totalX, totalY = 0, 0
  local bottomOffset = root.getConfigurationPath("inventory.bottomActionBar") and 32 or 0
  for _, message in ipairs(storage.messages) do
    local activeX, activeY = config.offset[1], config.offset[2] + bottomOffset
    bottomOffset = 0
    local dx = float(float(activeX - config.offsetHidden[1]) * message.spring)
    local dy = float(float(activeY - config.offsetHidden[2]) * message.spring)
    totalX, totalY = float(totalX + dx), float(totalY + dy)
    local x = float(float(screen[1] * 0.5) + float(float(totalX + config.offsetHidden[1]) * scale))
    local y = float(float(totalY + config.offsetHidden[2]) * scale)
    local background = {
      float(x + float(config.textContainerOffset[1] * scale)),
      float(y + float(config.textContainerOffset[2] * scale))
    }
    local text = {
      float(background[1] + float(config.textOffset[1] * scale)),
      float(background[2] + float(config.textOffset[2] * scale))
    }
    local spring = float(message.spring * config.windowSpring)
    if message.cooldown > config.hideTime then spring = float(spring + 1) end
    message.spring = float(spring / float(config.windowSpring + 1))
    if message.overflow then storage.overflowSpring = message.spring end
    canvas:drawImage(config.textContainer, background, scale, nil, true)
    canvas:drawText(message.text, {position = text, horizontalAnchor = "mid", verticalAnchor = "mid"}, style)
  end
end
