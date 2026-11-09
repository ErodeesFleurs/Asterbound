import star.json_extra;
import std;
import star.algorithm;
import star.memory;
import star.json;
import star.id_map;
import star.ordered_map;
import star.lru_cache;
import star.thread;
import star.time;
import star.random;
import star.ttl_cache;
import star.casting;
import star.game_types;
import star.strong_typedef;
import star.rpc_promise;
import star.vector;
import star.rect;
import star.bi_map;
import star.a_star;
import star.data_stream;
import star.lua;
import star.input_event;
import star.color;
import star.font;
import star.list;
import star.image_processing;
import star.hash;
import star.string_view;
import star.directives;
import star.string;
import star.text;
import star.poly;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.io_device;
import star.ref_ptr;
import star.logging;
import star.format;
import star.version;

import star.lua_root;
import star.lua_components;

import star.damage_types;
import star.world_geometry;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.tile_damage;
import star.interaction_types;
import star.celestial_coordinate;
import star.quest_descriptor;
import star.interactive_entity;
import star.collision_block;
import star.tile_entity;
import star.tile_modification;
import star.force_regions;
import star.world;
import star.physics_entity;
import star.movement_controller;
import star.platformer_astar_types;
import star.anchorable_entity;

import star.game_timers;
import star.actor_movement_controller;
import star.item;
import star.item_descriptor;
import star.item_database;
import star.item_recipe;
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
import star.widget_parsing;
import star.gui_reader;
import star.widget_lua_bindings;
import star.lua_game_converters;
import star.canvas_widget;
import star.label_widget;
import star.list_widget;
import star.button_group;
import star.button_widget;
import star.text_box_widget;
import star.progress_widget;
import star.portrait_entity;
import star.portrait_widget;
import star.image_widget;
import star.slider_bar;
import star.animation;
import star.item_slot_widget;
import star.item_grid_widget;
import star.layout;
import star.flow_layout;
import star.image_stretch_widget;
import star.scroll_area;


import star.item_bag;

import star.matrix3;
namespace Star {

namespace {

template <typename T, typename Function>
[[nodiscard]] auto withWidget(std::weak_ptr<Widget> const& parent, String const& name, Function&& function)
    -> std::invoke_result_t<Function, std::shared_ptr<T>> {
  auto owner = parent.lock();
  if (!owner)
    return std::unexpected(GuiException("Widget callback target no longer exists"));
  auto widget = owner->fetchChild<T>(name);
  if (!widget)
    return std::unexpected(std::move(widget.error()));
  if (!*widget)
    return {};
  return std::invoke(std::forward<Function>(function), std::move(*widget));
}

std::expected<TextStyle, StarException> canvasTextStyle(Json const& sizeOrStyle, std::optional<Color> const& color,
    std::optional<float> spacing, std::optional<String> const& font, std::optional<String> const& directives) {
  if (sizeOrStyle.isType(Json::Type::Object))
    return TextStyle::create(sizeOrStyle);
  auto size = sizeOrStyle.toUInt();
  if (!size)
    return std::unexpected(std::move(size.error()));
  TextStyle style;
  style.fontSize = *size;
  style.color = color.value_or(Color::White).toRgba();
  style.lineSpacing = spacing.value_or(DefaultLineSpacing);
  style.font = font.value_or("");
  style.directives = directives.value_or("");
  return style;
}

struct RetireMemberCallback {
  std::reference_wrapper<std::optional<LuaFunction>> callback;
  void operator()() const { callback.get().reset(); }
};

struct ScopedMemberCallback {
  ScopedMemberCallback(LuaFunction function, std::stop_token lifetime,
      std::optional<std::reference_wrapper<LuaBaseComponent>> issuer)
      : callback(std::move(function)), lifetime(lifetime), issuer(issuer),
        retirement(lifetime, RetireMemberCallback{std::ref(callback)}) {}

