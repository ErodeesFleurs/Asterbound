-- Loaded last, only for the fingerprinted Alpha 0.4.0 / Enhancements 1.4.1 pair.
-- A work unit is a terrain candidate or one terrain-widget mutation. Leave two
-- units for hiding/showing the terrain layer when a generation is invalidated.
local WORK_BUDGET = 62
local VIEWPORT_SHARE = 32
local originalInit = init
local originalUninit = uninit
local tiles, byKey = {}, {}
local request
local sweep = 1
local scan = 0
local purge = false
local invalid = true
local hidden = false
local closed = false
local suspended = false
local refreshBatch
local refreshIndex = 0

local function hideTerrain()
  if tileList and not hidden then
    widget.setVisible(tileList, false)
    hidden = true
  end
end

local function invalidate()
  invalid = true
  purge = true
  hideTerrain()
end

local function contains(view, x, y)
  return y >= view.firstY and y <= view.lastY
    and (x - view.firstX) % view.worldXChunk < view.countX
end

local function syncRequest()
  local sourceWorld = player.worldId()
  local sourceServer = player.serverUuid()
  local centerX = math.floor(dragOffset[1] / chunkSize[1] * zoomLevel) % worldXChunk
  local centerY = math.floor(dragOffset[2] / chunkSize[2] * zoomLevel)
  local sameWorld = request and request.sourceWorld == sourceWorld
    and request.sourceServer == sourceServer
  local sameGeneration = sameWorld and request.worldId == worldId
    and request.serverUuid == serverUuid and request.width == worldSize[1]
    and request.height == worldSize[2] and request.zoom == zoomLevel
    and request.chunkWidth == chunkSize[1] and request.chunkHeight == chunkSize[2]
    and request.worldXChunk == worldXChunk
  if sameGeneration and not invalid
    and request.centerX == centerX and request.centerY == centerY then return end

  if request and not sameWorld then
    -- Notifications have no world tag. Discard the old world's batch and cache;
    -- authoritative properties will supply any just-completed new-world tile.
    refreshBatch = nil
    refreshIndex = 0
    player.setProperty("MM_bigMap_refreshList", jarray())
    osbMMChunkStoreWorld = nil
  end
  if not sameGeneration then
    purge = true
    hideTerrain()
  end

  local radiusX = math.ceil(14 * zoomLevel)
  local radiusY = math.ceil(12 * zoomLevel)
  request = {
    sourceWorld = sourceWorld, sourceServer = sourceServer,
    worldId = worldId, serverUuid = serverUuid,
    width = worldSize[1], height = worldSize[2], zoom = zoomLevel,
    chunkWidth = chunkSize[1], chunkHeight = chunkSize[2], worldXChunk = worldXChunk,
    centerX = centerX, centerY = centerY,
    firstX = (centerX - radiusX) % worldXChunk,
    countX = math.min(2 * radiusX + 1, worldXChunk),
    firstY = centerY - radiusY, lastY = centerY + radiusY,
    countY = 2 * radiusY + 1
  }
  sweep = 1
  scan = 0
  invalid = false
end

