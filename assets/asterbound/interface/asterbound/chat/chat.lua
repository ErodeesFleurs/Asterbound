-- Chat is an ordinary Lua-managed Pane using Canvas and the native text editor.
-- Messages stay raw; wrapping/portrait layout is cached independently of storage.
local state, chrome, log, inputPaint, appearance, style, inputStyle, tabs, layouts, images
local elapsed, dirty, lastAlpha, editorSnapshot, restoring, party, dragging, hover, clientStorage

local function copy(t)
  local result = {}
  for k, v in pairs(t) do result[k] = v end
  return result
end

local function prefix(text, count)
  return text:sub(1, (utf8.offset(text, count + 1) or (#text + 1)) - 1)
end

local function formatMessage(template, tags)
  local pieces, cursor = {}, 1
  while cursor <= #template do
    local first = template:find("<", cursor, true)
    if not first then pieces[#pieces + 1] = template:sub(cursor); break end
    pieces[#pieces + 1] = template:sub(cursor, first - 1)
    if template:sub(first + 1, first + 1) == "<" then
      pieces[#pieces + 1] = "<"
      cursor = first + 2
    else
      local last = assert(template:find(">", first + 1, true), "Unclosed chat format tag")
      local tag = template:sub(first + 1, last - 1)
      pieces[#pieces + 1] = assert(tags[tag], "Unknown chat format tag: " .. tag)
      cursor = last + 1
    end
  end
  return table.concat(pieces)
end

local function checkpoint()
  if not state or restoring then return end
  local editor = widget.getTextEditorState("input")
  state.input = editor.text
  state.cursor = editor.cursor
  state.anchor = editor.anchor
  state.focused = widget.hasFocus("input") == true
end

local function wake()
  state.age = 0
  dirty = true
end

local function bodyHeight()
  return state.expanded and appearance.expandedBodyHeight or appearance.bodyHeight
end

local function resize()
  widget.setSize("log", {262, bodyHeight() + 2})
  widget.setSize("chrome", {290, bodyHeight() + 28})
  pane.setSize({290, bodyHeight() + 28})
  dirty = true
end

function init()
  local original = root.assetJson("/interface/chat/chat.config")
  appearance = original.config
  style = copy(appearance.textStyle or {})
  style.fontSize = style.fontSize or 8
  style.lineSpacing = appearance.lineHeight or 1
  inputStyle = root.assetJson("/interface.config:textBoxTextStyle")
  inputStyle.fontSize = 8
  inputStyle.color = "green"
  tabs = original.gui.filterGroup.buttons
  images = {}
  for _, name in ipairs({"header", "body", "footer", "chat", "up", "upOver", "down", "downOver", "bottom", "bottomOver", "atbottom"}) do
    local path = "/interface/chat/" .. name .. ".png"
    images[name] = {path = path, size = root.imageSize(path)}
  end
  clientStorage = client.storage("chatHistory")
  state = storage
  state.messages = state.messages or jarray()
  state.history = state.history or clientStorage.get(jarray())
  state.tab = state.tab or 1
  state.scroll = state.scroll or 0
  state.historyIndex = state.historyIndex or 0
  state.draft = state.draft or ""
  state.age = state.age or 0
  state.expanded = state.expanded or false
  layouts = {}
  chrome = widget.bindCanvas("chrome")
  log = widget.bindCanvas("log")
  inputPaint = widget.bindCanvas("inputPaint")
  restoring = true
  widget.setTextEditorRendering("input", false)
  widget.setTextStyle("input", inputStyle)
  widget.setText("input", state.input or "")
  widget.setTextSelection("input", state.anchor or 0, state.cursor or 0)
  if state.focused then widget.focus("input") else widget.blur("input") end
  restoring = false
  elapsed, lastAlpha, dirty, editorSnapshot = 0, -1, true, ""
  party = player and #(player.teamMembers() or {}) > 0 or false
  resize()
end

function uninit()
  checkpoint()
end

function inputChanged()
  if restoring or not state then return end
  wake()
  checkpoint()
end

function hasFocus()
  return widget.hasFocus("input") == true
end

function currentChat()
  return widget.getText("input")
end

function setCurrentChat(text, moveCursor)
  local before = widget.getTextEditorState("input")
  restoring = true
  widget.setText("input", text)
  local actual = widget.getText("input")
  if moveCursor then
    widget.setCursorPosition("input", utf8.len(actual))
  else
    widget.setCursorPosition("input", math.min(before.cursor, utf8.len(actual)))
  end
  restoring = false
  dirty = true
  checkpoint()
  return actual == text
end

function startChat()
  pane.show()
  state.focused = true
  widget.focus("input")
  wake()
  checkpoint()
end

function startCommand()
  setCurrentChat("/", true)
  startChat()
end

function stopChat()
  state.focused = false
  widget.blur("input")
  state.historyIndex = 0
  state.draft = ""
  setCurrentChat("", true)
  wake()
  checkpoint()
end

function clearCurrentChat()
  state.historyIndex = 0
  state.draft = ""
  return setCurrentChat("", true)
end

function sendMode()
  return tabs[state.tab].data.sendMode or "Broadcast"
end

function addHistory(text)
  if text == "" then return end
  if state.history[1] ~= text then
    table.insert(state.history, 1, text)
    while #state.history > appearance.chatHistoryLimit do table.remove(state.history) end
    clientStorage.set(state.history)
  end
  state.historyIndex = 0
  wake()
  checkpoint()
end

function incrementIndex()
  if #state.history == 0 then return end
  if state.historyIndex == 0 then state.draft = currentChat() end
  state.historyIndex = math.min(#state.history, state.historyIndex + 1)
  setCurrentChat(state.history[state.historyIndex], true)
  wake()
end

function decrementIndex()
  state.historyIndex = math.max(0, state.historyIndex - 1)
  setCurrentChat(state.historyIndex == 0 and state.draft or state.history[state.historyIndex], true)
  wake()
end

function addMessages(messages, showPane)
  for _, message in ipairs(messages) do
    table.insert(state.messages, 1, message)
  end
  while #state.messages > appearance.chatHistoryLimit do
    layouts[state.messages[#state.messages]] = nil
    table.remove(state.messages)
  end
  if showPane ~= false then wake() end
  dirty = true
  checkpoint()
end

function receiveMessages(event)
  addMessages(event.messages, event.showPane)
end

function clear(count)
  count = math.min(count or #state.messages, #state.messages)
  for _ = 1, count do layouts[state.messages[1]] = nil; table.remove(state.messages, 1) end
  state.scroll = 0
  dirty = true
  checkpoint()
end

function visible()
  if hasFocus() then return 1 end
  return math.max(0, math.min(1, 1 - math.max(0, state.age - appearance.visTime) / appearance.fadeRate))
end

function layoutHeight()
  return visible() > 0.1 and (bodyHeight() + 28) or 0
end

local function filtered(message)
  local filter = tabs[state.tab].data.filter
  if #filter == 0 then return true end
  for _, mode in ipairs(filter) do if mode == message.context.mode then return true end end
  return false
end

local function messageLayout(message)
  if layouts[message] then return layouts[message] end
  local text = message.text
  if message.fromNick and not message.portrait then
    text = formatMessage(appearance.chatFormatString, {fromNick = message.fromNick, text = text})
  end
  local mode = message.context.mode
  local colorKey = mode:sub(1, 1):lower() .. mode:sub(2)
  text = (appearance.colors[colorKey] or "^reset;") .. "^set;" .. text
  local x = message.portrait and appearance.portraitTextOffset[1] or 0
  local bounds = log:measureText(text, {position = {x, 0}, horizontalAnchor = "left", verticalAnchor = "bottom", wrapWidth = 260 - x}, style)
  local height = bounds[4] - bounds[2] + (style.lineSpacing - 1) * style.fontSize
  local portraitSize
  if message.portrait then
    portraitSize = root.imageSize(appearance.portraitBackground)
    height = math.max(height, portraitSize[2] + appearance.portraitVerticalMargin)
  end
  local layout = {text = text, height = math.ceil(height), x = x, portraitSize = portraitSize}
  layouts[message] = layout
  return layout
end

local function scrollBy(amount)
  local total = 0
  for _, message in ipairs(state.messages) do if filtered(message) then total = total + messageLayout(message).height end end
  state.scroll = math.max(0, math.min(math.max(0, total - style.fontSize), state.scroll + amount))
  wake()
  checkpoint()
end

function scrollUp() scrollBy(style.fontSize * style.lineSpacing) end
function scrollDown() scrollBy(-style.fontSize * style.lineSpacing) end
function scrollBottom() state.scroll = 0; wake(); checkpoint() end

function onAction(action, pressed, event)
  if not pressed then return false end
  if event and event.keyboardCaptured and not hasFocus() then return false end
  if action == "begin" and not hasFocus() then startChat(); return true end
  if action == "command" and not hasFocus() then startCommand(); return true end
  if not hasFocus() then return false end
  if action == "send" then
    local text = currentChat()
    if text ~= "" then
      if text:sub(1, 1) == "/" then
        for _, result in ipairs(chat.command(text)) do chat.addMessage(result) end
      else
        chat.send(text, sendMode())
      end
      addHistory(text)
    end
    clearCurrentChat()
    stopChat()
  elseif action == "stop" then stopChat()
  elseif action == "previous" then incrementIndex()
  elseif action == "next" then decrementIndex()
  elseif action == "pageUp" then scrollUp()
  elseif action == "pageDown" then scrollDown()
  else return false end
  return true
end

local function inside(p, rect)
  return p[1] >= rect[1] and p[2] >= rect[2] and p[1] < rect[3] and p[2] < rect[4]
end

local function cursorAt(x)
  local editor = widget.getTextEditorState("input")
  local target = x - 44 + math.max(0, editor.cursorOffset - 241)
  local text = editor.displayText
  local best, distance = 0, math.abs(target)
  for i = 1, utf8.len(text) do
    local bounds = chrome:measureText(prefix(text, i), {position = {0, 0}}, inputStyle)
    local nextDistance = math.abs(bounds[3] - bounds[1] - target)
    if nextDistance > distance then break end
    best, distance = i, nextDistance
  end
  return best
end

function onPointer(event)
  local p = event.position
  if not p then return false end
  if event.type == "buttonUp" then
    local wasDragging = dragging ~= nil
    dragging = nil
    return wasDragging
  end
  if dragging and event.type == "move" then
    widget.setTextSelection("input", dragging, cursorAt(p[1]))
    dirty = true
    checkpoint()
    return true
  end
  local inChat = inside(p, {0, 0, 290, bodyHeight() + 28})
  if hover and not inChat then hover = nil; dirty = true end
  if not inChat or visible() == 0 then return false end
  hover = p
  wake()
  if event.type == "wheel" then scrollBy(event.direction * style.fontSize * style.lineSpacing); return true end
  if event.type ~= "buttonDown" or event.button ~= 0 then return false end
  if inside(p, {20, 10, 290, 24}) then
    startChat()
    local cursor = cursorAt(p[1])
    dragging = cursor
    widget.setTextSelection("input", cursor, cursor)
    checkpoint()
  elseif p[2] < 10 and p[1] >= 20 then
    for index, tab in ipairs(tabs) do
      if p[1] >= tab.position[1] and p[1] < tab.position[1] + 40 then
        if tab.data.sendMode ~= "Party" or party then state.tab = index; state.scroll = 0; checkpoint() end
        break
      end
    end
  elseif p[1] < 20 then
    if p[2] < 22 then scrollBottom()
    elseif p[2] >= bodyHeight() + 20 then scrollUp()
    else scrollDown() end
  elseif p[2] >= 25 then state.expanded = not state.expanded; resize(); checkpoint()
  else return false end
  return true
end
function onInput(event)
  return onPointer(event)
end


function update(dt)
  elapsed = elapsed + dt
  state.age = hasFocus() and 0 or state.age + dt
  local nextParty = player and #(player.teamMembers() or {}) > 0 or false
  if nextParty ~= party then
    party = nextParty
    if not party and sendMode() == "Party" then state.tab = 1; state.scroll = 0 end
    dirty = true
    checkpoint()
  end
  local editor = widget.getTextEditorState("input")
  local snapshot = editor.text .. ":" .. editor.cursor .. ":" .. editor.anchor .. ":" .. tostring(hasFocus())
  if snapshot ~= editorSnapshot then editorSnapshot = snapshot; dirty = true; checkpoint() end
end

local function image(name, position, color)
  chrome:drawImage(images[name].path, position, 1, color)
end

local function drawChrome(alpha)
  chrome:clear()
  local tint = {255, 255, 255, alpha}
  local h = bodyHeight() + 28
  image("footer", {0, 0}, tint)
  chrome:drawTiledImage(images.body.path, {0, 0}, {0, images.footer.size[2], 290, h - images.header.size[2]}, 1, tint)
  image("header", {0, h - images.header.size[2]}, tint)
  image("chat", {20, 10}, tint)
  local upY = bodyHeight() + 25 - images.up.size[2]
  image(hover and inside(hover, {4, upY, 20, upY + images.up.size[2]}) and "upOver" or "up", {4, upY}, tint)
  image(hover and inside(hover, {4, 22, 20, 22 + images.down.size[2]}) and "downOver" or "down", {4, 22}, tint)
  local bottomImage = state.scroll == 0 and "bottom" or "atbottom"
  image(hover and inside(hover, {4, 1, 20, 1 + images.bottom.size[2]}) and "bottomOver" or bottomImage, {4, 1}, tint)
  for index, tab in ipairs(tabs) do
    chrome:drawImage(index == state.tab and tab.baseImageChecked or tab.baseImage, tab.position, 1, tint)
    local color = index == state.tab and {255, 255, 255, alpha} or {128, 128, 128, alpha}
    if tab.data.sendMode == "Party" and not party then color = {50, 50, 50, alpha} end
    chrome:drawText(tab.text, {position = {tab.position[1] + 20, tab.position[2] + 4}, horizontalAnchor = "mid", verticalAnchor = "mid"}, 8, color)
  end
  local green = {0, 255, 0, alpha}
  chrome:drawText("Say:", {position = {24, 13}, horizontalAnchor = "left", verticalAnchor = "bottom"}, 8, green)
  inputPaint:clear()
  local editor = widget.getTextEditorState("input")
  local offset = math.max(0, editor.cursorOffset - 241)
  local x = -offset
  local textStyle = copy(inputStyle)
  textStyle.color = green
  if editor.selectionStart ~= editor.selectionEnd then
    local a = chrome:measureText(prefix(editor.displayText, editor.selectionStart), {position = {0, 0}}, inputStyle)
    local b = chrome:measureText(prefix(editor.displayText, editor.selectionEnd), {position = {0, 0}}, inputStyle)
    local left, right = math.max(0, x + a[3]), math.min(242, x + b[3])
    if right > left then inputPaint:drawRect({left, 0, right, 10}, {70, 100, 140, math.floor(alpha * 0.7)}) end
  end
  inputPaint:drawText(editor.displayText, {position = {x, 1}, horizontalAnchor = "left", verticalAnchor = "bottom"}, textStyle)
  if hasFocus() and elapsed % 1 < 0.5 then
    inputPaint:drawRect({math.min(241, editor.cursorOffset), 0, 1 + math.min(241, editor.cursorOffset), 9}, green)
  end
end

local function drawLog(alpha)
  log:clear()
  local y = 1 - state.scroll
  local messageStyle = copy(style)
  messageStyle.color = {255, 255, 255, alpha}
  local tint = {255, 255, 255, alpha}
  for _, message in ipairs(state.messages) do
    if filtered(message) then
      local layout = messageLayout(message)
      if y + layout.height > 0 then
        if y > bodyHeight() + 2 then break end
        local position = {layout.x + 1, y}
        local anchor = "bottom"
        if message.portrait then
          local imageY = y + math.floor(layout.height / 2) - math.floor(layout.portraitSize[2] / 2)
          log:drawImage(appearance.portraitBackground, {1, imageY}, 1, tint)
          log:drawImage(message.portrait, {1 + appearance.portraitImageOffset[1], imageY + appearance.portraitImageOffset[2]}, appearance.portraitScale, tint)
          position[2] = y + math.floor(layout.height / 2) + appearance.portraitTextOffset[2]
          anchor = "mid"
        end
        log:drawText(layout.text, {position = position, horizontalAnchor = "left", verticalAnchor = anchor, wrapWidth = 260 - layout.x}, messageStyle)
      end
      y = y + layout.height
    end
  end
end

function render()
  local alpha = math.floor(visible() * 255)
  local blink = hasFocus() and math.floor(elapsed * 2) or 0
  if dirty or alpha ~= lastAlpha or blink ~= state.lastBlink then
    if alpha == 0 then chrome:clear(); log:clear(); inputPaint:clear()
    else drawChrome(alpha); drawLog(alpha) end
    dirty, lastAlpha, state.lastBlink = false, alpha, blink
  end
end
