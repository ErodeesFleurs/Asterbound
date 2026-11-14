-- Instance lifetime, scope, event ordering and semantic actions live here.
-- The native interface table only allocates, binds and displays real Panes.
UiManager = {}
UiManager.__index = UiManager

local function scopeExpired(definition, session, previous)
  if definition.scope == "player" then
    return not session.hasPlayer or not previous or previous.playerGeneration ~= session.playerGeneration
  end
  return definition.scope == "world" and
      (not session.inWorld or not previous or previous.worldGeneration ~= session.worldGeneration
        or previous.playerGeneration ~= session.playerGeneration)
end

function UiManager.new(checkpoint, definitions, session)
  checkpoint.pressed = checkpoint.pressed or {}
  local self = setmetatable({state = checkpoint, definitions = {}, pressed = checkpoint.pressed, flushing = false}, UiManager)
  checkpoint.open = checkpoint.open or {}
  checkpoint.registered = checkpoint.registered or {}
  self.previous = checkpoint.session
  checkpoint.queue = checkpoint.queue or jarray()
  checkpoint.session = session
  for id, definition in pairs(definitions) do
    self:validate(id, definition)
    self.definitions[id] = definition
  end
  return self
end

function UiManager:restore()
  local checkpoint, session, previous = self.state, self.state.session, self.previous
  for id, definition in pairs(checkpoint.registered) do
    self:validate(id, definition)
    self.definitions[id] = definition
  end
  -- A controller reload adopts session instances instead of creating a second
  -- presentation owner. Scoped instances are never adopted across their owner.
  local stale = {}
  for id in pairs(checkpoint.open) do
    if not self.definitions[id] then stale[id] = true end
  end
  for id, definition in pairs(self.definitions) do
    if scopeExpired(definition, session, previous) then
      stale[id] = true
    end
  end
  self:discardEvents(stale)
  for id in pairs(stale) do
    self:close(id)
    checkpoint.open[id] = nil
  end
  for _, id in ipairs(self:ordered()) do
    local definition = self.definitions[id]
    if checkpoint.open[id] or (checkpoint.open[id] == nil and definition.open) then
      self:open(id)
    end
  end
end

