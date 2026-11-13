require "/interface/asterbound/ui/geometry.lua"

local canvas, offset, barOffset, emptyWidth
local screen, scale, geometry, dirty, previousBlocks, previousWarning

-- Keep the native float32 operation boundaries, rather than quantizing the
-- resource percentage in Lua double precision at a different half-block edge.
local float, viewportBounds = UiGeometry.float32, UiGeometry.bounds

breathPresentation = {}

function breathPresentation.blocks(value, maximum)
  local ratio = float(float(10 * float(value)) / float(maximum))
  return math.floor(ratio + 0.5)
end

function breathPresentation.warning(milliseconds)
  return milliseconds % 500 > 250
end

function breathPresentation.layout(size, interfaceScale, position, barPosition, iconWidth)
  local x = float(float(size[1] * 0.5) + float(position[1] * interfaceScale))
  local y = float(size[2] - float(position[2] * interfaceScale))
  -- Vec2I::operator*(int) converts the scalar before multiplication. The old
  -- bar offset and icon step therefore use truncated scale, not scaled pixels.
  local integerScale = math.modf(interfaceScale)
  return {
    center = {x, y},
    bar = {float(x + barPosition[1] * integerScale), float(y + barPosition[2] * integerScale)},
    step = iconWidth * integerScale
  }
end

local function refreshLayout()
  local nextScreen, nextScale = camera.screenSize(), interface.scale()
  if screen and nextScreen[1] == screen[1] and nextScreen[2] == screen[2] and nextScale == scale then return end
  screen, scale = nextScreen, nextScale
  geometry = breathPresentation.layout(screen, scale, offset, barOffset, emptyWidth)
  -- Widget clipping stays in interface pixels, while the Canvas commands below
  -- use physical pixels. Do not anchor to ceil(screen / scale): that shifts the
  -- original physical screen center on odd windows and fractional UI scales.
  local bounds = viewportBounds(screen, scale)
  pane.setSize(bounds)
  widget.setSize("breath", bounds)
  dirty = true
end

function init()
  canvas = widget.bindCanvas("breath")
  offset = root.assetJson("/interface.config:breathPos")
  barOffset = root.assetJson("/interface.config:breathBarPos")
  emptyWidth = root.imageSize("/interface/breath/empty.png")[1]
  screen, previousBlocks, previousWarning = nil, nil, nil
  refreshLayout()
end

function update(dt)
  refreshLayout()
end

function render()
  local blocks = breathPresentation.blocks(status.resource("breath"), status.resourceMax("breath"))
  local warning = blocks == 0 and breathPresentation.warning(sb.monotonicMilliseconds())
  if not dirty and blocks == previousBlocks and warning == previousWarning then return end
  canvas:clear()
  if blocks < 10 then
    canvas:drawImage("/interface/breath/breath.png", geometry.center, scale, nil, true)
    for index = 0, 9 do
      local image = index < blocks and "/interface/breath/breathbar.png"
          or (warning and "/interface/breath/warning.png" or "/interface/breath/empty.png")
      canvas:drawImage(image, {geometry.bar[1] + geometry.step * index, geometry.bar[2]}, scale)
    end
  end
  previousBlocks, previousWarning, dirty = blocks, warning, false
end
