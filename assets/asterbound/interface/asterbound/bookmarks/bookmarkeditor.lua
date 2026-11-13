-- Native bookmark serialization canonicalizes WorldId and SpawnTarget. Both
-- opening callers and player.teleportBookmarks supply that same representation;
-- target alone is Bookmark<T>'s identity, not its display name or icon.
local function savedBookmark(bookmark)
  for _, existing in ipairs(player.teleportBookmarks()) do
    if sb.jsonEqual(existing.target, bookmark.target) then return existing end
  end
end

local function validContext()
  local edit = storage.edit
  if not edit then return false end
  local session = interface.session()
  if not session.hasPlayer or not session.inWorld
      or session.playerGeneration ~= edit.session.playerGeneration
      or session.worldGeneration ~= edit.session.worldGeneration then return false end
  -- Only the local interaction source must remain valid. A stored destination
  -- may be in an entirely different, unloaded world.
  if not world.entityExists(edit.sourceEntityId) then return false end
  return not edit.sourceUniqueId or world.entityUniqueId(edit.sourceEntityId) == edit.sourceUniqueId
end

local function displayBookmark()
  local edit = storage.edit
  widget.setText("lblTitle", edit.isNew and "NEW BOOKMARK" or "EDIT BOOKMARK")
  widget.setVisible("remove", not edit.isNew)
  widget.setImage("imgIcon", "/interface/bookmarks/icons/" .. edit.bookmark.icon .. ".png")
  widget.setText("lblPlanetName", edit.bookmark.targetName)
  widget.setText("name", edit.draft)
  widget.focus("name")
end

function init()
  if storage.edit then
    if validContext() then displayBookmark() else close() end
  end
end

function editBookmark(payload)
  local bookmark = payload.bookmark
  local existing = savedBookmark(bookmark)
  storage.edit = {
    bookmark = bookmark,
    draft = existing and existing.bookmarkName or bookmark.bookmarkName,
    isNew = existing == nil,
    sourceEntityId = payload.sourceEntityId,
    sourceUniqueId = payload.sourceUniqueId,
    session = interface.session()
  }
  if validContext() then displayBookmark() else close() end
end

function name()
  if storage.edit then storage.edit.draft = widget.getText("name") end
end

function ok()
  if not validContext() then close() return end
  local edit = storage.edit
  -- A destination invalidated while the editor was open must not be revived.
  if not edit.isNew and not savedBookmark(edit.bookmark) then close() return end
  local bookmark = edit.bookmark
  bookmark.bookmarkName = widget.getText("name")
  if bookmark.bookmarkName == "" then bookmark.bookmarkName = bookmark.targetName end
  if not edit.isNew then player.removeTeleportBookmark(bookmark) end
  player.addTeleportBookmark(bookmark)
  close()
end

function remove()
  if validContext() and not storage.edit.isNew then
    player.removeTeleportBookmark(storage.edit.bookmark)
  end
  close()
end

function close()
  storage.edit = nil
  pane.dismiss()
end

function update()
  if storage.edit and not validContext() then close() end
end
