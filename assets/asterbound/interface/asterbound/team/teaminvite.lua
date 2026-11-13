function init()
  -- Rebinding/reload is not a new show. A genuine close resets this flag.
  if not storage.shown then
    widget.setText("name", "")
    widget.focus("name")
    storage.shown = true
  end
end

function update(dt)
  if team.currentTeam() and not team.isTeamLeader() then pane.dismiss() end
end

function name()
end

function ok()
  team.invitePlayer(widget.getText("name"))
  pane.dismiss()
end

function close()
  pane.dismiss()
end

function displayed()
  if storage.shown then return end
  widget.setText("name", "")
  widget.focus("name")
  storage.shown = true
end

function dismissed()
  storage.shown = nil
end
