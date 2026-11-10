local essentialNames = {"beamaxe", "wiretool", "painttool", "inspectiontool"}
local view, hover, dirty, style
local itemStates, overlays = {}, {}

local function within(name, screenPosition)
  local origin = pane.getScreenPosition()
  local position, size = widget.getPosition(name), widget.getSize(name)
  local x, y = screenPosition[1] - origin[1] - position[1], screenPosition[2] - origin[2] - position[2]
  return x >= 0 and y >= 0 and x <= size[1] and y <= size[2]
end

local function setOverlay(name, visible)
  if overlays[name] ~= visible then
    overlays[name] = visible
    widget.setVisible(name, visible)
  end
end

local function setSlot(name, item, durability, count, secondary, highlight)
  local handle = item and item.handle
  local state = itemStates[name]
  if not state or state.handle ~= handle then widget.setItemSlotHandle(name, handle) end
  if not state or state.durability ~= durability or state.count ~= count
      or state.secondary ~= secondary or state.highlight ~= highlight then
    widget.setItemSlotDisplay(name, {durability = durability, count = count, secondaryIcon = secondary, highlight = highlight})
  end
  itemStates[name] = {handle = handle, durability = durability, count = count, secondary = secondary, highlight = highlight}
end

local function refresh()
  if not interface.session().hasPlayer then return end
  local changed = player.actionBarView(view and view.revision)
  if changed then view, dirty = changed, true end
  if not view or not dirty then return end
  dirty = false
  local selected = view.selected
  widget.setVisible("customSelect", type(selected) == "number")
  widget.setVisible("essentialSelect", type(selected) == "string")
  local selectionWidget
  if type(selected) == "number" then
    selectionWidget = "customBar" .. selected .. "L"
  else
    for i, name in ipairs(essentialNames) do
      if name == selected then selectionWidget = "essentialBar" .. i end
    end
  end
  if selectionWidget then
    local p = widget.getPosition(selectionWidget)
    widget.setPosition(type(selected) == "number" and "customSelect" or "essentialSelect",
      {p[1] + style.actionBarSelectOffset[1], p[2] + style.actionBarSelectOffset[2]})
  end
  local swap = view.items[view.indexes * 2 + #essentialNames + 1]
  for i = 1, view.indexes do
    local primary, secondary = view.items[i * 2 - 1], view.items[i * 2]
    local primaryPreview, secondaryPreview = false, false
    if swap.handle and hover and hover.index == i then
      if hover.primary or swap.twoHanded then
        if not primary.handle and swap.identity == secondary.identity then secondary = {} end
        primary, primaryPreview = swap, true
      else
        if primary.twoHanded then primary = {} end
        if not secondary.handle and swap.identity == primary.identity then primary = {} end
        secondary, secondaryPreview = swap, true
      end
    end
    local left, right = "customBar" .. i .. "L", "customBar" .. i .. "R"
    setSlot(left, primary, not primaryPreview, not primaryPreview, false, not primary.handle and swap.handle ~= nil)
    setOverlay(left .. "Overlay", primaryPreview)
    if primary.twoHanded then
      setSlot(right, primary, false, false, true, false)
      setOverlay(right .. "Overlay", not primary.secondaryDrawables)
    else
      setSlot(right, secondary, not secondaryPreview, not secondaryPreview, false, not secondary.handle and swap.handle ~= nil)
      setOverlay(right .. "Overlay", secondaryPreview)
    end
  end
  for i = 1, #essentialNames do
    -- Essential slots keep their authored flags; only the real item changes.
    local item = view.items[view.indexes * 2 + i]
    widget.setItemSlotHandle("essentialBar" .. i, item.handle)
  end
  widget.setChecked("swapCustomBar", view.group ~= 1)
end

function init()
  style = config.getParameter("actionBar")
  view, hover, dirty = nil, nil, true
  itemStates, overlays = {}, {}
  local layout = config.getParameter("gui")
  if config.getParameter("bottomActionBar", false) then
    local height = config.getParameter("actionBarBodyHeight")
    for name, member in pairs(layout) do
      if member.type ~= "background" and member.type ~= "panefeature" then
        local p = member.position or {0, 0}
        widget.setPosition(name, {p[1], height - p[2] - widget.getSize(name)[2] + 1})
      end
    end
  end
  for name in pairs(layout) do
    if name:match("^customBar%d+[LR]$") then
      widget.setItemSlotDisplay(name, {countPosition = {position = style.countMidAnchor, horizontalAnchor = "mid"}})
    end
  end
  refresh()
end

function update()
  refresh()
  local pickup = root.getConfigurationPath("inventory.pickupToActionBar")
  if pickup ~= storage.pickup then
    storage.pickup = pickup
    widget.setChecked("pickupToActionBar", pickup)
  end
end

function customClick(name)
  local index, hand = name:match("^customBar(%d+)([LR])$")
  player.linkActionBarSwap(tonumber(index), hand == "L")
  refresh()
end

function customRight(name)
  if not interface.paneDisplayed("Inventory") then return end
  local index, hand = name:match("^customBar(%d+)([LR])$")
  player.clearActionBarLink(tonumber(index), hand == "L")
  refresh()
end

function essentialClick(name)
  player.setSelectedActionBarSlot(essentialNames[tonumber(name:match("(%d+)$"))])
  refresh()
end

function pickupToActionBar()
  root.setConfigurationPath("inventory.pickupToActionBar", widget.getChecked("pickupToActionBar"))
end

function swapCustomBar()
  local group, groups = player.actionBarGroup()
  player.setActionBarGroup(group % groups + 1)
  refresh()
end

function onInput(event)
  if not interface.session().hasPlayer then return false end
  refresh()
  if event.type == "move" then
    local nextHover
    for i = 1, view.indexes do
      if within("customBar" .. i .. "L", event.screenPosition) then nextHover = {index = i, primary = true}
      elseif within("customBar" .. i .. "R", event.screenPosition) then nextHover = {index = i, primary = false} end
    end
    if (hover and hover.index) ~= (nextHover and nextHover.index)
        or (hover and hover.primary) ~= (nextHover and nextHover.primary) then
      hover, dirty = nextHover, true
      refresh()
    end
  elseif event.type == "wheel" then
    local count, half = view.indexes + #essentialNames, math.floor(view.indexes / 2)
    local index
    if view.selected == nil then
      index = event.direction < 0 and 0 or count - 1
    else
      if type(view.selected) == "number" then
        index = view.selected - 1
        if index >= half then index = index + #essentialNames end
      else
        for i, name in ipairs(essentialNames) do
          if name == view.selected then index = half + i - 1 end
        end
      end
      index = (index - event.direction) % count
    end
    if index < half then player.setSelectedActionBarSlot(index + 1)
    elseif index < half + #essentialNames then player.setSelectedActionBarSlot(essentialNames[index - half + 1])
    else player.setSelectedActionBarSlot(index - #essentialNames + 1) end
    pane.playSound(style.sounds.switch[math.random(#style.sounds.switch)])
    refresh()
    return true
  elseif event.type == "keyDown" then
    for _, action in ipairs(event.actions) do
      local custom = tonumber(action:match("^InterfaceBar(%d+)$"))
      local essential = tonumber(action:match("^EssentialBar(%d+)$"))
      if custom and custom <= view.indexes then player.setSelectedActionBarSlot(custom)
      elseif essential then player.setSelectedActionBarSlot(essentialNames[essential])
      elseif action == "InterfaceDeselectHands" then
        local selected = player.selectedActionBarSlot()
        if selected ~= nil then
          storage.emptyHandsPrevious = selected
          player.setSelectedActionBarSlot(nil)
        else
          player.setSelectedActionBarSlot(storage.emptyHandsPrevious)
          storage.emptyHandsPrevious = nil
        end
      elseif action == "InterfaceChangeBarGroup" then swapCustomBar() end
    end
    refresh()
  end
  -- Native slots/buttons still own click hit-testing and capture. Keyboard
  -- actions intentionally do not consume the event, matching the original bar.
  return false
end

function cursorOverride()
  if hover and view and view.items[view.indexes * 2 + #essentialNames + 1].handle then return style.linkCursor end
end

function tooltipItem(screenPosition)
  if not view then return end
  local item
  for i = 1, view.indexes do
    for _, hand in ipairs({"L", "R"}) do
      local name = "customBar" .. i .. hand
      if within(name, screenPosition) then item = itemStates[name].handle end
    end
  end
  for i = 1, #essentialNames do
    if within("essentialBar" .. i, screenPosition) then item = view.items[view.indexes * 2 + i].handle end
  end
  return item
end
