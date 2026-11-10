local request

function init()
  request = config.getParameter("questDialog")
  widget.setText("lblQuestTitle", request.title)
  widget.setText("lblQuestBody", request.text)
  widget.setImageDrawables("portraitImage", request.portrait)
  widget.setText("portraitTitle", request.portraitTitle)
  widget.setItemGridItems("rewardItems", request.rewards, 5)
  if request.kind == "offer" then
    widget.setButtonEnabled("btnDecline", request.canDecline)
    if #request.objectivePortrait > 0 and widget.getPosition("objectivePortraitImage") then
      widget.setImageDrawables("objectivePortraitImage", request.objectivePortrait, {-1, 1})
      for _, name in ipairs({"objectivePortraitTitle", "imgPolaroid", "imgPolaroidBack"}) do
        assert(widget.getPosition(name), "Missing quest interface widget '" .. name .. "'")
        widget.setVisible(name, true)
      end
      widget.setText("objectivePortraitTitle", request.objectivePortraitTitle)
    end
    widget.setVisible("rewardItems", #request.rewards > 0)
    widget.setVisible("lblRewards", #request.rewards > 0)
  else
    pane.setScissoringEnabled(false)
    if request.kind == "complete" then widget.setText("lblMoneyAmount", tostring(request.money)) end
  end
  if not storage.shown then
    local sounds = config.getParameter("onShowSound")
    assert(type(sounds) == "table", "Quest dialog requires onShowSound")
    if #sounds > 0 then
      local sound = sounds[math.random(#sounds)]
      if sound ~= "" then widget.playSound(sound) end
    end
    storage.shown = true
  end
end

local function decide(decision)
  if storage.finished then return end
  if request.kind == "offer" then
    storage.decision = decision
  else
    -- Only a real close/accept/decline widget action plays completion cinema.
    -- Escape, scope expiry and external dismissal merely retire the lease.
    local cinema = player.questDialogCinema(request.token)
    if cinema then interface.playCinematic(cinema) end
    player.finishQuestDialog(request.token, "close")
    storage.finished = true
  end
  pane.dismiss()
end

function close() decide("cancel") end
function btnDecline() decide("decline") end
function btnAccept() decide("accept") end

function update()
  if not player.questDialogValid(request.token) then pane.dismiss() end
end

function dismissed()
  if not storage.finished then
    player.finishQuestDialog(request.token, storage.decision or "cancel")
    storage.finished = true
  end
end
