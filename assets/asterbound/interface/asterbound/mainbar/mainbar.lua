require "/interface/asterbound/mainbar/geometry.lua"
require "/interface/asterbound/ui/geometry.lua"

local canvas, config, tooltips, screen, scale, geometry, previousTooltip
local paneIds = {
  inventory = "Inventory", craft = "CraftingPlain", codex = "codex",
  mmUpgrade = "MmUpgrade", collections = "Collections", questLog = "QuestLog"
}
local tooltipKeys = {
  inventory = "inventoryText", craft = "craftingText", codex = "codexText",
  mmUpgrade = "mmUpgradeText", collections = "collectionsText", questLog = "questsText"
}
local clickOrder = {"inventory", "craft", "codex", "deploy", "beam", "questLog", "mmUpgrade", "collections"}

local float = UiGeometry.float32

local function layout()
  local size, nextScale = camera.screenSize(), interface.scale()
  if screen and screen[1] == size[1] and screen[2] == size[2] and scale == nextScale then return end
  screen, scale = size, nextScale
  geometry = mainbarPresentation.layout(config, screen, scale)
  pane.setSize(config.size)
  widget.setSize("bar", config.size)
end

function init()
  canvas = widget.bindCanvas("bar")
  config = root.assetJson("/interface.config:mainBar")
  tooltips = root.assetJson("/interface.config:cursorTooltip")
  screen, previousTooltip = nil, false
  layout()
end

function update(dt)
  layout()
end

function render()
  layout()
  canvas:clear()
  local cursor = interface.cursorView()
  -- Physical Canvas drawing still adds the raw, unscaled Widget screen offset.
  local offset = pane.getScreenPosition()
  local tooltip
  local function hovered(name)
    return mainbarPresentation.contains(geometry.polygons[name], cursor.position)
  end
  local function draw(name, image)
    local position = geometry.positions[name]
    canvas:drawImage(image, {float(position[1] - offset[1]), float(position[2] - offset[2])}, scale)
  end
  local function stateButton(name, disabled)
    local button = config[name]
    local over = hovered(name)
    local open = interface.paneDisplayed(paneIds[name])
    local image
    if disabled then image = button.disabled
    elseif open then image = over and button.openHover or button.open
    elseif name == "inventory" and interface.inventoryChanged() then image = over and button.glowHover or button.glow
    else image = over and button.hover or button.base end
    draw(name, image)
    if over then tooltip = tooltips[disabled and "disabledText" or tooltipKeys[name]] end
  end
  stateButton("inventory")
  stateButton("craft")
  stateButton("codex")
  local hasManipulator = interface.session().hasPlayer and player.essentialItem("beamaxe") ~= nil
  stateButton("mmUpgrade", not hasManipulator)
  stateButton("collections")
  local travel = interface.travelState()
  if travel.beamUp or travel.deploy then
    local over = hovered("deploy")
    local image = travel.beamUp and config.beamUp or config.deploy
    draw("deploy", over and image.hover or image.base)
    if over then tooltip = tooltips[travel.beamUp and "beamUpText" or "deployText"] end
  else
    draw("deploy", config.deploy.disabled)
  end
  if travel.beamDown then
    local over = hovered("beam")
    draw("beam", over and config.beam.hover or config.beam.base)
    if over then tooltip = tooltips.beamDownText end
  end
  stateButton("questLog")
  if tooltip or previousTooltip ~= nil then
    interface.callPane("MainBarTooltip", "setTooltip", {tooltip and {text = tooltip, cursor = cursor, scale = scale} or nil})
  end
  previousTooltip = tooltip
end

function onInput(event)
  if event.type ~= "buttonDown" or event.button < 0 or event.button > 2 then return false end
  layout()
  for _, name in ipairs(clickOrder) do
    if mainbarPresentation.contains(geometry.polygons[name], event.physicalPosition) then
      if name == "deploy" then
        local travel = interface.travelState()
        if travel.deploy then interface.travel("deploy")
        elseif travel.beamUp then interface.travel("beamUp") end
      elseif name == "beam" then
        interface.travel("beamDown")
      elseif name ~= "mmUpgrade" or (interface.session().hasPlayer and player.essentialItem("beamaxe") ~= nil) then
        interface.togglePane(paneIds[name])
      end
      return true
    end
  end
  return false
end
