require "/interface/opensb/ui/geometry.lua"

mainbarPresentation = {}

local float = UiGeometry.float32

function mainbarPresentation.layout(config, screen, scale)
  local origin = {
    float(screen[1] - float(config.size[1] * scale)),
    float(screen[2] - float(config.size[2] * scale))
  }
  local result = {origin = origin, positions = {}, polygons = {}}
  for name, button in pairs(config) do
    if type(button) == "table" and button.pos and button.poly then
      result.positions[name] = {
        float(origin[1] + float(button.pos[1] * scale)),
        float(origin[2] + float(button.pos[2] * scale))
      }
      local polygon = {}
      for _, vertex in ipairs(button.poly) do
        polygon[#polygon + 1] = {
          float(origin[1] + float(float(float(vertex[1] + origin[1]) - origin[1]) * scale)),
          float(origin[2] + float(float(float(vertex[2] + origin[2]) - origin[2]) * scale))
        }
      end
      result.polygons[name] = polygon
    end
  end
  return result
end

function mainbarPresentation.contains(polygon, point)
  local winding = 0
  for index, first in ipairs(polygon) do
    local second = polygon[index % #polygon + 1]
    local cross = float(float(float(second[1] - first[1]) * float(point[2] - first[2]))
        - float(float(point[1] - first[1]) * float(second[2] - first[2])))
    if first[2] <= point[2] then
      if second[2] > point[2] and cross > 0 then winding = winding + 1 end
    elseif second[2] <= point[2] and cross < 0 then
      winding = winding - 1
    end
  end
  return winding ~= 0
end
