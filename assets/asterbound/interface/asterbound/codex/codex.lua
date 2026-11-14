local list = "scrollArea.bookList"
local books, rows, speciesOptions, selectText
local refreshing = false

local function content(id)
  if not id then return nil end
  if books[id] == nil then books[id] = player.getCodex(id) or false end
  return books[id] or nil
end

local function pageText()
  local book = content(storage.id)
  local count = book and #book.contentPages or 0
  storage.page = math.max(1, math.min(storage.page or 1, math.max(1, count)))
  widget.setText("pageText", book and (book.contentPages[storage.page] or "") or "")
  widget.setText("titleLabel", book and book.title or "")
  widget.setVisible("pageLabel", book ~= nil)
  widget.setText("pageNum", book and ((count == 0 and 0 or storage.page) .. " of " .. count) or "")
  widget.setButtonEnabled("prevButton", book ~= nil and storage.page > 1)
  widget.setButtonEnabled("nextButton", book ~= nil and storage.page < count)
end

local function refresh(force)
  local entries = player.getCodexList()
  local visible = {}
  for _, entry in ipairs(entries) do
    local book = content(entry.id)
    if book and book.species == storage.species then visible[#visible + 1] = entry end
  end
  local changed = force or not rows or #rows ~= #visible
  if not changed then
    for index, entry in ipairs(visible) do
      if entry.id ~= rows[index].id or entry.read ~= rows[index].read then changed = true break end
    end
  end
  if not changed then return end
  refreshing = true
  widget.clearListItems(list)
  rows = visible
  for _, entry in ipairs(rows) do
    local name = widget.addListItem(list)
    local row = list .. "." .. name
    local book = content(entry.id)
    widget.setData(row, entry.id)
    widget.setText(row .. ".bookName", book.title)
    widget.setImage(row .. ".bookIcon", book.icon)
    if storage.selectedId == entry.id then widget.setListSelected(list, name) end
  end
  refreshing = false
end

function updateSpecies()
  if refreshing then return end
  local selected = widget.getSelectedData("speciesTabs")
  local species = selected and selected.species or "other"
  if storage.species ~= species then
    storage.species, storage.id, storage.selectedId, storage.page = species, nil, nil, 1
    pageText()
  end
  local text = selectText:gsub("<species>", function() return species end):lower()
  widget.setText("selectLabel", (text:gsub("%a+", function(word)
    return word:sub(1, 1):upper() .. word:sub(2)
  end)))
  refresh(false)
end

function init()
  books, rows, speciesOptions = {}, nil, {}
  local config = root.assetJson("/interface/windowconfig/codex.config")
  selectText = config.selectText
  -- Match ButtonGroup's auto-ID assignment while respecting modded explicit IDs.
  local used, highest = {}, nil
  for _, button in ipairs(config.paneLayout.speciesTabs.buttons) do
    local id = button.id or highest or -1
    while used[id] do id = id + 1 end
    used[id], highest = true, math.max(highest or id, id)
    if button.data then speciesOptions[button.data.species] = id end
  end
  updateSpecies()
  pageText()
end

function update()
  refresh(false)
end

function showCodex(id)
  if not player.isCodexKnown(id) then return false end
  local book = content(id)
  if not book then return false end
  local option = speciesOptions[book.species]
  if option ~= nil then widget.setSelectedOption("speciesTabs", option) end
  updateSpecies()
  storage.id, storage.selectedId, storage.page = id, id, 1
  player.markCodexRead(id)
  refresh(true)
  pageText()
  return true
end

function selectCodex()
  if refreshing then return end
  local selected = widget.getListSelected(list)
  if selected then showCodex(widget.getData(list .. "." .. selected)) end
end

function prevButton()
  storage.page = (storage.page or 1) - 1
  pageText()
end

function nextButton()
  storage.page = (storage.page or 1) + 1
  pageText()
end
