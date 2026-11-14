EscapeMenu = {}

function EscapeMenu.config(source)
  local config = type(source) == "string" and root.assetJson(source) or source
  if config.size then return config end
  local layout = type(config.gui) == "string" and root.assetJson(config.gui) or config.gui
  -- Legacy Pane bounds came only from its background, not the child union.
  -- Resolve them before display so the original modal centers identically.
  config.size = {0, 0}
  for _, member in pairs(layout) do
    if member.type == "background" then
      config.size = {0, 0}
      for _, field in ipairs({"fileHeader", "fileBody", "fileFooter"}) do
        local image = member[field]
        if image and image ~= "" then
          local part = root.imageSize(image)
          config.size[1] = math.max(config.size[1], part[1])
          config.size[2] = config.size[2] + part[2]
        end
      end
    end
  end
  return config
end
