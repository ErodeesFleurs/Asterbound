require "/interface/opensb/ui/geometry.lua"

local settings
local state
local rendered = {}
local stages = {AnimateIn = "ScrollText", ScrollText = "Persist", Persist = "AnimateOut", AnimateOut = "Hidden"}

local f32 = UiGeometry.float32

local function trunc(value)
  return (math.modf(value))
end

local function background(image, body, footer)
  body, footer = body or "", footer or ""
  local key = image .. "\0" .. body .. "\0" .. footer
  if rendered.background ~= key then
    pane.setBackground(image, body, footer)
    rendered.background = key
  end
end

local function portrait(image)
  if rendered.portrait ~= image then
    widget.setImage("imgPortrait", image)
    rendered.portrait = image
  end
end

local function charLimit(limit)
  local key = limit
  if key == nil then key = false end
  if rendered.limit ~= key then
    widget.setTextCharLimit("lblMessage", limit)
    rendered.limit = key
  end
end

local function stopChatter()
  if state.sound and not state.sound.stopped then
    if state.sound.playing then pane.stopAllSounds(state.sound.path) end
    state.sound.stopped = true
    state.sound.playing = false
  end
end

local function visible(value)
  if rendered.visible ~= value then
    if value then pane.show() else pane.hide() end
    rendered.visible = value
  end
end

local function updateAnchorOffset()
  local ratio = f32(state.slideTimer / settings.slideTime)
  local remaining = f32(1 - ratio)
  local x = trunc(f32(f32(state.chatStart[1] * remaining) + f32(state.chatEnd[1] * ratio)))
  local y = trunc(f32(f32(state.chatStart[2] * remaining) + f32(state.chatEnd[2] * ratio)))
  if rendered.x ~= x or rendered.y ~= y then
    pane.setAnchorOffset({x, y})
    rendered.x, rendered.y = x, y
  end
end

local function percent()
  return state.duration ~= 0 and f32(state.timer / state.duration) or 0
end

local function paint()
  local stage = state.stage
  local content = stage == "ScrollText" or stage == "Persist"
  if rendered.content ~= content then
    widget.setVisible("imgPortrait", content)
    widget.setVisible("lblMessage", content)
    rendered.content = content
  end
  if stage == "Hidden" then
    background("", settings.animateInImage .. ":0")
    visible(false)
  elseif stage == "AnimateIn" then
    background("", settings.animateInImage .. ":" .. math.floor(f32(f32(1 - percent()) * settings.animateInFrames)))
    visible(true)
  elseif stage == "AnimateOut" then
    background("", settings.animateOutImage .. ":" .. math.floor(f32(f32(1 - percent()) * settings.animateOutFrames)))
    visible(true)
  else
    local message = state.message
    if rendered.text ~= message.text then
      widget.setText("lblMessage", message.text)
      rendered.text = message.text
    end
    background(settings.backgroundImage)
    if stage == "ScrollText" then
      -- The legacy portrait animation runs from remaining time, not elapsed time.
      local frame = trunc(f32(f32(state.timer / message.portraitSpeed) * message.portraitFrames)) % message.portraitFrames
      portrait((message.portraitImage:gsub("<frame>", tostring(frame))))
      charLimit(math.floor(f32(state.scrollLength * f32(1 - percent()))))
    else
      portrait((message.portraitImage:gsub("<frame>", "0")))
      charLimit(nil)
    end
    visible(true)
  end
end

local function enterStage(stage)
  state.stage = stage
  if stage == "AnimateIn" then
    state.duration = f32(settings.animateInTime)
  elseif stage == "ScrollText" then
    if state.message.textSpeed == 0 then return enterStage("Persist") end
    state.scrollLength = utf8.len(sb.stripEscapeCodes(state.message.text))
    state.duration = f32(state.scrollLength / state.message.textSpeed)
    if state.sound and not state.sound.stopped and not state.sound.playing then
      pane.playSound(state.sound.path, -1)
      state.sound.playing = true
    end
  elseif stage == "Persist" then
    state.duration = state.message.persistTime
    stopChatter()
  elseif stage == "AnimateOut" then
    state.duration = f32(settings.animateOutTime)
  end
  state.timer = state.duration
  paint()
end

function init()
  settings = root.assetJson("/interface/radiomessage/radiomessage.config")
  settings.slideTime = f32(settings.slideTime)
  if not storage.radio then
    storage.radio = {
      stage = "Hidden", duration = 0, timer = 0,
      chatStart = settings.chatOffset, chatEnd = settings.chatOffset,
      slideTimer = f32(settings.slideTime)
    }
  end
  state = storage.radio
  -- Reload/service rebinding restores presentation only: no dequeue, chat replay,
  -- stage transition or restart of the Pane-owned looping AudioInstance.
  paint()
  updateAnchorOffset()
end

function messageActive()
  return state.stage ~= "Hidden"
end

function setMessage(message)
  state.message = message
  if message.chatterSound ~= "" and message.textSpeed > 0 then
    stopChatter()
    state.sound = {path = message.chatterSound, playing = false, stopped = false}
  end
  enterStage("AnimateIn")
  updateAnchorOffset()
end

function setChatHeight(height)
  local target = {settings.chatOffset[1], settings.chatOffset[2] + height}
  if target[1] ~= state.chatEnd[1] or target[2] ~= state.chatEnd[2] then
    state.chatStart = pane.anchorOffset()
    state.chatEnd = target
    state.slideTimer = 0
  end
end

function interrupt()
  if state.stage ~= "Hidden" and state.stage ~= "AnimateOut" then enterStage("AnimateOut") end
end

function update(dt)
  dt = f32(dt)
  -- Advance exactly one stage per tick and discard timer overshoot, as GameTimer
  -- did. Presentation continues during cinematics; only the consumer is gated.
  if messageActive() then
    if state.timer > 0 then state.timer = math.max(0, f32(state.timer - dt))
    elseif state.timer < 0 then state.timer = math.min(0, f32(state.timer + dt)) end
    if state.timer == 0 then enterStage(stages[state.stage]) else paint() end
    state.slideTimer = math.min(f32(state.slideTimer + dt), f32(settings.slideTime))
    updateAnchorOffset()
  end

  local height = 0
  if interface.paneVisible("chat") then height = interface.callPane("chat", "layoutHeight", {}) or 0 end
  setChatHeight(height)
  if not interface.cinematicActive() and interface.session().hasPlayer then
    if player.interruptRadioMessage() then interrupt() end
    if not messageActive() then
      local message = player.pullPendingRadioMessage()
      if message then
        -- Native dequeue has already committed unique-message accounting. There
        -- is no visual-completion ack and this record must never replay on reload.
        setMessage(message)
        chat.addMessage(message.text, {
          mode = "RadioMessage", fromNick = message.senderName,
          portrait = sb.stripEscapeCodes((message.portraitImage:gsub("<frame>", "0"))),
          fromConnection = 0, showPane = false
        })
      else
        -- Legacy dismissal stops chatter only after the outgoing animation. If
        -- another message replaces it immediately, its Persist stage stops it.
        stopChatter()
      end
    end
  end
end

function dismissed()
  stopChatter()
end
