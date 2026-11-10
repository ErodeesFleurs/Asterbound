-- Public client-side UI policy requests. Requests are drained by the single
-- controller after the calling script returns, never inside a pane callback.
ui = {}

function ui.register(id, definition)
  interface.sendPaneEvent("$ui", "register", {id = id, definition = definition})
end

function ui.open(id)
  interface.sendPaneEvent("$ui", "open", {id = id})
end

function ui.toggle(id)
  interface.sendPaneEvent("$ui", "toggle", {id = id})
end

function ui.close(id)
  interface.sendPaneEvent("$ui", "close", {id = id})
end

function ui.send(id, event, payload)
  interface.sendPaneEvent(id, event, payload)
end
