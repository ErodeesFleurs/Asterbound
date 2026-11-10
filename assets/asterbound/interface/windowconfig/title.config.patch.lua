function patch(data)
  for _, v in pairs(data.mainMenuButtons) do
    if not v.rightAnchored then
      v.offset[2] = v.offset[2] + (v.key == "quit" and -5 or 15)
    end
  end
  data.skyBackdropDarken = jarray{0, 0, 0, 64}
  data.backdropImages = jarray{
    jarray{
      jarray{0, 0},
      "/interface/title/starbound.png",
      0.5,
      jarray{0.5, 0.5}
    }
  }
  data.scripts = jarray{"/interface/title/title.lua"}
  return data
end
