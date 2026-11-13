Team = {}

function Team.config(source)
  local config = type(source) == "string" and root.assetJson(source) or source
  if config.size then return config end
  local layout = type(config.gui) == "string" and root.assetJson(config.gui) or config.gui
  -- The native helpers sized their panes from the background alone. Preserve
  -- authored child coordinates without letting BaseScriptPane's child union
  -- change centering or the menu's outside-click rectangle.
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
