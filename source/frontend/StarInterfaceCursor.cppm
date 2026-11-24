module;
import std;
import star.json;
import star.format;

import star.string;
import star.data_stream;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;



import star.drawable;
import star.animation;

import star.vector;
namespace Star {

class GuiContext;
class Assets;
class ImageMetadataDatabase;

class InterfaceCursor {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<InterfaceCursor>, StarException> create(GuiContext& context);

  // Sets the cursor to the default defined in interface.config
  [[nodiscard]] std::expected<void, StarException> resetCursor();

  // Sets the cursor config to the given config IF the config is different than
  // the current one.  Expects a full asset path to the cursor config.
  [[nodiscard]] std::expected<void, StarException> setCursor(String const& configFile);

  [[nodiscard]] std::expected<Drawable, StarException> drawable() const;
  Vec2I size() const;
  Vec2I offset() const;
  float scale(float interfaceScale = 0) const;

  [[nodiscard]] std::expected<void, StarException> update(float dt);

private:
  explicit InterfaceCursor(GuiContext& context);
  std::expected<void, StarException> selectCursor(String const& configFile, bool optional);
  std::expected<void, StarException> loadCursor(String const& configFile, Assets const& assets, ImageMetadataDatabase const& metadata);
  std::reference_wrapper<GuiContext> m_guiContext;
  String m_configFile;
  std::optional<String> m_rejectedOverride;
  Vec2I m_offset;
  Vec2I m_size;
  unsigned int m_scale;
  std::variant<std::monostate, String, Animation> m_drawable;
};

}

export module star.interface_cursor;

export namespace Star {
  using ::Star::InterfaceCursor;
}
