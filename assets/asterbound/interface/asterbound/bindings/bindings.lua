local LIST = "categories.list"
local ROWS = "binds.rows"
local gameCategories = {"player", "toolbar", "game"}

-- KeyDown supplies the native KeyMod bitset. Lock states are not chord
-- modifiers. The game and .binds encoders spell the GUI modifiers differently.
local modifiers = {
  {"LShift", 0x0001}, {"RShift", 0x0002},
  {"LCtrl",  0x0040}, {"RCtrl",  0x0080},
  {"LAlt",   0x0100}, {"RAlt",   0x0200},
  {"LGui",   0x0400, "LMeta"}, {"RGui", 0x0800, "RMeta"},
  {"AltGr",  0x4000}
}
local modBits = {}
for _, mod in ipairs(modifiers) do modBits[mod[1]] = mod[2] end

local categories, category = {}, nil
local rowTemplate, groupTemplate
local openingGame, currentGame, defaultGame, maxBindings
local gameChanged = false
local capture

local function rowPath(index)
  return ROWS .. ".row" .. index
end

local function getBinds(id)
  if category.modId then return input.getBinds(category.modId, id) end
  return currentGame[id] or jarray()
end

local function describe(binds)
  local descriptions = {}
  for _, bind in ipairs(binds) do
    local value = tostring(bind.value)
    if bind.type == "controller" then
      value = string.format("Pad %d: %s", bind.controller or 0, value)
    end
    if bind.mods and #bind.mods > 0 then
      value = table.concat(bind.mods, " + ") .. " + " .. value
    end
    descriptions[#descriptions + 1] = value
  end
  return table.concat(descriptions, ", ")
end

local function refresh(id)
  for i, row in ipairs(category.rows) do
    if row.id and (not id or row.id == id) then
      widget.setText(rowPath(i) .. ".apply", describe(getBinds(row.id)))
    end
  end
end

local function setBinds(id, binds)
  if category.modId then
    input.setBinds(category.modId, id, binds)
  else
    currentGame[id] = binds
    gameBinds.set(currentGame)
    gameChanged = true
  end
  refresh(id)
end

local function endCapture()
  if capture then
    local index = capture.index
    capture = nil
    widget.setVisible("capture", false)
    widget.setText(rowPath(index) .. ".apply", describe(getBinds(category.rows[index].id)))
  end
end

function applyBind(_, index)
  endCapture()
  local held = 0
  for _, mod in ipairs(modifiers) do
    if input.keyHeld(mod[1]) then held = held | mod[2] end
  end
  capture = {index = index, mods = held}
  widget.setText(rowPath(index) .. ".apply", category.modId and "Press input..." or "Press key...")
  widget.setVisible("capture", true)
end

function eraseBind(_, index)
  endCapture()
  setBinds(category.rows[index].id, jarray())
end

function resetBind(_, index)
  endCapture()
  local id = category.rows[index].id
  if category.modId then
    input.resetBinds(category.modId, id)
    refresh(id)
  else
    setBinds(id, defaultGame[id] or jarray())
  end
end

function cancelGame()
  if gameChanged then gameBinds.set(openingGame) end
  pane.dismiss()
end

function resetGame()
  endCapture()
  currentGame = gameBinds.defaults()
  gameBinds.set(currentGame)
  gameChanged = true
  if not category.modId then refresh() end
end

local function finishBind(kind, value, controller)
  local id = category.rows[capture.index].id
  local newBind = {type = kind, value = value, controller = controller}
  if kind ~= "controller" then
    local names = jarray()
    for _, mod in ipairs(modifiers) do
      if (capture.mods & mod[2]) ~= 0 then
        names[#names + 1] = not category.modId and mod[3] or mod[1]
      end
    end
    if not category.modId or #names > 0 then newBind.mods = names end
  end
  endCapture()

  -- Never mutate a descriptor array from the opening snapshot or defaults.
  local previous = getBinds(id)
  local nextBinds = jarray()
  local duplicate = false
  for _, old in ipairs(previous) do
    if sb.jsonEqual(old, newBind) then duplicate = true; break end
  end
  if not duplicate then
    local first = category.modId and 1 or math.max(1, #previous - maxBindings + 2)
    for i = first, #previous do nextBinds[#nextBinds + 1] = previous[i] end
  end
  if category.modId or maxBindings > 0 then nextBinds[#nextBinds + 1] = newBind end
  setBinds(id, nextBinds)
end

function onInput(event)
  if not capture then return false end
  if event.type == "keyDown" then
    local cancel = false
    for _, action in ipairs(event.actions) do
      if not category.modId and action == "KeybindingClear" then
        eraseBind(nil, capture.index)
        return true
      end
      if action == "KeybindingCancel" then cancel = true end
    end
    if cancel then
      endCapture()
    else
      local bit = modBits[event.key]
      capture.mods = event.mods | (bit or 0)
      if not bit then finishBind("key", event.key) end
    end
    return true
  elseif event.type == "keyUp" then
    local bit = modBits[event.key]
    if bit and (capture.mods & bit) ~= 0 then
      capture.mods = capture.mods & ~bit
      finishBind("key", event.key)
    end
    return true
  elseif event.type == "buttonDown" and category.modId then
    local path = rowPath(capture.index)
    if event.button == 0 and (widget.inMember("categories", event.screenPosition)
        or widget.inMember("cancelGame", event.screenPosition) or widget.inMember("resetGame", event.screenPosition)
        or widget.inMember("close", event.screenPosition) or widget.inMember(path .. ".erase", event.screenPosition)
        or widget.inMember(path .. ".reset", event.screenPosition)) then
      endCapture()
      return false
    end
    finishBind("mouse", event.buttonName)
    return true
  elseif event.type == "controller" and category.modId and event.controllerButton then
    finishBind("controller", event.controllerButton, event.controller)
    return true
  end
  return event.type == "textInput"
end

local function addCategory(entry)
  local member = widget.addListItem(LIST)
  categories[member] = entry
  widget.setText(LIST .. "." .. member .. ".label", entry.name)
  return member
end

local function byName(a, b)
  return a.name < b.name or (a.name == b.name and a.id < b.id)
end

local function modRows(data)
  local groups, sorted = {}, {}
  for id, group in pairs(data.groups or {}) do
    groups[id] = {id = id, name = group.name or id, rows = {}}
  end
  groups.unsorted = groups.unsorted or {id = "unsorted", name = "Unsorted", rows = {}}
  for id, bind in pairs(data.binds or {}) do
    if type(bind) == "table" then
      local rows = (groups[bind.group] or groups.unsorted).rows
      rows[#rows + 1] = {id = id, name = bind.name or id}
    end
  end
  for _, group in pairs(groups) do sorted[#sorted + 1] = group end
  table.sort(sorted, byName)
  local rows = {}
  for _, group in ipairs(sorted) do
    if #group.rows > 0 then
      if #sorted > 1 then rows[#rows + 1] = {name = group.name} end
      table.sort(group.rows, byName)
      for _, row in ipairs(group.rows) do rows[#rows + 1] = row end
    end
  end
  return rows
end

local function addModCategories()
  local definitions, sorted = {}, {}
  for _, path in ipairs(root.assetsByExtension("binds")) do
    for id, data in pairs(root.assetJson(path)) do
      definitions[id] = definitions[id] and sb.jsonMerge(definitions[id], data) or data
    end
  end
  for id, data in pairs(definitions) do
    sorted[#sorted + 1] = {id = id, modId = id, name = data.name or id, banner = data.bannerName, rows = modRows(data)}
  end
  table.sort(sorted, byName)
  for _, entry in ipairs(sorted) do addCategory(entry) end
end

function selectCategory()
  local entry = categories[widget.getListSelected(LIST)]
  if not entry then return end
  endCapture()
  category = entry
  -- The scroll area preserves its distance from the top when its content resizes.
  widget.setScrollOffset("binds", widget.getMaxScrollPosition("binds"))
  widget.removeAllChildren(ROWS)
  widget.setSize(ROWS, {240, math.max(0, #entry.rows * 14 - 1)})
  for i, row in ipairs(entry.rows) do
    local template = row.id and rowTemplate or groupTemplate
    template.position = {0, (#entry.rows - i) * 14}
    template.children.label.value = row.name
    if row.id then
      template.children.apply.caption = describe(getBinds(row.id))
      template.children.apply.data = i
      template.children.erase.data = i
      template.children.reset.data = i
    end
    widget.addChild(ROWS, template, "row" .. i)
  end
  widget.setText("banner", entry.banner or entry.name)
  widget.setText("source", entry.modId and "Mod controls" or "Game controls")
end

function createTooltip(position)
  local target = widget.getChildAt(position)
  local index, button
  if target then index, button = target:match("binds%.rows%.row(%d+)%.(%a+)$") end
  local row = index and category.rows[tonumber(index)]
  if row and row.id then
    if button == "erase" then return "Clear " .. row.name end
    if button == "reset" then return "Restore default: " .. row.name end
    if button == "apply" then
      local keys = describe(getBinds(row.id))
      return row.name .. ": " .. (keys ~= "" and keys or "Unbound")
    end
  end
end

function init()
  widget.setVisible("capture", false)
  rowTemplate = config.getParameter("actionRow")
  groupTemplate = config.getParameter("groupRow")
  openingGame = gameBinds.get()
  defaultGame = gameBinds.defaults()
  currentGame = jobject()
  for id, binds in pairs(openingGame) do currentGame[id] = binds end

  local menu = root.assetJson("/interface/windowconfig/keybindingsmenu.config")
  maxBindings = menu.maxBindings
  local names = {player = "Player", toolbar = "Toolbar", game = "Game"}
  local tabs = menu.paneLayout and menu.paneLayout.categories
  for _, tab in ipairs(tabs and tabs.tabs or {}) do
    if tab.tabName and tab.tabTitle then names[tab.tabName] = tab.tabTitle end
  end

  widget.clearListItems(LIST)
  local first
  for _, id in ipairs(gameCategories) do
    local rows = {}
    for _, action in ipairs(menu.keyActions[id]) do
      rows[#rows + 1] = {id = action.action, name = action.label}
    end
    local member = addCategory({name = names[id], rows = rows})
    first = first or member
  end
  addModCategories()
  widget.setListSelected(LIST, first)
end

function uninit()
  widget.setVisible("capture", false)
  capture = nil
end