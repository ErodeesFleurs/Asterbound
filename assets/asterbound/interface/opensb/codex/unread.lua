-- The domain owns unread ordering and read flags. Do not checkpoint an unread
-- event: marking it read is the acknowledgment, including across live reload.
CodexUnread = {}

function CodexUnread.update(manager)
  if not manager.state.session.hasPlayer or interface.paneLayerOccupied("ModalWindow") then return end
  local id = player.getNewCodexId()
  if id and manager:open("codex") then
    interface.callPane("codex", "showCodex", {id})
  end
end
