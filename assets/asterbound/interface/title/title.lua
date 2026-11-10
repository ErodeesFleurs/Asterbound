require "/scripts/vec2.lua"

local canvas
local backdropImages
function init()
  canvas = background.bindCanvas("canvas")
  backdropImages = root.assetJson("/interface/windowconfig/title.config:backdropImages")
end

function render(_data)
  canvas:clear()
  local window = canvas:size()
  for _, v in pairs(backdropImages) do
    local offset, image, scale, origin = table.unpack(v)
    origin = origin or {0.5, 1.0}
    local imageSize = vec2.mul(root.imageSize(image), scale)
    local position = vec2.add(vec2.mul(window, origin), vec2.sub(offset, vec2.mul(imageSize, vec2.sub(origin, 0.5))))
    canvas:drawImageDrawable(image, position, scale)
  end
end
