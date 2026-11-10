require "/interface/opensb/ui/manager.lua"
require "/interface/opensb/codex/unread.lua"
require "/interface/opensb/questdialogs/pending.lua"
require "/interface/opensb/escapemenu/layout.lua"
require "/interface/opensb/team/layout.lua"
require "/interface/opensb/actionbar/layout.lua"

local manager

function init()
  local definitions = root.assetJson("/interface.config:panes")
  if definitions.EscapeDialog then
    definitions.EscapeDialog.config = EscapeMenu.config(definitions.EscapeDialog.config)
  end
  for _, id in ipairs({"TeamInvite", "TeamInvitation", "TeamMemberMenu"}) do
    if definitions[id] then definitions[id].config = Team.config(definitions[id].config) end
  end
  if definitions.ActionBar then
    definitions.ActionBar.config = ActionBarLayout.config(definitions.ActionBar.config)
  end
  manager = UiManager.new(storage, definitions, interface.session())
  manager:restore()
end

function enqueue(id, event, payload)
  manager:enqueue(id, event, payload)
end

function dispatch(id, method, arguments)
  return manager:dispatch(id, method, arguments)
end

function open(id)
  manager:flush()
  return manager:open(id)
end

function toggle(id)
  manager:flush()
  return manager:toggle(id)
end

function input(event)
  return manager:input(event)
end
function flush()
  manager:flush()
end

function pauseRequested()
  return interface.paneDisplayed("EscapeDialog") or interface.paneDisplayed("Options")
end

function cursorOverride(screenPosition)
  if interface.paneDisplayed("ActionBar") then
    return interface.callPane("ActionBar", "cursorOverride", {screenPosition})
  end
end


function update(dt)
  manager:update()
  QuestDialogs.update(manager)
  CodexUnread.update(manager)
end

function suspend(session)
  manager:suspend(session)
end

function shutdown()
  if manager then manager:shutdown() end
end
