import std;
import star.json;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.ordered_map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.interpolation;
import star.game_types;

import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.renderer;
import star.asset_source;
import star.assets;
import star.root_base;
import star.configuration;
import star.root;
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
import star.fuel_widget;

import star.format;
namespace Star {

std::expected<std::shared_ptr<FuelWidget>, StarException> FuelWidget::create(GuiContext& context) {
  auto widget = std::shared_ptr<FuelWidget>(new FuelWidget(context));
  auto assets = context.root().assets();
  if (!assets) return std::unexpected(std::move(assets.error()));
  auto fontJson = (*assets)->json("/interface.config:font.buttonSize");
  if (!fontJson) return std::unexpected(std::move(fontJson.error()));
  auto fontSize = fontJson->toInt();
  if (!fontSize) return std::unexpected(std::move(fontSize.error()));
  widget->m_textStyle.fontSize = *fontSize;
  auto styleJson = (*assets)->json("/interface.config:textStyle");
  if (!styleJson) return std::unexpected(std::move(styleJson.error()));
  auto style = widget->m_textStyle.loadJson(*styleJson);
  if (!style) return std::unexpected(std::move(style.error()));
  return widget;
}

FuelWidget::FuelWidget(GuiContext& context) : Widget(context) {
  m_fuelLevel = 0;
  m_maxLevel = 0;
  m_potential = 0;
  m_requested = 0;

  m_pingTimeout = 0;
  disableScissoring();
}

std::expected<void, StarException> FuelWidget::update(float dt) {
  if ((m_pingTimeout -= dt) < 0)
    m_pingTimeout = 0;
  return {};
}

std::expected<void, StarException> FuelWidget::renderImpl() {
  auto scissor = context().resetInterfaceScissorRect();
  if (!scissor) return std::unexpected(std::move(scissor.error()));

  auto texture = context().textureSize("/interface/fuel/fuelgauge.png");
  if (!texture) return std::unexpected(std::move(texture.error()));
  Vec2F textureSize(*texture);
  RectF entireTex = RectF::withSize({}, textureSize);
  RectF entirePosition = RectF::withSize(Vec2F(screenPosition()), textureSize);
  Vec2F textPosition = entirePosition.center();

  float fuel = 1;
  float fuelPotential = 1;
  float fuelRequested = 0;
  if (m_maxLevel > 0) {
    fuel = std::min(1.0f, m_fuelLevel / m_maxLevel);
    fuelPotential = std::min(1.0f, (m_fuelLevel + m_potential) / m_maxLevel);
    fuelRequested = std::min(1.0f, m_requested / m_maxLevel);
    fuel -= fuelRequested;
    if (fuel < 0)
      fuel = 0;
  }

  auto shift = [](float begin, float end, RectF templ) {
    RectF result = templ;

    result.min()[0] = lerp(begin, templ.min()[0], templ.max()[0]);
    result.max()[0] = lerp(end, templ.min()[0], templ.max()[0]);

    return result;
  };

  auto background = context().drawInterfaceQuad(
      std::fmod(m_pingTimeout, 0.2f) > 0.1f ? std::string_view("/interface/fuel/fuelgaugebackgroundflash.png") : std::string_view("/interface/fuel/fuelgaugebackground.png"),
      shift(0, 1, entireTex), shift(0, 1, entirePosition));
  if (!background) return std::unexpected(std::move(background.error()));
  auto green = context().drawInterfaceQuad("/interface/fuel/fuelgaugegreen.png", shift(fuel, fuelPotential, entireTex), shift(fuel, fuelPotential, entirePosition));
  if (!green) return std::unexpected(std::move(green.error()));
  auto red = context().drawInterfaceQuad("/interface/fuel/fuelgaugered.png", shift(fuel, fuelRequested, entireTex), shift(fuel, fuelRequested, entirePosition));
  if (!red) return std::unexpected(std::move(red.error()));
  auto gauge = context().drawInterfaceQuad("/interface/fuel/fuelgauge.png", shift(0, fuel, entireTex), shift(0, fuel, entirePosition));
  if (!gauge) return std::unexpected(std::move(gauge.error()));
  auto markings = context().drawInterfaceQuad("/interface/fuel/fuelgaugemarkings.png", shift(0, 1, entireTex), shift(0, 1, entirePosition));
  if (!markings) return std::unexpected(std::move(markings.error()));

  auto& guiContext = context();
  auto style = guiContext.setTextStyle(m_textStyle);
  if (!style) return std::unexpected(std::move(style.error()));
  Color color = Color::White;
  if (m_potential == 0 && m_fuelLevel == 0)
    color = m_requested != 0 && m_requested == m_fuelLevel ? Color::Orange : Color::Red;
  auto font = guiContext.setFontColor(color.toRgba());
  if (!font) return std::unexpected(std::move(font.error()));
  auto text = guiContext.renderInterfaceText(
    strf("Fuel {}/{}", std::min(m_fuelLevel + m_potential, m_maxLevel), static_cast<int>(m_maxLevel)),
    {textPosition, HorizontalAnchor::HMidAnchor, VerticalAnchor::VMidAnchor});
  if (!text) return std::unexpected(std::move(text.error()));
  return {};
}

void FuelWidget::setCurrentFuelLevel(float amount) {
  m_fuelLevel = amount;
}

void FuelWidget::setMaxFuelLevel(float amount) {
  m_maxLevel = amount;
}

void FuelWidget::setPotentialFuelAmount(float amount) {
  m_potential = amount;
}

void FuelWidget::setRequestedFuelAmount(float amount) {
  m_requested = amount;
}

void FuelWidget::ping() {
  m_pingTimeout = 1.0f;
}

}
