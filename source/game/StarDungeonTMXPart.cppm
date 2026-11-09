module;
import std;
import star.string;
import star.list;
import star.format;
import star.json;
import star.rect;
import star.image;
import star.data_stream;
import star.bi_map;
import star.vector;
import star.game_types;
import star.material_types;
import star.random;
import star.set;
import star.thread;
import star.ordered_map;
import star.lru_cache;


import star.item_descriptor;
import star.world_geometry;
import star.dungeon_generator;

namespace Star {
class Root;
}

export module star.dungeon_tmx_part;
import star.tileset_database;

export namespace Star {

namespace Dungeon {
  class TMXMap;

  class TMXTilesets {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TMXTilesets>, StarException> create(Root& root, Json const& tmx);

    [[nodiscard]] std::expected<std::reference_wrapper<Tiled::Tile const>, StarException> getTile(unsigned gid, TileLayer layer) const;

    Tiled::Tile const& nullTile() const {
      return *m_nullTile;
    }

  private:
    TMXTilesets() = default;

    // The default empty background tile has clear=true.  (If you use the pink
    // tile in the background, clear will be false instead.) Analogous to
    // EmptyMaterialId.
    std::shared_ptr<Tiled::Tile const> m_emptyBackTile;
    // The default foreground tile doesn't have a 'clear' property.  Also
    // returned by tile layers when given coordinates outside the bounds of the
    // layer.  Analogous to the NullMaterialId that mission maps are initially
    // filled with.
    std::shared_ptr<Tiled::Tile const> m_nullTile;

    List<std::shared_ptr<Tiled::Tileset const>> m_tilesets;
    List<std::optional<std::reference_wrapper<Tiled::Tile const>>> m_foregroundTilesByGid;
    List<std::optional<std::reference_wrapper<Tiled::Tile const>>> m_backgroundTilesByGid;
  };

  class TMXTileLayer {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TMXTileLayer>, StarException> create(Json const& tmx);

    [[nodiscard]] std::expected<std::reference_wrapper<Tiled::Tile const>, StarException> getTile(std::shared_ptr<TMXTilesets> const& tilesets, Vec2I pos) const;

    unsigned width() const {
      return m_rect.xMax() - m_rect.xMin() + 1;
    }
    unsigned height() const {
      return m_rect.yMax() - m_rect.yMin() + 1;
    }

    RectI const& rect() const {
      return m_rect;
    }
    String const& name() const {
      return m_name;
    }

    TileLayer layer() const {
      return m_layer;
    }

    [[nodiscard]] std::expected<bool, StarException> forEachTile(TMXMap const& map, TileCallback const& callback) const;
    [[nodiscard]] std::expected<bool, StarException> forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const;

  private:
    TMXTileLayer() = default;
    RectI m_rect;
    String m_name;
    TileLayer m_layer;
    List<unsigned> m_tileData;
  };

  enum class ObjectKind {
    Tile,
    Rectangle,
    Ellipse,
    Polygon,
    Polyline,
    Stagehand
  };

  enum TileFlip {
    Horizontal = 0x80000000u,
    Vertical = 0x40000000u,
    Diagonal = 0x20000000u,
    AllBits = 0xe0000000u
  };

  class TMXObject {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TMXObject>, StarException> create(Root& root, std::optional<Json> const& groupProperties, Json const& tmx, std::shared_ptr<TMXTilesets> tilesets);

    Vec2I const& pos() const {
      return m_rect.min();
    }
    RectI const& rect() const {
      return m_rect;
    }
    Tiled::Tile const& tile() const {
      return *m_tile;
    }
    ObjectKind kind() const {
      return m_kind;
    }

    [[nodiscard]] std::expected<bool, StarException> forEachTile(TMXMap const& map, TileCallback const& callback) const;
    [[nodiscard]] std::expected<bool, StarException> forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const;

