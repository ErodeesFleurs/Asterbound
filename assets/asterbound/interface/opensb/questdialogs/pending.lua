-- Acknowledgment belongs to the native quest; presentation and modal priority
-- belong to Lua. The request/config survive controller and pane script reloads.
QuestDialogs = {}

function QuestDialogs.update(manager)
  if not manager.state.session.hasPlayer or not manager.state.session.inWorld
      or interface.paneLayerOccupied("ModalWindow") then return end
  local request = player.pendingQuestDialog("preview")
      or player.pendingQuestDialog("failed")
      or player.pendingQuestDialog("complete")
      or player.pendingQuestDialog("offer")
  if not request then return end
  local defaults = root.assetJson("/quests/quests.config:defaultGuiConfigs")
  local key = request.kind == "offer" and (#request.objectivePortrait > 0 and "newQuestPortrait" or "newQuest")
      or (request.kind == "complete" and "questComplete" or "questFailed")
  local source = root.assetJson(request.guiConfig or defaults[key])
  -- Legacy QuestPane bounds came from its background, not its children.
  -- Some authored layouts deliberately park unused controls far outside it.
  local size = {0, 0}
  for _, member in pairs(source.paneLayout) do
    if member.type == "background" then
      size = {0, 0}
      for _, field in ipairs({"fileHeader", "fileBody", "fileFooter"}) do
        local image = member[field]
        if image and image ~= "" then
          local part = root.imageSize(image)
          size[1], size[2] = math.max(size[1], part[1]), size[2] + part[2]
        end
      end
    end
  end
  local config = {
    scripts = {"/interface/opensb/questdialogs/questdialog.lua"},
    scriptWidgetCallbacks = {"close", "btnDecline", "btnAccept"},
    gui = source.paneLayout,
    size = size,
    onShowSound = source.onShowSound,
    questDialog = request
  }
  manager:register("QuestDialog", {config = config, layer = "ModalWindow", scope = "world"})
  if manager:open("QuestDialog") then
    if not player.acknowledgeQuestDialog(request.token) then
      manager:close("QuestDialog")
    end
  end
end