local function removeTile(index)
  local tile = tiles[index]
  widget.removeChild(tileList, tile.key .. "a")
  widget.removeChild(tileList, tile.key .. "b")
  byKey[tile.key] = nil
  tiles[index] = tiles[#tiles]
  tiles[#tiles] = nil
end

local function showTile(x, y, image)
  local key = x .. "_" .. y
  local tile = byKey[key]
  if tile then
    if tile.image ~= image then
      widget.setImage(tileList .. "." .. key .. "a", image)
      widget.setImage(tileList .. "." .. key .. "b", image)
      tile.image = image
      return 2
    end
    return 0
  end

  local px = math.floor(x * request.chunkWidth / request.zoom)
  local py = math.floor(y * request.chunkHeight / request.zoom)
  widget.addChild(tileList, {
    type = "image", zlevel = 12, position = {px, py}, file = image,
    scale = 1 / request.zoom
  }, key .. "a")
  widget.addChild(tileList, {
    type = "image", zlevel = 12,
    position = {px + request.width / request.zoom, py}, file = image,
    scale = 1 / request.zoom
  }, key .. "b")
  local swept = sweep > #tiles
  tile = {x = x, y = y, key = key, image = image}
  tiles[#tiles + 1] = tile
  byKey[key] = tile
  if swept then sweep = #tiles + 1 end
  return 2
end

local function advanceViewport(budget)
  while budget >= 3 do
    if sweep <= #tiles then
      local tile = tiles[sweep]
      budget = budget - 1
      if purge or not contains(request, tile.x, tile.y) then
        removeTile(sweep)
        budget = budget - 2
      else
        sweep = sweep + 1
      end
    elseif suspended or not request then
      break
    else
      purge = false
      if hidden then
        widget.setVisible(tileList, true)
        hidden = false
      end
      if scan >= request.countX * request.countY then break end
      local x = (request.firstX + math.floor(scan / request.countY)) % request.worldXChunk
      local y = request.firstY + scan % request.countY
      scan = scan + 1
      budget = budget - 1
      if not byKey[x .. "_" .. y] then
        local image = osbMMReadChunkImage(x, y)
        if image and image ~= "" then budget = budget - showTile(x, y, image) end
      end
    end
  end
  return budget
end

local function takeRefreshBatch()
  if refreshBatch then return end
  local batch = player.getProperty("MM_bigMap_refreshList") or jarray()
  if #batch == 0 then return end
  -- Transfer once, not a full property copy on every tick of a large batch.
  -- New scanner notifications accumulate separately while this batch drains.
  player.setProperty("MM_bigMap_refreshList", jarray())
  refreshBatch = batch
  refreshIndex = #batch
end

local function advanceRefresh(budget)
  -- Keep images in the transferred batch until they can be displayed; the
  -- enhancement's bounded cache alone need not retain a full large viewport.
  while refreshBatch and not purge and sweep > #tiles and budget >= 3 do
    local item = refreshBatch[refreshIndex]
    refreshBatch[refreshIndex] = nil
    refreshIndex = refreshIndex - 1
    budget = budget - 1
    local position = type(item) == "table" and item[1] or nil
    if type(position) == "table" then
      local x = math.floor(tonumber(position[1]) or 0) % request.worldXChunk
      local y = math.floor(tonumber(position[2]) or 0)
      -- Preserve the enhancement's newest-completed-image preference.
      local image = osbMMSafeChunkImage(item[2])
      if not image or image == "" then
        image = osbMMSafeChunkImage(osbMMReadChunkImageFresh(x, y))
      end
      if image and image ~= "" then
        osbMMCacheChunkImage(x, y, image)
        if contains(request, x, y) then
          budget = budget - showTile(x, y, image)
        end
      end
    end
    if refreshIndex == 0 then refreshBatch = nil end
  end
end

function init()
  closed = false
  -- The viewport reads authoritative terrain, so reopening need not replay
  -- notifications retained from a dismissed pane or a previously visited world.
  player.setProperty("MM_bigMap_refreshList", jarray())
  originalInit()
end

function updateTile(_xd, _yd)
  if not closed then syncRequest() end
end

function rebuildMapLayers()
  if closed then return end
  invalidate()
  dragChunk = chunkPosition(dragOffset)
  dragChunkDif = dragChunk
end

function clearList()
  widget.removeAllChildren(entityList)
  if osbMMWaypointClearWidgets then osbMMWaypointClearWidgets() end
  if osbMMResetMapRenderer then osbMMResetMapRenderer() end
  dragChunkDif = {10000, 10000}
  entityTable = {}
  invalidate()
end

function selectWorld(_widgetName, selected)
  worldId = selected.worldId
  serverUuid = selected.serverUuid
  worldSize = selected.worldSize
  dragOffset = vec2.div(selected.position, zoomLevel)
  widget.setChecked("fixedCamera", false)
  widget.setVisible("worldListFrame", false)
  worldXChunk = math.floor((worldSize[1] - 0.5) / 16) + 1
  rebuildMapLayers()
end

function refreshTile()
  if closed then return end
  if not playerExists then
    player.setProperty("MM_bigMap_refresh", false)
    if not suspended then
      suspended = true
      invalidate()
    end
    advanceViewport(WORK_BUDGET)
    return
  end
  suspended = false
  syncRequest()
  player.setProperty("MM_bigMap_refresh", worldId == player.worldId())
  takeRefreshBatch()
  -- Scanner traffic cannot starve the implicit viewport cursor, and a large
  -- viewport cannot starve live terrain notifications either.
  local share = refreshBatch and VIEWPORT_SHARE or WORK_BUDGET
  local remaining = advanceViewport(share)
  advanceRefresh(WORK_BUDGET - share + remaining)
end

function uninit()
  closed = true
  refreshBatch = nil
  refreshIndex = 0
  hideTerrain()
  -- The dismissed pane owns its remaining children. Do not synchronously walk
  -- thousands of widgets here, and never resume a retired viewport afterward.
  originalUninit()
end
