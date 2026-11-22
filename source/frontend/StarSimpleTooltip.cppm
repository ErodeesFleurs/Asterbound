module;
import std;
import star.format;
import star.json;
import star.string;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;


import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.font_texture_group;
import star.anchor_types;
import star.text_painter;
import star.drawable;
import star.asset_texture_group;
import star.drawable_painter;
import star.gui_types;
import star.key_bindings;
import star.mixer;
import star.gui_context;
import star.widget;
import star.widget_parsing;
import star.gui_reader;
import star.pane;

export module star.simple_tooltip;

export namespace Star::SimpleTooltipBuilder {
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> buildTooltip(GuiContext& context, String const& text);
}

namespace Star {

std::expected<std::shared_ptr<Pane>, StarException> SimpleTooltipBuilder::buildTooltip(GuiContext& context, String const& text) {
  auto createResult895 = Pane::create(context);
  if (!createResult895)
    return std::unexpected(std::move(createResult895.error()));
  std::shared_ptr<Pane> tooltip = std::move(*createResult895);
  tooltip->removeAllChildren();
  GuiReader reader(context);
  auto assetsResult896 = context.root().assets();
  if (!assetsResult896)
    return std::unexpected(std::move(assetsResult896.error()));
  auto jsonResult897 = std::move(*assetsResult896)->json("/interface/tooltips/simpletooltip.tooltip");
  if (!jsonResult897)
    return std::unexpected(std::move(jsonResult897.error()));
  auto constructResult898 = reader.construct(std::move(*jsonResult897), *tooltip);
  if (!constructResult898)
    return std::unexpected(std::move(constructResult898.error()));
  auto setLabelResult899 = tooltip->setLabel("contentLabel", text);
  if (!setLabelResult899)
    return std::unexpected(std::move(setLabelResult899.error()));

  auto fetchChildResult900 = tooltip->fetchChild<Widget>("stretchBackground");
  if (!fetchChildResult900)
    return std::unexpected(std::move(fetchChildResult900.error()));
  auto stretchBackground = std::move(*fetchChildResult900);
  auto fetchChildResult901 = tooltip->fetchChild<Widget>("contentLabel");
  if (!fetchChildResult901)
    return std::unexpected(std::move(fetchChildResult901.error()));
  stretchBackground->setSize(Vec2I{std::move(*fetchChildResult901)->size()[0] + 8, stretchBackground->size()[1]});
  tooltip->setSize(stretchBackground->size());

  return tooltip;
}

}
