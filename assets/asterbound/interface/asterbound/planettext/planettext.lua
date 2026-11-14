require "/interface/asterbound/ui/geometry.lua"

local duration, fadeTime, format, offset
local previousName, previousScale, previousPosition, previousAlpha

local float = UiGeometry.float32

local function visible(show)
  if storage.visible ~= show then
    storage.visible = show
    if show then pane.show() else pane.hide() end
  end
end

function init()
  duration = float(root.assetJson("/interface.config:planetNameTime"))
  fadeTime = float(root.assetJson("/interface.config:planetNameFadeTime"))
  format = root.assetJson("/interface.config:planetNameFormatString")
  offset = root.assetJson("/interface.config:planetTextOffset")
  widget.setTextStyle("planetText", root.assetJson("/interface.config:planetTextStyle"))
  pane.setScissoringEnabled(false)
  if storage.timer == nil then
    storage.timer = duration
    pane.setPosition(offset)
  end
  -- The session pane and its real label survive script reload/service rebinding.
  -- Restore presentation, but never advance/reset the countdown during init.
  if storage.alpha ~= nil then
    widget.setFontColor("planetText", "white", storage.alpha)
    previousAlpha = storage.alpha
  end
  if storage.visible then pane.show() else pane.hide() end
end

function update(dt)
  local session = interface.session()
  local inWorld = session.hasPlayer and player.inWorld()
  if interface.cinematicActive() then
    if not inWorld then
      storage.timer = duration
      visible(false)
    end
    return
  end

  -- GameTimer::tick uses float32 approach(0, timer, dt), before all display work.
  local timer = storage.timer
  dt = float(dt)
  if timer > 0 then
    timer = math.max(float(timer - dt), 0)
  elseif timer < 0 then
    timer = math.min(float(timer + dt), 0)
  end
  storage.timer = timer
  if timer == 0 then visible(false); return end

  if inWorld then
    local name = session.inWorld and world.name() or nil
    if name and name ~= "" then
      local scale = interface.scale()
      if scale ~= previousScale then
        -- Vec2I's scalar operator takes int: truncate the float32 ratio
        -- before multiplying, not each component of a floating-point vector.
        local ratio = math.modf(float(2 / scale))
        local position = {offset[1] * ratio, offset[2] * ratio}
        if not previousPosition or position[1] ~= previousPosition[1] or position[2] ~= previousPosition[2] then
          pane.setPosition(position)
          previousPosition = position
        end
        previousScale = scale
      end
      if name ~= previousName then
        widget.setText("planetText", sb.formatTags(format, {worldName = name}))
        previousName = name
      end
      visible(true)
    end
  end

  -- Missing/empty names retain the old text and visibility, but still fade.
  local alpha = timer < fadeTime and float(timer / fadeTime) or 1
  if alpha ~= previousAlpha then
    widget.setFontColor("planetText", "white", alpha)
    previousAlpha, storage.alpha = alpha, alpha
  end
end
