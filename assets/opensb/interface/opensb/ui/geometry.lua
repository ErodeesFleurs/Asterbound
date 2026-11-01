UiGeometry = {}

function UiGeometry.float32(value)
  return (string.unpack("f", string.pack("f", value)))
end

local float = UiGeometry.float32

function UiGeometry.bounds(screen, scale)
  return {math.ceil(float(screen[1] / scale)), math.ceil(float(screen[2] / scale))}
end