function UiManager:ordered()
  local ids = {}
  for id in pairs(self.definitions) do ids[#ids + 1] = id end
  table.sort(ids, function(a, b)
    local x, y = self.definitions[a].order or 0, self.definitions[b].order or 0
    return x == y and a < b or x < y
  end)
  return ids
end

function UiManager:validate(id, definition)
  assert(type(id) == "string" and interface.paneIdAvailable(id), "Invalid or reserved UI instance id")
  assert(type(definition) == "table" and definition.config, "A UI registration requires config")
  assert(definition.scope == nil or definition.scope == "session" or definition.scope == "player"
      or definition.scope == "world", "Invalid UI scope")
end

function UiManager:register(id, definition)
  self:validate(id, definition)
  if self.state.open[id] ~= nil then self:close(id) end
  self.definitions[id] = definition
  self.state.registered[id] = definition
  self.state.open[id] = nil
  if definition.open then self:open(id) end
end

function UiManager:open(id)
  local definition = assert(self.definitions[id], "Unknown UI registration: " .. id)
  if definition.scope == "world" and not self.state.session.inWorld then return false end
  if definition.scope == "player" and not self.state.session.hasPlayer then return false end
  if not interface.bindRegisteredPane(id) then
    local config = type(definition.config) == "string" and root.assetJson(definition.config) or definition.config
    interface.createPane(id, config, definition.layer or "Window")
  end
  self.state.open[id] = true
  interface.displayRegisteredPane(id)
  return true
end

function UiManager:toggle(id)
  local pane = interface.bindRegisteredPane(id)
  if pane and pane.isDisplayed() then
    pane.dismiss()
    self:close(id, self.definitions[id] and self.definitions[id].retain)
    return true
  end
  return self:open(id)
end

function UiManager:close(id, retain)
  for key, action in pairs(self.pressed) do
    if action.id == id then
      if action.action and self.state.open[id] and interface.bindRegisteredPane(id) then
        interface.callPane(id, "onAction", {action.action, false, {type = "cancel", key = key, cancelled = true}})
      end
      self.pressed[key] = nil
    end
  end
  if not retain then interface.destroyPane(id) end
  self.state.open[id] = false
end

function UiManager:discardEvents(closed)
  local retained = jarray()
  for _, message in ipairs(self.state.queue) do
    local target = message.id
    -- Registration declares session policy. Open/close requests are scoped to
    -- the instance they targeted and must not affect a replacement world UI.
    if target == "$ui" and message.event ~= "register" then target = message.payload.id end
    if not closed[target] then retained[#retained + 1] = message end
  end
  self.state.queue = retained
end

function UiManager:suspend(session)
  local previous = self.state.session
  local closed = {}
  for id, definition in pairs(self.definitions) do
    if scopeExpired(definition, session, previous) then
      closed[id] = true
    end
  end
  -- Never dispatch queued work through an old world's borrowed capabilities.
  self:discardEvents(closed)
  for id in pairs(closed) do
    self:close(id)
    self.state.open[id] = nil
  end
end

function UiManager:enqueue(id, event, payload)
  self.state.queue[#self.state.queue + 1] = {id = id, event = event, payload = payload}
end

function UiManager:flush()
  if self.flushing then return end
  self.flushing = true
  local pending = self.state.queue
  self.state.queue = jarray()
  local nextMessage = 1
  local ok, failure = pcall(function()
    for index, message in ipairs(pending) do
      nextMessage = index + 1
      if message.id == "$ui" then
        if message.event == "register" then self:register(message.payload.id, message.payload.definition)
        elseif message.event == "open" then self:open(message.payload.id)
        elseif message.event == "close" then self:close(message.payload.id)
        elseif message.event == "toggle" then self:toggle(message.payload.id)
        else error("Unknown UI operation: " .. message.event) end
      else
        local definition = assert(self.definitions[message.id], "Unknown UI registration: " .. message.id)
        if not self.state.open[message.id] then self:open(message.id) end
        if self.state.open[message.id] then
          local method = definition.events and definition.events[message.event]
          if method then interface.callPane(message.id, method, {message.payload})
          else interface.callPane(message.id, "onEvent", {message.event, message.payload}) end
        end
      end
    end
  end)
  self.flushing = false
  if not ok then
    local retained = jarray()
    for index = nextMessage, #pending do retained[#retained + 1] = pending[index] end
    for _, message in ipairs(self.state.queue) do retained[#retained + 1] = message end
    self.state.queue = retained
    error(failure, 0)
  end
end

function UiManager:dispatch(id, method, arguments)
  self:flush()
  if not self.state.open[id] then self:open(id) end
  local result = interface.callPane(id, method, arguments)
  self:flush()
  return result
end

function UiManager:input(event)
  local key = tostring(event.key)
  if event.type == "keyUp" then
    local action = self.pressed[key]
    self.pressed[key] = nil
    if not action then return false end
    if action.action and self.state.open[action.id] then interface.callPane(action.id, "onAction", {action.action, false, event}) end
    return true
  end
  -- A held global toggle must not repeatedly close/open on native key repeat.
  if self.pressed[key] and not self.pressed[key].action then return true end
  for _, id in ipairs(interface.paneInputTargets()) do
    local definition = self.definitions[id]
    local pane = self.state.open[id] and interface.bindRegisteredPane(id)
    if pane and pane.isDisplayed() and definition and definition.actions then
      for _, engineAction in ipairs(event.actions) do
        local action = definition.actions[engineAction]
        if action and interface.callPane(id, "onAction", {action, true, event}) then
          self.pressed[key] = {id = id, action = action}
          return true
        end
      end
    end
  end
  -- Native focus and modal barriers win even if their widgets declined the key.
  -- This path runs only after local/native input, and can create unopened panes.
  if event.keyboardCaptured or interface.paneLayerOccupied("ModalWindow") then return false end
  for _, engineAction in ipairs(event.actions) do
    for _, id in ipairs(self:ordered()) do
      local definition = self.definitions[id]
      local operation = definition.globalActions and definition.globalActions[engineAction]
      if operation then
        assert(operation == "open" or operation == "toggle", "Invalid global UI operation")
        if self[operation](self, id) then
          self.pressed[key] = {id = id}
          return true
        end
      end
    end
  end
  return false
end

function UiManager:update()
  self:flush()
  local closed = {}
  for id, open in pairs(self.state.open) do
    if open then
      local pane = interface.bindRegisteredPane(id)
      if not pane or not pane.isDisplayed() then closed[#closed + 1] = id end
    end
  end
  for _, id in ipairs(closed) do
    -- Ordinary dismissal may retain a window's native widgets and script
    -- storage for reopening. Explicit close and scope expiry still destroy it.
    self:close(id, self.definitions[id] and self.definitions[id].retain)
  end
end

function UiManager:shutdown()
  local open = {}
  for id in pairs(self.state.open) do open[#open + 1] = id end
  for _, id in ipairs(open) do self:close(id) end
  self.state.queue = jarray()
end
