function patch(original)
  local size = original:size()
  local x = math.floor(size[1] / 2)
  local image = original:process(string.format("?crop=%d;0;%d;%d?scalenearest=472;1", x, x + 1, size[2]))
  image:copyInto({0, 0}, original:process(string.format("?crop=0;0;24;%d", size[2])))
  image:copyInto({448, 0}, original:process(string.format("?crop=%d;0;%d;%d", size[1] - 24, size[1], size[2])))
  return image
end
