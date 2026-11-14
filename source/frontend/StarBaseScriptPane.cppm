module;
import std;
import star.type_identity;
import star.format;
import star.json;

import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.game_types;
import star.set;
import star.audio;
import star.map;
import star.lua_components;
import star.lua;
import star.lua_root;


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
import star.pane;
import star.widget_parsing;
import star.gui_reader;

namespace Star {

class CanvasWidget;

// A more 'raw' script pane that doesn't depend on a world being present.
// Requires a derived class to provide a Lua root.
// Should maybe move into windowing?

class BaseScriptPane : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<BaseScriptPane, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<BaseScriptPane>(); }

  [[nodiscard]] static std::expected<std::shared_ptr<BaseScriptPane>, StarException> create(GuiContext& guiContext, Json config, bool construct = true);

  [[nodiscard]] std::expected<void, StarException> addCallbacks(String name, LuaCallbacks callbacks);
  [[nodiscard]] std::expected<bool, StarException> removeCallbacks(String const& name);
  [[nodiscard]] std::expected<void, StarException> setLuaRoot(std::shared_ptr<LuaRoot> root);
  // Retire borrowed script capabilities without dismissing the presentation.
  [[nodiscard]] std::expected<void, StarException> stopScripts();
  [[nodiscard]] std::expected<void, StarException> resumeScripts();
  [[nodiscard]] std::expected<std::optional<Json>, StarException> call(String const& method, JsonArray const& args = {});
  [[nodiscard]] std::expected<bool, StarException> hasMethod(String const& method);
  [[nodiscard]] std::expected<Json, StarException> inputJson(InputEvent const& event) const;
  [[nodiscard]] std::expected<bool, StarException> forwardEvent(InputEvent const& event);

  [[nodiscard]] std::expected<void, StarException> show() override;
  [[nodiscard]] std::expected<void, StarException> displayed() override;
  [[nodiscard]] std::expected<void, StarException> dismissed() override;

  [[nodiscard]] std::expected<void, StarException> tick(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> sendEvent(InputEvent const& event) override;
  
  Json const& config() const;
  Json const& rawConfig() const;

  bool interactive() const override;

  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> createTooltip(Vec2I const& screenPosition) override;
  [[nodiscard]] std::expected<std::optional<String>, StarException> cursorOverride(Vec2I const& screenPosition) override;
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<Item>>, StarException> shiftItemFromInventory(std::shared_ptr<Item> const& input) override;
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makePaneCallbacks() override;

protected:
  explicit BaseScriptPane(GuiContext& guiContext);
  [[nodiscard]] std::expected<void, StarException> init(Json config, bool construct = true);
  std::shared_ptr<GuiReader> reader() override;
  [[nodiscard]] std::expected<void, StarException> construct(Json config);
  [[nodiscard]] std::expected<void, StarException> renderImpl() override;

  Json m_config;
  Json m_rawConfig;

  std::shared_ptr<GuiReader> m_reader;

  Map<std::shared_ptr<CanvasWidget>, String> m_canvasClickCallbacks;
  Map<std::shared_ptr<CanvasWidget>, String> m_canvasKeyCallbacks;

  bool m_interactive;

  bool m_callbacksAdded;
  mutable LuaUpdateScript m_script;
};


}

export module star.base_script_pane;

export namespace Star {
  using ::Star::CanvasWidget;
  using ::Star::BaseScriptPane;
}
