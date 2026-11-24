import std;
import star.format;
import star.json;
import star.json_extra;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.string;
import star.color;
import star.ordered_map;
import star.bi_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.listener;

import star.drawable;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;


import star.animation;


import star.interface_cursor;


import star.image_metadata_database;

import star.gui_context;
import star.vector;
namespace Star {

InterfaceCursor::InterfaceCursor(GuiContext& context) : m_guiContext(context), m_scale(0) {}

std::expected<std::shared_ptr<InterfaceCursor>, StarException> InterfaceCursor::create(GuiContext& context) {
  auto cursor = std::shared_ptr<InterfaceCursor>(new InterfaceCursor(context));
  auto reset = cursor->resetCursor();
  if (!reset)
    return std::unexpected(std::move(reset.error()));
  return cursor;
}

std::expected<void, StarException> InterfaceCursor::resetCursor() {
  auto assets = m_guiContext.get().root().assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto config = (*assets)->json("/interface.config:defaultCursor");
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto path = config->to_string();
  if (!path)
    return std::unexpected(std::move(path.error()));
  return setCursor(*path);
}

std::expected<void, StarException> InterfaceCursor::setCursor(String const& configFile) {
  if (m_configFile == configFile)
    return {};

  auto& root = m_guiContext.get().root();
  auto assets = root.assets();
  if (!assets)
    return std::unexpected(std::move(assets.error()));
  auto imageMetadata = root.imageMetadataDatabase();
  if (!imageMetadata)
    return std::unexpected(std::move(imageMetadata.error()));
  auto config = (*assets)->json(configFile);
  if (!config)
    return std::unexpected(std::move(config.error()));
  auto offsetJson = config->get("offset");
  if (!offsetJson)
    return std::unexpected(std::move(offsetJson.error()));
  auto offset = json_to_vec2i(*offsetJson);
  if (!offset)
    return std::unexpected(std::move(offset.error()));

  std::variant<std::monostate, String, Animation> drawable;
  Vec2I size;
  auto hasImage = config->contains("image");
  if (!hasImage) return std::unexpected(std::move(hasImage.error()));
  if (*hasImage) {
    auto image = config->get_string("image");
    if (!image)
      return std::unexpected(std::move(image.error()));
    auto imageSize = (*imageMetadata)->imageSize(*image);
    if (!imageSize)
      return std::unexpected(std::move(imageSize.error()));
    drawable = std::move(*image);
    size = Vec2I(*imageSize);
  } else {
    auto animationJson = config->get("animation");
    if (!animationJson)
      return std::unexpected(std::move(animationJson.error()));
    auto animation = Animation::create(root, *animationJson, "/interface");
    if (!animation)
      return std::unexpected(std::move(animation.error()));
    auto frame = animation->drawable(1.0f);
    if (!frame)
      return std::unexpected(std::move(frame.error()));
    auto bounds = frame->bound_box(**imageMetadata, false);
    if (!bounds)
      return std::unexpected(std::move(bounds.error()));
    size = Vec2I(bounds->size());
    drawable = std::move(*animation);
  }
  auto scale = config->get_uint("scale", 0);
  if (!scale)
    return std::unexpected(std::move(scale.error()));
  m_configFile = configFile;
  m_offset = *offset;
  m_size = size;
  m_scale = *scale;
  m_drawable = std::move(drawable);
  return {};
}

std::expected<Drawable, StarException> InterfaceCursor::drawable() const {
  if (std::holds_alternative<String>(m_drawable)) {
    auto const& image = std::get<String>(m_drawable);
    auto metadata = m_guiContext.get().root().imageMetadataDatabase();
    if (!metadata)
      return std::unexpected(std::move(metadata.error()));
    return Drawable::make_image(**metadata, image, 1.0f, false, {});
  }
  if (std::holds_alternative<Animation>(m_drawable))
    return std::get<Animation>(m_drawable).drawable(1.0f);
  return std::unexpected(StarException("Cursor has not been initialized"));
}

Vec2I InterfaceCursor::size() const {
  return m_size;
}

Vec2I InterfaceCursor::offset() const {
  return m_offset;
}

float InterfaceCursor::scale(float interfaceScale) const {
  return m_scale ? m_scale : interfaceScale;
}

std::expected<void, StarException> InterfaceCursor::update(float dt) {
  if (std::holds_alternative<Animation>(m_drawable))
    std::get<Animation>(m_drawable).update(dt);
  return {};
}

}
