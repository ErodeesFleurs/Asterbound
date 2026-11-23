local list = "scrollArea.bookList"
local summaries, revision, domain, rows, book, speciesOptions, selectText
local refreshing = false

local function syncSummaries()
  local current, owner = player.codexRevision()
  if revision == current and domain == owner then return false, false end
  local replaced = domain ~= owner
  summaries = player.getCodexSummaries()
  revision, domain = current, owner
  return true, replaced
end

local function pageText()
  local count = book and #book.contentPages or 0
  storage.page = math.max(1, math.min(storage.page or 1, math.max(1, count)))
  widget.setText("pageText", book and (book.contentPages[storage.page] or "") or "")
  widget.setText("titleLabel", book and book.title or "")
  widget.setVisible("pageLabel", book ~= nil)
  widget.setText("pageNum", book and ((count == 0 and 0 or storage.page) .. " of " .. count) or "")
  widget.setButtonEnabled("prevButton", book ~= nil and storage.page > 1)
  widget.setButtonEnabled("nextButton", book ~= nil and storage.page < count)
end

local function selectRow()
  local name
  for _, entry in ipairs(rows) do
    if entry.id == storage.selectedId then name = entry.rowName break end
  end
  if widget.getListSelected(list) == name then return end
  refreshing = true
  if name then widget.setListSelected(list, name) else widget.clearListSelected(list) end
  refreshing = false
end

local function renderRows()
  local visible, changed = {}, not rows
  for _, entry in ipairs(summaries) do
    if entry.species == storage.species then
      visible[#visible + 1] = entry
      local old = rows and rows[#visible]
      if not old or old.id ~= entry.id or old.title ~= entry.title or old.icon ~= entry.icon then
        changed = true
      end
    end
  end
  if not changed and #visible == #rows then
    selectRow()
    return
  end
  rows = visible
  refreshing = true
  widget.clearListItems(list)
  for _, entry in ipairs(rows) do
    local name = widget.addListItem(list)
    entry.rowName = name
    local row = list .. "." .. name
    widget.setData(row, entry.id)
    widget.setText(row .. ".bookName", entry.title)
    widget.setImage(row .. ".bookIcon", entry.icon)
    if storage.selectedId == entry.id then widget.setListSelected(list, name) end
  end
  refreshing = false
end

local function setSpecies(species)
  storage.species = species
  local text = selectText:gsub("<species>", function() return species end):lower()
  widget.setText("selectLabel", (text:gsub("%a+", function(word)
    return word:sub(1, 1):upper() .. word:sub(2)
  end)))
end

local function selectedSpecies()
  local selected = widget.getSelectedData("speciesTabs")
  return selected and selected.species or "other"
end

local function chooseSpecies(species)
  local option = speciesOptions[species]
  if option ~= nil then
    refreshing = true
    widget.setSelectedOption("speciesTabs", option)
    refreshing = false
  end
  setSpecies(selectedSpecies())
end

function updateSpecies()
  if refreshing then return end
  local species = selectedSpecies()
  if storage.species == species then return end
  book, storage.id, storage.selectedId, storage.page = nil, nil, nil, 1
  setSpecies(species)
  syncSummaries()
  renderRows()
  pageText()
end

function init()
  summaries, revision, domain, rows, book, speciesOptions = nil, nil, nil, nil, nil, {}
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
  update()
end

function update()
  local changed, replaced = syncSummaries()
  if not changed then return end
  if replaced then
    -- diskLoad can replace the library on the very same player. Only restore
    -- the selected ID/page; never reuse the previous owner's parsed pages.
    book = storage.id and player.getCodex(storage.id)
    if not book then storage.id, storage.selectedId, storage.page = nil, nil, 1 end
    chooseSpecies(book and book.species or storage.species)
    pageText()
  end
  renderRows()
end

function showCodex(id)
  local _, owner = player.codexRevision()
  local selected = domain == owner and book and book.id == id and book or player.getCodex(id)
  if not selected then return false end

  local previousSpecies = storage.species
  if selected.species ~= previousSpecies then chooseSpecies(selected.species) end
  book, storage.id, storage.selectedId, storage.page = selected, id, id, 1
  player.markCodexRead(id)
  local changed = syncSummaries()
  if changed or previousSpecies ~= storage.species then
    renderRows()
  else
    selectRow()
  end
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
