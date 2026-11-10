local layout = "scrollArea.verticalLayout."
local mainList, sideList = layout .. "mainQuestList", layout .. "sideQuestList"
local rows, detailsId, detailsRevision, refreshTimer, trackLabel, untrackLabel
local active, tracked, abandonable
local portraitRevision, rewardsRevision
local refreshing = false

local function details()
  local id = storage.selectedId
  local same = detailsId == id
  local view = id and player.questLogDetails(id, same and detailsRevision or nil,
      same and portraitRevision or nil, same and rewardsRevision or nil)
  local nextActive = view ~= nil and view.active or false
  local nextTracked = nextActive and view.tracked or false
  local nextAbandonable = view ~= nil and view.canAbandon or false
  if active ~= nextActive then widget.setButtonEnabled("btnToggleTracking", nextActive) end
  if tracked ~= nextTracked then widget.setText("btnToggleTracking", nextTracked and untrackLabel or trackLabel) end
  if abandonable ~= nextAbandonable then widget.setButtonEnabled("btnAbandon", nextAbandonable) end
  active, tracked, abandonable = nextActive, nextTracked, nextAbandonable
  if not view then
    if detailsId == false then return end
    detailsId, detailsRevision = false, nil
    portraitRevision, rewardsRevision = nil, nil
    widget.setText("lblQuestTitle", "")
    widget.setText("lblQuestBody", "")
    widget.setText("lblPortraitTitle", "")
    widget.setImageDrawables("imgPortrait", {})
    widget.setItemGridItems("rewardItems", {}, 5)
    for _, name in ipairs({"imgPolaroid", "imgPolaroidBack", "lblRewards", "rewardItems"}) do widget.setVisible(name, false) end
    return
  end
  player.markQuestRead(id)
  detailsId, detailsRevision = id, view.revision
  portraitRevision, rewardsRevision = view.portraitRevision, view.rewardsRevision
  if view.title ~= nil then
    widget.setText("lblQuestTitle", view.title)
    widget.setText("lblQuestBody", view.text)
  end
  if view.portraitTitle ~= nil then
    widget.setText("lblPortraitTitle", view.portraitTitle)
    widget.setImageDrawables("imgPortrait", view.portrait or {}, {-1, 1})
    widget.setVisible("imgPolaroid", view.portrait ~= nil)
    widget.setVisible("imgPolaroidBack", view.portrait ~= nil)
  end
  if view.rewards ~= nil then
    widget.setItemGridItems("rewardItems", view.rewards, 5)
    widget.setVisible("lblRewards", #view.rewards > 0)
    widget.setVisible("rewardItems", #view.rewards > 0)
  end
end

local function refresh(force)
  local selectedFilter = widget.getSelectedData("filter") or "inProgress"
  local completed = selectedFilter:lower() == "completed"
  if not completed and selectedFilter:lower() ~= "inprogress" then error("Unknown quest filter '" .. selectedFilter .. "'") end
  local snapshot = player.questLogEntries(completed)
  local entries, present = snapshot.entries, {}
  local rebuild = force or not rows or #rows ~= #entries
  for index, entry in ipairs(entries) do
    present[entry.id] = true
    if not rows or not rows[index] or rows[index].id ~= entry.id or rows[index].main ~= entry.main then rebuild = true end
  end
  if not present[storage.selectedId] then storage.selectedId = present[snapshot.current] and snapshot.current or nil end
  refreshing = true
  if rebuild then
    widget.clearListItems(mainList)
    widget.clearListItems(sideList)
  end
  local mainCount, sideCount = 0, 0
  for index, entry in ipairs(entries) do
    local list = entry.main and mainList or sideList
    if entry.main then mainCount = mainCount + 1 else sideCount = sideCount + 1 end
    local previous = not rebuild and rows[index] or nil
    local name = previous and previous.name or widget.addListItem(list)
    entry.name = name
    local row = list .. "." .. name
    if not previous then widget.setData(row, entry.id) end
    if not previous or previous.title ~= entry.title then widget.setText(row .. ".lblQuestEntry", entry.title) end
    if not previous or previous.unread ~= entry.unread then widget.setVisible(row .. ".imgNew", entry.unread) end
    if not previous or previous.tracked ~= entry.tracked then widget.setVisible(row .. ".imgTracked", entry.tracked) end
    if not previous or previous.currentWorld ~= entry.currentWorld then widget.setVisible(row .. ".imgCurrent", entry.currentWorld) end
    if not previous or previous.portraitRevision ~= entry.portraitRevision then widget.setImageDrawables(row .. ".imgPortrait", player.questPortrait(entry.id, "QuestStarted") or {}) end
    if entry.id == storage.selectedId then widget.setListSelected(list, name) end
  end
  widget.setVisible(layout .. "mainQuestHeader", mainCount > 0)
  widget.setVisible(layout .. "sideQuestHeader", sideCount > 0)
  widget.refreshLayout("scrollArea.verticalLayout")
  rows = entries
  refreshing = false
  refreshTimer = 30
end

local function select(list, other)
  if refreshing then return end
  local selected = widget.getListSelected(list)
  if selected then
    refreshing = true
    widget.clearListSelected(other)
    refreshing = false
    storage.selectedId = widget.getData(list .. "." .. selected)
  else
    storage.selectedId = nil
  end
  details()
end

function mainQuestList() select(mainList, sideList) end
function sideQuestList() select(sideList, mainList) end

function filter()
  if refreshing then return end
  refresh(true)
  details()
end

function btnToggleTracking()
  if storage.selectedId and player.hasActiveQuest(storage.selectedId) then player.setTrackedQuest(storage.selectedId) end
  details()
end

function btnAbandon()
  if storage.selectedId then player.abandonQuest(storage.selectedId) end
  refresh(false)
  details()
end

function init()
  local config = root.assetJson("/interface/windowconfig/questlog.config")
  trackLabel, untrackLabel = config.trackLabel, config.untrackLabel
  rows, detailsId, detailsRevision = nil, nil, nil
  portraitRevision, rewardsRevision = nil, nil
  active, tracked, abandonable = nil, nil, nil
  widget.setScissoringEnabled(mainList, false)
  widget.setScissoringEnabled(sideList, false)
  refresh(true)
  details()
end

function update()
  details()
  refreshTimer = refreshTimer - 1
  if refreshTimer < 0 then refresh(false) end
end
