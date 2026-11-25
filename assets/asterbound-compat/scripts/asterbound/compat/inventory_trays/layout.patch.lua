local trays = "/scripts/asterbound/compat/inventory_trays/"
local variants = {
  ["/interface/inventory/inventoryfooter.png"] = {
    footer = "redone.png", pitch = 22, firstX = 23, legs = {96,78}, chest = {18,78}, head = {57,63}
  },
  ["/interface/mwh_arcanaBk3kInventory/inventoryfooter.png"] = {
    footer = "arcana.png", pitch = 20, firstX = 12, legs = {36,182}, chest = {36,201}, head = {36,220}
  }
}

function patch(data)
  local layout = data.paneLayout
  local variant = layout and layout.background and variants[layout.background.fileFooter]
  if not variant or not assets.exists(trays .. variant.footer) then return end
  if variant.footer == "arcana.png" and not data.mwh_arcanaBk3kInventory then return end
  local slots = {}
  for index, part in ipairs({"legs", "chest", "head"}) do
    local base = layout[part .. "Cosmetic"]
    local original = variant[part]
    local position = base and base.position
    if not position or position[1] ~= original[1] or position[2] ~= original[2] then return end
    for column = 1, 4 do
      local slot = layout["cosmetic" .. ((index - 1) * 4 + column)]
      local p = slot and slot.position
      if not p or slot.type ~= "itemslot" or slot.backingImage ~= base.backingImage
          or p[1] ~= original[1] + column * 20 or p[2] ~= original[2] then return end
      slots[#slots + 1] = slot
    end
  end

  -- Growing the footer moves the header and body upward by 24 UI pixels.
  -- Move the original controls with them, leaving nested coordinates intact.
  for _, widget in pairs(layout) do
    if widget.type ~= "background" and widget.type ~= "panefeature" and widget.position then
      widget.position[2] = widget.position[2] + 24
    end
  end
  if variant.footer == "redone.png" and layout.close then
    for field, image in pairs({base = "x.png", hover = "xhover.png", pressed = "xpress.png"}) do
      local old, live = "/interface/inventory/" .. image, "/interface/" .. image
      if layout.close[field] == old and not assets.exists(old) and assets.exists(live) then
        layout.close[field] = live
      end
    end
  end
  for index, slot in ipairs(slots) do
    slot.position = {variant.firstX + (index - 1) * variant.pitch, 3}
  end
  layout.background.fileFooter = trays .. variant.footer
  -- The footer is not adjacent to the original vanity hover targets.
  data.alwaysDisplayCosmetics = true
  return data
end