  private:
    TMXObject() = default;
    // "Tile Objects" in Tiled are objects that contain an image from a tileset,
    // and have a bunch of their own Tile Object-specific properties.
    struct TileObjectInfo {
      Tiled::Properties tileProperties;
      unsigned flipBits;
    };

    [[nodiscard]] static std::expected<Vec2I, StarException> getSize(Json const& tmx);
    [[nodiscard]] static std::expected<Vec2I, StarException> getImagePosition(Tiled::Properties const& properties);
    [[nodiscard]] static std::expected<ObjectKind, StarException> getObjectKind(Json const& tmx, std::optional<Json> const& objectProperties);
    [[nodiscard]] static std::expected<std::optional<TileObjectInfo>, StarException> getTileObjectInfo(Json const& tmx, std::shared_ptr<TMXTilesets> tilesets, TileLayer layer);
    [[nodiscard]] static std::expected<TileLayer, StarException> getLayer(std::optional<Json> const& groupProperties, std::optional<Json> const& objectProperties);

    [[nodiscard]] static std::expected<Vec2I, StarException> getPos(Json const& tmx);
    static StarException tmxObjectError(Vec2I position, String const& msg);

    RectI m_rect;
    std::shared_ptr<Tiled::Tile const> m_tile;
    TileLayer m_layer;
    ObjectKind m_kind;
    unsigned m_objectId;
    List<Vec2I> m_polyline;
  };

  class TMXObjectGroup {
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TMXObjectGroup>, StarException> create(Root& root, Json const& tmx, std::shared_ptr<TMXTilesets> tilesets);

    List<std::shared_ptr<TMXObject>> const& objects() const {
      return m_objects;
    }

    String name() const;

    [[nodiscard]] std::expected<bool, StarException> forEachTile(TMXMap const& map, TileCallback const& callback) const;
    [[nodiscard]] std::expected<bool, StarException> forEachTileAt(Vec2I pos, TMXMap const& map, TileCallback const& callback) const;

  private:
    TMXObjectGroup() = default;
    String m_name;
    List<std::shared_ptr<TMXObject>> m_objects;
  };

  class TMXMap {
  // Factories report malformed maps and unavailable tilesets without publishing
  // a partially initialized map.
  public:
    [[nodiscard]] static std::expected<std::shared_ptr<TMXMap>, StarException> create(Root& root, Json const& tmx);

    List<std::shared_ptr<TMXTileLayer>> const& tileLayers() const {
      return m_tileLayers;
    }
    List<std::shared_ptr<TMXObjectGroup>> const& objectGroups() const {
      return m_objectGroups;
    }
    std::shared_ptr<TMXTilesets> const& tilesets() const {
      return m_tilesets;
    }
    unsigned width() const {
      return m_width;
    }
    unsigned height() const {
      return m_height;
    }

    [[nodiscard]] std::expected<bool, StarException> forEachTile(TileCallback const& callback) const;
    [[nodiscard]] std::expected<bool, StarException> forEachTileAt(Vec2I pos, TileCallback const& callback) const;

  private:
    TMXMap() = default;
    List<std::shared_ptr<TMXTileLayer>> m_tileLayers;
    List<std::shared_ptr<TMXObjectGroup>> m_objectGroups;

    std::shared_ptr<TMXTilesets> m_tilesets;
    unsigned m_width, m_height;
  };

  class TMXPartReader : public PartReader {
    Root& m_root;
  public:
    explicit TMXPartReader(Root& root) : m_root(root) {}
    [[nodiscard]] std::expected<void, StarException> readAsset(String const& asset) override;

    Vec2U size() const override;

    [[nodiscard]] std::expected<void, StarException> forEachTile(TileCallback const& callback) const override;
    [[nodiscard]] std::expected<void, StarException> forEachTileAt(Vec2I pos,
                                                                   TileCallback const& callback) const override;

  private:

    List<std::pair<String, std::shared_ptr<TMXMap const>>> m_maps;
  };
}

}

