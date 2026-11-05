-- The Forge: Blade Dance 0.7.1 (Workshop 2052901374, author Ribs) embeds its
-- actual custom weapon art as image directives in these five recipes. Their
-- carrier PNG is global, not relative to the vanilla pistol/broadsword folder.
-- Prepend only the missing slash: retain every encoded pixel, directive, JSON
-- null/array type, whitespace byte, and unrelated recipe parameter unchanged.
-- https://steamcommunity.com/sharedfiles/filedetails/?id=2052901374
local recipes = {
  {"/recipes/books/zapbook.recipe", 23378, 0x3e3447603274b86d},
  {"/recipes/books/fridgebook.recipe", 16417, 0xc87c6a91ac32287e},
  {"/recipes/books/firebook.recipe", 14356, 0x4fba1ae73e4e2899},
  {"/recipes/books/poisonbook.recipe", 15774, 0xbc2f5b7bd2a38e27},
  {"/recipes/uniqueblades/pbd_dragonslayer.recipe", 68396, 0x68b8a86300984f1d}
}
for _, recipe in ipairs(recipes) do
  local source = assets.origin(recipe[1])
  local metadata = source and assets.sourceMetadata(source)
  if metadata and metadata.name == "BDR" and metadata.version == "0.7.1" then
    local bytes = assets.sourceBytes(source, recipe[1])
    local fingerprint = 0xcbf29ce484222325
    for i = 1, #bytes do
      fingerprint = (fingerprint ~ bytes:byte(i)) * 0x100000001b3
    end
    if #bytes == recipe[2] and fingerprint == recipe[3] then
      -- Each recipe has exactly two intended carriers: inventory icon plus
      -- held weapon part. Books' middle part has no directives on that field.
      local repaired, count = bytes:gsub('"assetmissing%.png', '"/assetmissing.png')
      if count == 2 then assets.add(recipe[1], repaired) end
    end
  end
end
