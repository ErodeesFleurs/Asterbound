local songPathPrefix = "/songs/"
local songList = "songs.list"
local files, lastSearch

-- String::CaseInsensitive folds ASCII only. Keep UTF-8 bytes and literal
-- search metacharacters unchanged, just as the original native songbook did.
local function lower(text)
  return (text:gsub("[A-Z]", string.lower))
end

local function scanSongs()
  local files = {}
  for _, path in ipairs(root.assetsByExtension(".abc")) do
    if lower(path:sub(1, #songPathPrefix)) == songPathPrefix then
      files[#files + 1] = {path = path, name = path:sub(#songPathPrefix + 1, -5), key = lower(path)}
    else
      sb.logWarn("Song '%s' isn't in %s, ignoring", path, songPathPrefix)
    end
  end
  table.sort(files, function(a, b) return a.key < b.key end)
  return files
end

local function highlighted(name, query)
  if query == "" then return name end
  local folded = lower(name)
  local first, last = folded:find(query, 1, true)
  if not first then return nil end
  local text, cursor = {}, 1
  repeat
    text[#text + 1] = "^#bbb;" .. name:sub(cursor, first - 1) .. "^#7f7;" .. name:sub(first, last)
    cursor = last + 1
    first, last = folded:find(query, cursor, true)
  until not first
  text[#text + 1] = "^#bbb;" .. name:sub(cursor)
  return table.concat(text)
end

local function refresh(force)
  local searchText = widget.getText("search")
  if not force and searchText == lastSearch then return end
  lastSearch = searchText
  local query = lower(searchText)
  widget.clearListItems(songList)
  for _, file in ipairs(files) do
    local text = highlighted(file.name, query)
    if text then
      local row = songList .. "." .. widget.addListItem(songList)
      -- A filtered row carries the original resource identity, never its
      -- position in either the full catalog or the visible list.
      widget.setData(row, file.path)
      widget.setText(row .. ".songName", text)
      widget.setVisible(row, true)
    end
  end
end

function init()
  files = scanSongs()
  local paths = {}
  for index, file in ipairs(files) do paths[index] = file.path end
  local previous = storage.songPaths
  local changed = not previous or #paths ~= #previous
  if not changed then
    for index, path in ipairs(paths) do
      if path ~= previous[index] then changed = true break end
    end
  end
  -- Only catalog identity crosses sessions; derived names/keys and filter
  -- caches stay local. Native widgets already retain text and selection.
  storage.songPaths = paths
  storage.files, storage.lastSearch = nil, nil
  lastSearch = widget.getText("search")
  -- Dismiss/reopen keeps the actual widgets (including band and selection).
  -- A live script/asset reload rebuilds the catalog without playing anything.
  refresh(changed or not storage.dismissed)
  storage.dismissed = false
end

function dismissed()
  storage.dismissed = true
end

function update()
  refresh(false)
end

function search()
  refresh(false)
end

function group()
end

function btnPlay()
  local selected = widget.getListSelected(songList)
  if not selected then return end
  local path = widget.getData(songList .. "." .. selected)
  songbook.play({resource = path, abc = root.assetData(path)}, widget.getText("group"))
  pane.dismiss()
end
