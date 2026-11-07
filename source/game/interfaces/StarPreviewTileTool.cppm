module;
import std;
import star.type_identity;
import star.list;
import star.format;


struct PreviewTile;

import star.entity_rendering_types;

namespace Star {

class PreviewTileTool {
public:
  virtual void const* queryType(TypeKey key) const noexcept {
    return queryTypeOf<PreviewTileTool>(this, key);
  }
  virtual std::string_view dynamicTypeName() const noexcept {
    return Star::typeName<PreviewTileTool>();
  }

  virtual ~PreviewTileTool() = default;
  [[nodiscard]]
  virtual std::expected<List<PreviewTile>, StarException> previewTiles(bool shifting) const = 0;
};

}

export module star.preview_tile_tool;

export namespace Star {
  using ::Star::PreviewTileTool;
}

export using ::PreviewTile;
