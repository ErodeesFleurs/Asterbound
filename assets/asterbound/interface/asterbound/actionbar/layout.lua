ActionBarLayout = {}

function ActionBarLayout.config(source)
  local wrapper = type(source) == "string" and root.assetJson(source) or source
  local original = root.assetJson("/interface/windowconfig/actionbar.config")
  local bottom = root.getConfigurationPath("inventory.bottomActionBar") or false
  if bottom then
    original = sb.jsonMerge(original, root.assetJson("/interface/windowconfig/actionbarbottom.config"))
  end
  local layout = original.paneLayout
  wrapper.actionBar = original
  wrapper.bottomActionBar = bottom
  wrapper.gui = layout
  wrapper.scriptWidgetCallbacks = {"customClick", "customRight", "essentialClick", "pickupToActionBar", "swapCustomBar"}
  local size = {0, 0}
  local bodyHeight = 0
  for name, member in pairs(layout) do
    if member.type == "background" then
      for _, field in ipairs({"fileHeader", "fileBody", "fileFooter"}) do
        local image = member[field]
        if image and image ~= "" then
          local dimensions = root.imageSize(image)
          size[1] = math.max(size[1], dimensions[1])
          size[2] = size[2] + dimensions[2]
          if field == "fileBody" then bodyHeight = dimensions[2] end
          if bottom then member[field] = image .. "?flipy" end
        end
      end
      if bottom then member.fileHeader, member.fileFooter = member.fileFooter, member.fileHeader end
    elseif member.type == "panefeature" and bottom then
      member.anchor = "CenterBottom"
      local offset = member.offset or {0, 0}
      member.offset = {offset[1], -offset[2]}
    elseif name:match("^customBar%d+[LR]$") then
      member.callback = "customClick"
      member.rightClickCallback = "customRight"
    elseif name:match("^essentialBar%d+$") then
      member.callback = "essentialClick"
    end
  end
  -- Original Pane bounds came from the background, not the child union.
  wrapper.size = wrapper.size or size
  wrapper.actionBarBodyHeight = bodyHeight
  return wrapper
end
