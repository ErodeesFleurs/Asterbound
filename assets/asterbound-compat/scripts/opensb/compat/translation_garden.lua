-- Galactic Almanach 1.1 garden.biome.patch uses stale placement indices.
-- Its biome reader queries originBiome on spawned objects, hence parameters
-- (not objectSets.originBiome). Keep all generation data and unrelated fields.
function patch(data)
  data.friendlyName = "Lush"
  for _, layer in ipairs({"surfacePlaceables", "undergroundPlaceables"}) do
    local placeables = data[layer]
    if type(placeables) == "table" then
      for _, item in ipairs(placeables.items or {}) do
        if item.type == "object" then
          for _, objectSet in ipairs(item.objectSets or {}) do
            if objectSet.parameters == nil then objectSet.parameters = {} end
            if type(objectSet.parameters) == "table" then
              objectSet.parameters.originBiome = "surface/garden"
            end
          end
        end
      end
    end
  end
  return data
end