  std::optional<LuaFunction> callback;
  std::stop_token lifetime;
  std::optional<std::reference_wrapper<LuaBaseComponent>> issuer;
  std::stop_callback<RetireMemberCallback> retirement;
};
}

std::expected<LuaMethods<LuaCanvas>, StarException> LuaUserDataMethods<LuaCanvas>::make() {
  LuaMethods<LuaCanvas> methods;
  auto registerCanvasMethod = [&]<typename Function>(String name, Function function) {
    using Return = typename AlgorithmResultInfo<typename FunctionTraits<Function>::Return>::Value;
    auto guarded = [function = std::move(function)]<typename... Args>(
                     std::type_identity<std::tuple<std::shared_ptr<CanvasWidget>, Args...>>) mutable {
      return [function = std::move(function)](LuaCanvas const& handle,
                                              Args... args) mutable -> std::expected<Return, StarException> {
        if (handle.lifetime.stop_requested() || handle.hostLifetime.stop_requested())
          return std::unexpected(GuiException("Canvas callback scope is retired"));
        auto owner = handle.owner ? handle.owner->lock() : std::shared_ptr<Widget>{};
        if (handle.owner && !owner)
          return std::unexpected(GuiException("Canvas callback target no longer exists"));
        if constexpr (std::is_void_v<typename FunctionTraits<Function>::Return>) {
          std::invoke(function, handle.canvas, std::forward<Args>(args)...);
          return {};
        } else {
          return std::invoke(function, handle.canvas, std::forward<Args>(args)...);
        }
      };
    }(std::type_identity<typename FunctionTraits<Function>::ArgTuple>{});
    return methods.registerMethod(std::move(name), std::move(guarded));
  };
  if (auto registered =
        registerCanvasMethod("size", [](std::shared_ptr<CanvasWidget> canvas) { return canvas->size(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "mousePosition", [](std::shared_ptr<CanvasWidget> canvas) { return canvas->mousePosition(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("clear", [](std::shared_ptr<CanvasWidget> canvas) { canvas->clear(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "pushClipRect", [](std::shared_ptr<CanvasWidget> canvas, RectF const& rect) { canvas->pushClipRect(rect); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        registerCanvasMethod("popClipRect", [](std::shared_ptr<CanvasWidget> canvas) { return canvas->popClipRect(); });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "drawDrawable",
        [](std::shared_ptr<CanvasWidget> canvas, Drawable drawable, std::optional<Vec2F> position,
           std::optional<float> scale, std::optional<Vec2F> translation, std::optional<float> finalScale) {
          canvas->drawDrawable(std::move(drawable), position.value_or(Vec2F()), scale.value_or(1),
                               translation.value_or(Vec2F()), finalScale.value_or(1));
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "drawDrawables",
        [](std::shared_ptr<CanvasWidget> canvas, List<Drawable> drawables, std::optional<Vec2F> position,
           std::optional<float> scale, std::optional<Vec2F> translation, std::optional<float> finalScale) {
          canvas->drawDrawables(std::move(drawables), position.value_or(Vec2F()), scale.value_or(1),
                                translation.value_or(Vec2F()), finalScale.value_or(1));
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("drawJsonDrawable",
                                             [](std::shared_ptr<CanvasWidget> canvas, Json config,
                                                std::optional<Vec2F> position) -> std::expected<void, StarException> {
                                               auto metadata = canvas->context().root().imageMetadataDatabase();
                                               if (!metadata)
                                                 return std::unexpected(std::move(metadata.error()));
                                               auto drawable = Drawable::create(**metadata, config);
                                               if (!drawable)
                                                 return std::unexpected(std::move(drawable.error()));
                                               canvas->drawDrawable(std::move(*drawable), position.value_or(Vec2F()));
                                               return {};
                                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("drawJsonDrawables",
                                             [](std::shared_ptr<CanvasWidget> canvas, JsonArray configs,
                                                std::optional<Vec2F> position) -> std::expected<void, StarException> {
                                               auto metadata = canvas->context().root().imageMetadataDatabase();
                                               if (!metadata)
                                                 return std::unexpected(std::move(metadata.error()));
                                               for (auto const& config : configs) {
                                                 auto drawable = Drawable::create(**metadata, config);
                                                 if (!drawable)
                                                   return std::unexpected(std::move(drawable.error()));
                                                 canvas->drawDrawable(std::move(*drawable), position.value_or(Vec2F()));
                                               }
                                               return {};
                                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "drawImage",
        [](std::shared_ptr<CanvasWidget> canvas, String image, Vec2F position, std::optional<float> scale,
           std::optional<Color> color, std::optional<bool> centered) {
          if (centered.value_or(false))
            canvas->drawImageCentered(image, position, scale.value_or(1.0f), color.value_or(Color::White).toRgba());
          else
            canvas->drawImage(image, position, scale.value_or(1.0f), color.value_or(Color::White).toRgba());
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "drawImageDrawable",
        [](std::shared_ptr<CanvasWidget> canvas, String image, Vec2F position,
           std::variant<std::monostate, Vec2F, float> scale, std::optional<Color> color,
           std::optional<float> rotation) -> std::expected<void, StarException> {
          auto metadata = canvas->context().root().imageMetadataDatabase();
          if (!metadata)
            return std::unexpected(std::move(metadata.error()));
          auto drawable = Drawable::makeImage(**metadata, image, 1.0, true, {0.0, 0.0}, color.value_or(Color::White));
          if (!drawable)
            return std::unexpected(std::move(drawable.error()));
          if (std::holds_alternative<Vec2F>(scale))
            drawable->transform(Mat3F::scaling(std::get<Vec2F>(scale)));
          else if (std::holds_alternative<float>(scale))
            drawable->transform(Mat3F::scaling(std::get<float>(scale)));
          if (rotation)
            drawable->rotate(*rotation);
          canvas->drawDrawable(std::move(*drawable), position);
          return {};
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("drawImageRect",
                                             [](std::shared_ptr<CanvasWidget> canvas, String image, RectF texture,
                                                RectF screen, std::optional<Color> color) {
                                               canvas->drawImageRect(std::move(image), texture, screen,
                                                                     color.value_or(Color::White).toRgba());
                                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("drawTiledImage",
                                             [](std::shared_ptr<CanvasWidget> canvas, String image, Vec2D offset,
                                                RectF screen, std::optional<float> scale, std::optional<Color> color) {
                                               canvas->drawTiledImage(std::move(image), scale.value_or(1.0f), offset,
                                                                      screen, color.value_or(Color::White).toRgba());
                                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod("drawLine",
                                             [](std::shared_ptr<CanvasWidget> canvas, Vec2F begin, Vec2F end,
                                                std::optional<Color> color, std::optional<float> width) {
                                               canvas->drawLine(begin, end, color.value_or(Color::White).toRgba(),
                                                                width.value_or(1.0f));
                                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        registerCanvasMethod("drawRect",
                             [](std::shared_ptr<CanvasWidget> canvas, RectF rect, std::optional<Color> color) {
                               canvas->drawRect(rect, color.value_or(Color::White).toRgba());
                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = registerCanvasMethod(
        "drawPoly",
        [](std::shared_ptr<CanvasWidget> canvas, PolyF poly, std::optional<Color> color, std::optional<float> width) {
          canvas->drawPoly(poly, color.value_or(Color::White).toRgba(), width.value_or(1.0f));
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        registerCanvasMethod("drawTriangles",
                             [](std::shared_ptr<CanvasWidget> canvas, List<PolyF> polygons,
                                std::optional<Color> color) -> std::expected<void, StarException> {
                               List<std::tuple<Vec2F, Vec2F, Vec2F>> triangles;
                               triangles.reserve(polygons.size());
                               for (auto const& polygon : polygons) {
                                 if (polygon.sides() != 3)
                                   return std::unexpected(StarException("Triangle must have exactly 3 sides"));
                                 triangles.emplace_back(polygon.vertex(0), polygon.vertex(1), polygon.vertex(2));
                               }
                               canvas->drawTriangles(triangles, color.value_or(Color::White).toRgba());
                               return {};
                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        registerCanvasMethod("drawText",
                             [](std::shared_ptr<CanvasWidget> canvas, String text, Json config, Json sizeOrStyle,
                                std::optional<Color> color, std::optional<float> spacing, std::optional<String> font,
                                std::optional<String> directives) -> std::expected<void, StarException> {
                               auto positioning = TextPositioning::create(config);
                               if (!positioning)
                                 return std::unexpected(std::move(positioning.error()));
                               auto style = canvasTextStyle(sizeOrStyle, color, spacing, font, directives);
                               if (!style)
                                 return std::unexpected(std::move(style.error()));
                               canvas->drawText(std::move(text), *positioning, std::move(*style));
                               return {};
                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered =
        registerCanvasMethod("measureText",
                             [](std::shared_ptr<CanvasWidget> canvas, String text, Json config, Json sizeOrStyle,
                                std::optional<Color> color, std::optional<float> spacing, std::optional<String> font,
                                std::optional<String> directives) -> std::expected<RectF, StarException> {
                               auto positioning = TextPositioning::create(config);
                               if (!positioning)
                                 return std::unexpected(std::move(positioning.error()));
                               auto style = canvasTextStyle(sizeOrStyle, color, spacing, font, directives);
                               if (!style)
                                 return std::unexpected(std::move(style.error()));
                               return canvas->measureText(text, *positioning, *style);
                             });
      !registered)
    return std::unexpected(std::move(registered.error()));
  return methods;
}

namespace LuaBindings {

std::expected<LuaCallbacks, StarException> makeWidgetCallbacks(GuiContext& context, Widget& widget, std::shared_ptr<GuiReader> reader) {
  auto parentWidget = widget.weak_from_this();
  if (!reader)
    reader = std::make_shared<GuiReader>(context);
  LuaCallbacks callbacks;
  if (auto registered = callbacks.registerCallback("playSound", [parentWidget](String const& audio, std::optional<int> loops, std::optional<float> volume) -> std::expected<void, StarException> {
    auto owner = parentWidget.lock();
    if (!owner)
      return std::unexpected(GuiException("Widget callback target no longer exists"));
    auto played = owner->context().playAudio(audio, loops.value_or(0), volume.value_or(1.0f));
    if (!played)
      return std::unexpected(std::move(played.error()));
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(
        "bindCanvas",
        [parentWidget](LuaEngine& engine,
                       String const& widgetName) -> std::expected<std::optional<LuaCanvas>, StarException> {
          return withWidget<CanvasWidget>(
            parentWidget, widgetName,
            [&](std::shared_ptr<CanvasWidget> widget) -> std::expected<std::optional<LuaCanvas>, StarException> {
              return LuaCanvas{.canvas = std::move(widget),
                               .lifetime = engine.promiseLifetime(),
                               .hostLifetime = {},
                               .owner = parentWidget};
            });
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getPosition", [parentWidget](String const& widgetName) -> std::expected<std::optional<Vec2I>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<Vec2I>, StarException> {
      return widget->relativePosition();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setPosition", [parentWidget](String const& widgetName, Vec2I const& position) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->setPosition(position);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getSize", [parentWidget](String const& widgetName) -> std::expected<std::optional<Vec2I>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<Vec2I>, StarException> {
      return widget->size();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSize", [parentWidget](String const& widgetName, Vec2I const& size) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->setSize(size);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setDrawingOffset", [parentWidget](String const& widgetName, Vec2I const& offset) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->setDrawingOffset(offset);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setScissoringEnabled", [parentWidget](String const& widgetName, bool enabled) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      if (enabled)
        widget->enableScissoring();
      else
        widget->disableScissoring();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setVisible", [parentWidget](String const& widgetName, bool visible) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      return widget->setVisibility(visible);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("active", [parentWidget](String const& widgetName) -> std::expected<std::optional<bool>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<bool>, StarException> {
      return widget->active();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("focus", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      return widget->focus();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("hasFocus", [parentWidget](String const& widgetName) -> std::expected<std::optional<bool>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<bool>, StarException> {
      auto focused = widget->hasFocus();
      if (!focused)
        return std::unexpected(std::move(focused.error()));
      return *focused;
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("blur", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      return widget->blur();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getData", [parentWidget](String const& widgetName) -> std::expected<Json, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<Json, StarException> {
      return widget->data();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setData", [parentWidget](String const& widgetName, Json const& data) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->setData(data);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getChildAt", [parentWidget](Vec2I const& position) -> std::expected<std::optional<String>, StarException> {
    auto owner = parentWidget.lock();
    if (!owner)
      return std::unexpected(GuiException("Widget callback target no longer exists"));
    if (auto widget = owner->getChildAt(position))
      return widget->fullName();
    return {};
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("inMember", [parentWidget](String const& widgetName, Vec2I const& position) -> std::expected<std::optional<bool>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<bool>, StarException> {
      return widget->inMember(position);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("addChild", [parentWidget, reader](String const& widgetName, Json const& config, std::optional<String> const& childName) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      String name = childName.value_or(Star::to_string(widget->context().root().random().randu64()));
      auto child = reader->makeSingle(name, config);
      if (!child)
        return std::unexpected(std::move(child.error()));
      if (*child)
        return widget->addChild(name, std::move(*child));
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("removeAllChildren", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->removeAllChildren();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("removeChild", [parentWidget](String const& widgetName, String const& childName) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      widget->removeChild(childName);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setHint", [parentWidget](String const& widgetName, String const& hint) -> std::expected<void, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, widgetName, [&](std::shared_ptr<TextBoxWidget> widget) -> std::expected<void, StarException> {
      widget->setHint(hint);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getHint", [parentWidget](String const& widgetName) -> std::expected<std::optional<String>, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, widgetName, [&](std::shared_ptr<TextBoxWidget> widget) -> std::expected<std::optional<String>, StarException> {
      return widget->getHint();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setCursorPosition", [parentWidget](String const& widgetName, int position) -> std::expected<void, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, widgetName, [&](std::shared_ptr<TextBoxWidget> widget) -> std::expected<void, StarException> {
      return widget->setCursorPosition(position);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getCursorPosition", [parentWidget](String const& widgetName) -> std::expected<std::optional<int>, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, widgetName, [&](std::shared_ptr<TextBoxWidget> widget) -> std::expected<std::optional<int>, StarException> {
      return widget->getCursorPosition();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setTextEditorRendering", [parentWidget](String const& name, bool enabled) -> std::expected<void, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, name, [&](std::shared_ptr<TextBoxWidget> editor) -> std::expected<void, StarException> {
      editor->setEditorRendering(enabled);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getTextEditorState", [parentWidget](String const& name) -> std::expected<Json, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, name, [&](std::shared_ptr<TextBoxWidget> editor) {
      return editor->editorState();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setTextSelection", [parentWidget](String const& name, int anchor, int cursor) -> std::expected<void, StarException> {
    return withWidget<TextBoxWidget>(parentWidget, name, [&](std::shared_ptr<TextBoxWidget> editor) {
      return editor->setSelection(anchor, cursor);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setTextStyle", [parentWidget](String const& name, Json const& config) -> std::expected<void, StarException> {
    auto style = TextStyle::create(config);
    if (!style)
      return std::unexpected(std::move(style.error()));
    return withWidget<Widget>(parentWidget, name, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      if (auto label = as<LabelWidget>(widget))
        return label->setTextStyle(*style);
      if (auto editor = as<TextBoxWidget>(widget))
        editor->setTextStyle(*style);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getText", [parentWidget](String const& widgetName) -> std::expected<std::optional<String>, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<std::optional<String>, StarException> {
      if (auto label = as<LabelWidget>(widget))
        return label->text();
      if (auto button = as<ButtonWidget>(widget))
        return button->getText();
      if (auto textBox = as<TextBoxWidget>(widget))
        return textBox->getText();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setText", [parentWidget](String const& widgetName, String const& text) -> std::expected<void, StarException> {
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      if (auto label = as<LabelWidget>(widget))
        return label->setText(text);
      if (auto button = as<ButtonWidget>(widget))
        button->setText(text);
      else if (auto textBox = as<TextBoxWidget>(widget)) {
        auto changed = textBox->setText(text);
        if (!changed)
          return std::unexpected(std::move(changed.error()));
      }
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setTextCharLimit", [parentWidget](String const& name, std::optional<unsigned> limit) -> std::expected<void, StarException> {
    return withWidget<LabelWidget>(parentWidget, name, [&](std::shared_ptr<LabelWidget> label) {
      return label->setTextCharLimit(limit);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setProgressColor", [parentWidget](String const& name, Color color, std::optional<Color> mix, std::optional<float> amount) -> std::expected<void, StarException> {
    return withWidget<ProgressWidget>(parentWidget, name, [&](std::shared_ptr<ProgressWidget> progress) -> std::expected<void, StarException> {
      progress->setColor(mix ? color.mix(*mix, amount.value_or(0)) : color);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setProgressOverlay", [parentWidget](String const& name, String const& image) -> std::expected<void, StarException> {
    return withWidget<ProgressWidget>(parentWidget, name, [&](std::shared_ptr<ProgressWidget> progress) -> std::expected<void, StarException> {
      progress->setOverlay(image);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setPortraitEntity", [parentWidget](String const& name, std::optional<LuaUserData> const& handle) -> std::expected<void, StarException> {
    return withWidget<PortraitWidget>(parentWidget, name, [&](std::shared_ptr<PortraitWidget> portrait) -> std::expected<void, StarException> {
      std::shared_ptr<PortraitEntity> entity;
      if (handle) {
        auto value = handle->get<std::shared_ptr<PortraitEntity>>();
        if (!value) return std::unexpected(std::move(value.error()));
        entity = value->get();
      }
      return portrait->setEntity(std::move(entity));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setFontColor", [parentWidget](String const& widgetName, Color color, std::optional<float> alpha) -> std::expected<void, StarException> {
    if (alpha)
      color.setAlphaF(*alpha);
    return withWidget<Widget>(parentWidget, widgetName, [&](std::shared_ptr<Widget> widget) -> std::expected<void, StarException> {
      if (auto label = as<LabelWidget>(widget))
        label->setColor(color);
      else if (auto button = as<ButtonWidget>(widget))
        button->setFontColor(color);
      else if (auto textBox = as<TextBoxWidget>(widget))
        textBox->setColor(color);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImage", [parentWidget](String const& widgetName, String const& image) -> std::expected<void, StarException> {
    return withWidget<ImageWidget>(parentWidget, widgetName, [&](std::shared_ptr<ImageWidget> widget) -> std::expected<void, StarException> {
      return widget->setImage(image);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImageScale", [parentWidget](String const& widgetName, float scale) -> std::expected<void, StarException> {
    return withWidget<ImageWidget>(parentWidget, widgetName, [&](std::shared_ptr<ImageWidget> widget) -> std::expected<void, StarException> {
      return widget->setScale(scale);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImageRotation", [parentWidget](String const& widgetName, float rotation) -> std::expected<void, StarException> {
    return withWidget<ImageWidget>(parentWidget, widgetName, [&](std::shared_ptr<ImageWidget> widget) -> std::expected<void, StarException> {
      return widget->setRotation(rotation);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setButtonEnabled", [parentWidget](String const& widgetName, bool enabled) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      widget->setEnabled(enabled);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setButtonImage", [parentWidget](String const& widgetName, String const& image) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      return widget->setImages(image);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setButtonImages", [parentWidget](String const& widgetName, Json const& images) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      auto base = images.getString("base");
      if (!base)
        return std::unexpected(std::move(base.error()));
      auto hover = images.getString("hover", "");
      if (!hover)
        return std::unexpected(std::move(hover.error()));
      auto pressed = images.getString("pressed", "");
      if (!pressed)
        return std::unexpected(std::move(pressed.error()));
      auto disabled = images.getString("disabled", "");
      if (!disabled)
        return std::unexpected(std::move(disabled.error()));
      return widget->setImages(*base, *hover, *pressed, *disabled);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setButtonCheckedImages", [parentWidget](String const& widgetName, Json const& images) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      auto base = images.getString("base");
      if (!base)
        return std::unexpected(std::move(base.error()));
      auto hover = images.getString("hover", "");
      if (!hover)
        return std::unexpected(std::move(hover.error()));
      auto pressed = images.getString("pressed", "");
      if (!pressed)
        return std::unexpected(std::move(pressed.error()));
      auto disabled = images.getString("disabled", "");
      if (!disabled)
        return std::unexpected(std::move(disabled.error()));
      return widget->setCheckedImages(*base, *hover, *pressed, *disabled);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setButtonOverlayImage", [parentWidget](String const& widgetName, String const& image) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      widget->setOverlayImage(image);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getChecked", [parentWidget](String const& widgetName) -> std::expected<std::optional<bool>, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<std::optional<bool>, StarException> {
      return widget->isChecked();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setChecked", [parentWidget](String const& widgetName, bool checked) -> std::expected<void, StarException> {
    return withWidget<ButtonWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonWidget> widget) -> std::expected<void, StarException> {
      widget->setChecked(checked);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getSelectedOption", [parentWidget](String const& widgetName) -> std::expected<std::optional<int>, StarException> {
    return withWidget<ButtonGroupWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonGroupWidget> widget) -> std::expected<std::optional<int>, StarException> {
      return widget->checkedId();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getSelectedData", [parentWidget](String const& widgetName) -> std::expected<Json, StarException> {
    return withWidget<ButtonGroupWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonGroupWidget> widget) -> std::expected<Json, StarException> {
      if (auto button = widget->checkedButton())
        return button->get().data();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSelectedOption", [parentWidget](String const& widgetName, std::optional<int> index) -> std::expected<void, StarException> {
    return withWidget<ButtonGroupWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonGroupWidget> widget) -> std::expected<void, StarException> {
      return widget->select(index.value_or(ButtonGroup::NoButton));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setOptionEnabled", [parentWidget](String const& widgetName, int index, bool enabled) -> std::expected<void, StarException> {
    return withWidget<ButtonGroupWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonGroupWidget> widget) -> std::expected<void, StarException> {
      if (auto button = widget->button(index))
        button->get().setEnabled(enabled);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setOptionVisible", [parentWidget](String const& widgetName, int index, bool visible) -> std::expected<void, StarException> {
    return withWidget<ButtonGroupWidget>(parentWidget, widgetName, [&](std::shared_ptr<ButtonGroupWidget> widget) -> std::expected<void, StarException> {
      if (auto button = widget->button(index))
        return button->get().setVisibility(visible);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setProgress", [parentWidget](String const& widgetName, float value) -> std::expected<void, StarException> {
    return withWidget<ProgressWidget>(parentWidget, widgetName, [&](std::shared_ptr<ProgressWidget> widget) -> std::expected<void, StarException> {
      widget->setCurrentProgressLevel(value);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSliderEnabled", [parentWidget](String const& widgetName, bool enabled) -> std::expected<void, StarException> {
    return withWidget<SliderBarWidget>(parentWidget, widgetName, [&](std::shared_ptr<SliderBarWidget> widget) -> std::expected<void, StarException> {
      widget->setEnabled(enabled);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getSliderValue", [parentWidget](String const& widgetName) -> std::expected<std::optional<int>, StarException> {
    return withWidget<SliderBarWidget>(parentWidget, widgetName, [&](std::shared_ptr<SliderBarWidget> widget) -> std::expected<std::optional<int>, StarException> {
      return widget->val();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSliderValue", [parentWidget](String const& widgetName, int value) -> std::expected<void, StarException> {
    return withWidget<SliderBarWidget>(parentWidget, widgetName, [&](std::shared_ptr<SliderBarWidget> widget) -> std::expected<void, StarException> {
      return widget->setVal(value);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setSliderRange", [parentWidget](String const& widgetName, int min, int max, std::optional<int> delta) -> std::expected<void, StarException> {
    return withWidget<SliderBarWidget>(parentWidget, widgetName, [&](std::shared_ptr<SliderBarWidget> widget) -> std::expected<void, StarException> {
      return widget->setRange(min, max, delta.value_or(1));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("clearListItems", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<void, StarException> {
      return widget->clear();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("addListItem", [parentWidget](String const& widgetName) -> std::expected<std::optional<String>, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<std::optional<String>, StarException> {
      auto item = widget->addItem();
      if (!item)
        return std::unexpected(std::move(item.error()));
      return (*item)->name();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("removeListItem", [parentWidget](String const& widgetName, std::size_t index) -> std::expected<void, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<void, StarException> {
      return widget->removeItem(index);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getListSelected", [parentWidget](String const& widgetName) -> std::expected<std::optional<String>, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<std::optional<String>, StarException> {
      if (widget->selectedItem() != NPos)
        return widget->selectedWidget()->name();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setListSelected", [parentWidget](String const& widgetName, String const& name) -> std::expected<void, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<void, StarException> {
      auto selected = widget->fetchChild(name);
      if (!selected)
        return std::unexpected(std::move(selected.error()));
      if (*selected)
        return widget->setSelectedWidget(*selected);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("clearListSelected", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<ListWidget>(parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<void, StarException> {
      return widget->clearSelected();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback(
        "registerMemberCallback",
        [parentWidget](LuaEngine& engine, String const& widgetName, String const& name,
                       LuaFunction callback) -> std::expected<void, StarException> {
          return withWidget<ListWidget>(
            parentWidget, widgetName, [&](std::shared_ptr<ListWidget> widget) -> std::expected<void, StarException> {
              auto scoped = std::make_shared<ScopedMemberCallback>(
                std::move(callback), engine.promiseLifetime(), LuaBaseComponent::activeComponent());
              widget->registerMemberCallback(
                name, [scoped = std::move(scoped)](Widget& member) -> std::expected<void, StarException> {
                  if (scoped->lifetime.stop_requested() || !scoped->callback)
                    return {};
                  // The call may retire its own group and release the stored reference.
                  auto callback = *scoped->callback;
                  if (scoped->issuer) {
                    auto result = recoverLuaScriptError(
                      scoped->issuer->get().invokeCallback<LuaValue>(callback, member.name(), member.data()));
                    if (!result)
                      return std::unexpected(std::move(result.error()));
                    return {};
                  }
                  return callback.invoke<void>(member.name(), member.data());
                });
              return {};
            });
        });
      !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("refreshLayout", [parentWidget](String const& widgetName) -> std::expected<void, StarException> {
    return withWidget<Layout>(parentWidget, widgetName, [](std::shared_ptr<Layout> widget) {
      return widget->update(0);
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImageDrawables", [parentWidget](String const& widgetName, List<Drawable> drawables, std::optional<Vec2F> scale) -> std::expected<void, StarException> {
    return withWidget<ImageWidget>(parentWidget, widgetName, [&](std::shared_ptr<ImageWidget> widget) -> std::expected<void, StarException> {
      if (scale) Drawable::scaleAll(drawables, *scale);
      return widget->setDrawables(std::move(drawables));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setItemGridItems", [parentWidget](String const& widgetName, JsonArray const& items, std::optional<std::size_t> size) -> std::expected<void, StarException> {
    return withWidget<ItemGridWidget>(parentWidget, widgetName, [&](std::shared_ptr<ItemGridWidget> widget) -> std::expected<void, StarException> {
      auto database = widget->context().root().itemDatabase();
      if (!database) return std::unexpected(std::move(database.error()));
      auto bag = std::make_shared<ItemBag>(widget->context().root(), size.value_or(widget->itemSlots()));
      for (auto const& descriptor : items) {
        auto item = (*database)->fromJson(descriptor);
        if (!item) return std::unexpected(std::move(item.error()));
        auto added = bag->addItems(std::move(*item));
        if (!added) return std::unexpected(std::move(added.error()));
      }
      return widget->setItemBag(std::move(bag));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("itemGridItems", [parentWidget](String const& widgetName) -> std::expected<Json, StarException> {
    return withWidget<ItemGridWidget>(parentWidget, widgetName, [&](std::shared_ptr<ItemGridWidget> widget) -> std::expected<Json, StarException> {
      return widget->bag()->toJson();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("itemSlotItem", [parentWidget](String const& widgetName) -> std::expected<std::optional<Json>, StarException> {
    return withWidget<ItemSlotWidget>(parentWidget, widgetName, [&](std::shared_ptr<ItemSlotWidget> widget) -> std::expected<std::optional<Json>, StarException> {
      if (auto item = widget->item())
        return item->descriptor().toJson();
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setItemSlotHandle", [parentWidget](String const& name, std::optional<LuaUserData> const& handle) -> std::expected<void, StarException> {
    return withWidget<ItemSlotWidget>(parentWidget, name, [&](std::shared_ptr<ItemSlotWidget> slot) -> std::expected<void, StarException> {
      std::shared_ptr<Item> item;
      if (handle) {
        auto value = handle->get<std::shared_ptr<Item>>();
        if (!value) return std::unexpected(std::move(value.error()));
        item = value->get();
      }
      slot->setItem(item);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setItemSlotDisplay", [parentWidget](String const& name, Json const& display) -> std::expected<void, StarException> {
    return withWidget<ItemSlotWidget>(parentWidget, name, [&](std::shared_ptr<ItemSlotWidget> slot) -> std::expected<void, StarException> {
      static auto const fields = std::array{std::pair{String("durability"), &ItemSlotWidget::showDurability},
          std::pair{String("count"), &ItemSlotWidget::showCount}, std::pair{String("secondaryIcon"), &ItemSlotWidget::showSecondaryIcon},
          std::pair{String("highlight"), &ItemSlotWidget::setHighlightEnabled}};
      for (auto const& [field, setter] : fields) {
        auto found = display.contains(field);
        if (!found) return std::unexpected(std::move(found.error()));
        if (!*found) continue;
        auto enabled = display.getBool(field);
        if (!enabled) return std::unexpected(std::move(enabled.error()));
        std::invoke(setter, *slot, *enabled);
      }
      auto hasCountPosition = display.contains("countPosition");
      if (!hasCountPosition) return std::unexpected(std::move(hasCountPosition.error()));
      if (*hasCountPosition) {
        auto config = display.get("countPosition");
        if (!config) return std::unexpected(std::move(config.error()));
        auto position = TextPositioning::create(*config);
        if (!position) return std::unexpected(std::move(position.error()));
        slot->setCountPosition(*position);
      }
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setItemSlotItem", [parentWidget](String const& widgetName, Json const& config) -> std::expected<void, StarException> {
    return withWidget<ItemSlotWidget>(parentWidget, widgetName, [&](std::shared_ptr<ItemSlotWidget> widget) -> std::expected<void, StarException> {
      auto database = widget->context().root().itemDatabase();
      if (!database)
        return std::unexpected(std::move(database.error()));
      auto item = (*database)->fromJson(config);
      if (!item)
        return std::unexpected(std::move(item.error()));
      widget->setItem(*item);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setItemSlotProgress", [parentWidget](String const& widgetName, float progress) -> std::expected<void, StarException> {
    return withWidget<ItemSlotWidget>(parentWidget, widgetName, [&](std::shared_ptr<ItemSlotWidget> widget) -> std::expected<void, StarException> {
      widget->setProgress(progress);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("addFlowImage", [parentWidget](String const& widgetName, String const& name, String const& image) -> std::expected<void, StarException> {
    return withWidget<FlowLayout>(parentWidget, widgetName, [&](std::shared_ptr<FlowLayout> widget) -> std::expected<void, StarException> {
      auto child = ImageWidget::create(widget->context(), image);
      if (!child)
        return std::unexpected(std::move(child.error()));
      return widget->addChild(name, std::move(*child));
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setImageStretchSet", [parentWidget](String const& widgetName, Json const& images) -> std::expected<void, StarException> {
    return withWidget<ImageStretchWidget>(parentWidget, widgetName, [&](std::shared_ptr<ImageStretchWidget> widget) -> std::expected<void, StarException> {
      auto begin = images.getString("begin", "");
      if (!begin)
        return std::unexpected(std::move(begin.error()));
      auto inner = images.getString("inner", "");
      if (!inner)
        return std::unexpected(std::move(inner.error()));
      auto end = images.getString("end", "");
      if (!end)
        return std::unexpected(std::move(end.error()));
      widget->setImageStretchSet(*begin, *inner, *end);
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getScrollOffset", [parentWidget](String const& widgetName) -> std::expected<std::optional<Vec2I>, StarException> {
    return withWidget<ScrollArea>(parentWidget, widgetName, [&](std::shared_ptr<ScrollArea> widget) -> std::expected<std::optional<Vec2I>, StarException> {
      return widget->scrollOffset();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("setScrollOffset", [parentWidget](String const& widgetName, Vec2I const& offset) -> std::expected<void, StarException> {
    return withWidget<ScrollArea>(parentWidget, widgetName, [&](std::shared_ptr<ScrollArea> widget) -> std::expected<void, StarException> {
      widget->scrollAreaBy(offset - widget->scrollOffset());
      return {};
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  if (auto registered = callbacks.registerCallback("getMaxScrollPosition", [parentWidget](String const& widgetName) -> std::expected<std::optional<Vec2I>, StarException> {
    return withWidget<ScrollArea>(parentWidget, widgetName, [&](std::shared_ptr<ScrollArea> widget) -> std::expected<std::optional<Vec2I>, StarException> {
      return widget->maxScrollPosition();
    });
  }); !registered)
    return std::unexpected(std::move(registered.error()));
  return callbacks;
}

}

}
