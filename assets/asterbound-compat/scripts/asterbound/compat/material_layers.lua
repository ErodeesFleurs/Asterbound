-- Normalize the effective sort key on its consumer, not on a shared .config
-- that may also supply data to unrelated assets.
local templateLayers, patches = {}, {}

local function unsignedLayer(value)
  if type(value) ~= "number" then return end
  local n = math.tointeger(value)
  if n and n >= -2147483648 and n < 0 then return n + 4294967296 end
end

local function inheritedLayer(template, path, extension)
  if type(template) == "table" then return unsignedLayer(template.zLevel) end
  if type(template) ~= "string" then return end
  if template:sub(1, 1) ~= "/" then
    -- MaterialDatabase resolves material templates beside the material, but
    -- relative matmod templates against the asset root.
    template = (extension == "material" and path:match("^.*/") or "/") .. template
  end
  if templateLayers[template] == nil then
    local ok, data = pcall(assets.json, template)
    templateLayers[template] = ok and type(data) == "table" and unsignedLayer(data.zLevel) or false
  end
  return templateLayers[template]
end

local function repair(path, extension)
  local ok, data = pcall(assets.json, path)
  if not ok or type(data) ~= "table" or not data.renderTemplate then return end
  local params = data.renderParameters
  if params ~= nil and type(params) ~= "table" then return end

  local layer
  if params and params.zLevel ~= nil then
    layer = unsignedLayer(params.zLevel)
  else
    layer = inheritedLayer(data.renderTemplate, path, extension)
    if not layer then return end
    -- Explicit null inherits the template; missing parameters or an array
    -- must still receive the native material diagnostic, not a new object.
    if params == nil then
      if sb.jsonQuery(data, "renderParameters", false) == false then return end
    elseif sb.printJson(params):sub(1, 1) ~= "{" then
      return
    end
  end
  if not layer then return end

  local patch = patches[layer]
  if not patch then
    patch = "/scripts/asterbound/compat/material_layers/" .. layer .. ".patch"
    assets.add(patch, {renderParameters = {zLevel = layer}})
    patches[layer] = patch
  end
  assert(assets.patch(path, patch))
  sb.logInfo("[Asterbound Compatibility] Normalized legacy zLevel on %s to %s", path, layer)
end

for _, extension in ipairs({"material", "matmod"}) do
  for _, path in ipairs(assets.byExtension(extension)) do repair(path, extension) end
end
