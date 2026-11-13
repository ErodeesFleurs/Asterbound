function init()
  -- Only this dialog drains the native queue. The retained payload survives
  -- callback retirement/reload, but a genuine close discards it without a
  -- decline RPC, exactly like the original TeamInvitation.
  if not storage.invitation then storage.invitation = team.pullInvitation() end
  if not storage.invitation then
    pane.dismiss()
    return
  end
  widget.setText("inviterName", storage.invitation.name)
end

function ok()
  if storage.invitation then team.acceptInvitation(storage.invitation.uuid) end
  pane.dismiss()
end

function close()
  pane.dismiss()
end

function dismissed()
  storage.invitation = nil
end
