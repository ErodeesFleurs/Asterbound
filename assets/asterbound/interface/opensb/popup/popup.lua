local function displayContent()
  local content = storage.content
  pane.setTitle(content.title, content.subtitle)
  widget.setText("message", content.message)
end

function init()
  -- A script/engine reload restores presentation, never the consumed show sound.
  if storage.content then displayContent() end
end

function showPopup(payload)
  storage.content = {
    message = payload.message,
    title = payload.title or "",
    subtitle = payload.subtitle or ""
  }
  displayContent()
  pane.show()
  local sounds = root.assetJson("/interface/windowconfig/popup.config:onShowSound")
  local sound = #sounds > 0 and sounds[math.random(#sounds)] or ""
  if payload.sound ~= nil then sound = payload.sound end
  if sound ~= "" then pane.playSound(sound) end
end

function close()
  storage.content = nil
  pane.dismiss()
end

function ok()
  close()
end
