module;
import std;
import star.format;
import star.json;

import star.casting;
import star.input_event;
import star.application;
import star.statistics_service;
import star.p2p_networking_service;
import star.user_generated_content_service;
import star.desktop_service;
import star.image;
import star.application_controller;
import star.color;
import star.font;
import star.bi_map;
import star.ref_ptr;
import star.renderer;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
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

namespace Star {

class Widget;
class Pane;

struct WidgetParserExceptionTag {
  static constexpr std::string_view name() { return "WidgetParserException"; }
};
using WidgetParserException = StarError<WidgetParserExceptionTag, StarException>;

struct WidgetConstructResult {
  WidgetConstructResult();
  WidgetConstructResult(std::shared_ptr<Widget> obj, String const& name, float zlevel);

  std::shared_ptr<Widget> obj;
  String name;
  float zlevel;
};

using ConstuctorFunc =
  std::function<std::expected<WidgetConstructResult, StarException>(String const& name, Json const& config)>;

class WidgetParser {
public:
  WidgetParser(GuiContext& context);
  virtual ~WidgetParser() = default;

  [[nodiscard]] virtual std::expected<void, StarException> construct(Json const& config, Widget& widget);
  void registerCallback(String const& name, WidgetCallbackFunc callback);
  [[nodiscard]] std::expected<std::shared_ptr<Widget>, StarException> makeSingle(String const& name, Json const& config);

protected:
  [[nodiscard]] std::expected<void, StarException> constructImpl(Json const& config, Widget& widget);
  [[nodiscard]] std::expected<List<WidgetConstructResult>, StarException> constructor(Json const& config);

  // Parents
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> stackHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> scrollAreaHandler(String const& name, Json const& config);

  // Interactive
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> radioGroupHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> buttonHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> spinnerHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> textboxHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> itemSlotHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> itemGridHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> listHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> sliderHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> largeCharPlateHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> tabSetHandler(String const& name, Json const& config);

  // Non-interactive
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> widgetHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> imageHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> imageStretchHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> portraitHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> labelHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> canvasHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> fuelGaugeHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> progressHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> containerHandler(String const& name, Json const& config);
  [[nodiscard]] std::expected<WidgetConstructResult, StarException> layoutHandler(String const& name, Json const& config);

  // Utilities
  [[nodiscard]] std::expected<void, StarException> common(std::shared_ptr<Widget> widget, Json const& config, bool getChildren = true);
  [[nodiscard]] std::expected<ImageStretchSet, StarException> parseImageStretchSet(Json const& config);

  GuiContext& m_context;
  std::weak_ptr<Pane> m_pane;
  StringMap<ConstuctorFunc> m_constructors;
  StringMap<WidgetCallbackFunc> m_callbacks;
};

}

export module star.widget_parsing;

export namespace Star {
  using ::Star::WidgetParserExceptionTag;
  using ::Star::WidgetConstructResult;
  using ::Star::WidgetParser;
  using ::Star::Widget;
  using ::Star::Pane;
  using ::Star::WidgetParserException;
  using ::Star::ConstuctorFunc;
}
