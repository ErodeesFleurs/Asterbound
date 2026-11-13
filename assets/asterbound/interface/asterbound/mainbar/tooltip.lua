require "/interface/asterbound/ui/geometry.lua"

local canvas, config, imageSize, style, screen, scale

local float, viewportBounds = UiGeometry.float32, UiGeometry.bounds

local function layout()
  local size, nextScale = camera.screenSize(), interface.scale()
  if screen and screen[1] == size[1] and screen[2] == size[2] and scale == nextScale then return end
  screen, scale = size, nextScale
  local bounds = viewportBounds(size, scale)
  pane.setSize(bounds)
  widget.setSize("tooltip", bounds)
  style.fontSize = math.floor(float(config.fontSize * scale))
end

function init()
  canvas = widget.bindCanvas("tooltip")
  config = root.assetJson("/interface.config:cursorTooltip")
  imageSize = root.imageSize(config.background)
  style = config.textStyle or {}
  if type(style) == "string" then style = {font = style} end
  style.color = config.color
  screen = nil
  layout()
end

function setTooltip(data)
  storage.tooltip = data
end

function render()
  layout()
  canvas:clear()
  local data = storage.tooltip
  if not data or interface.cinematicActive() or not interface.paneVisible("MainBar") then return end
  local cursor = data.cursor
  -- The old integer-vector operators truncated each scalar before multiplying.
  local integerScale = math.modf(scale)
  local cursorScale = math.modf(cursor.scale)
  local width, height = imageSize[1] * integerScale, imageSize[2] * integerScale
  local x = cursor.roundedPosition[1] + config.offset[1] * cursorScale
  local y = cursor.roundedPosition[2] + (config.offset[2] - cursor.size[2]) * cursorScale
  canvas:drawImage(config.background, {x - width, y}, scale)
  canvas:drawText(data.text, {
    position = {float(x - float(width * 0.5)), float(y + float(height * 0.5))},
    horizontalAnchor = "mid", verticalAnchor = "mid"
  }, style)
end
