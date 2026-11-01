import std;
import star.chat_bubble_separation;
import star.format;
import star.game_test;
import star.input_event;
import star.json;
import star.key_bindings;
import star.list;
import star.rect;
import star.vector;
import star.set;
import star.string;
import star.root;
import star.configuration;
import star.gui_context;
import star.widget;
import star.button_widget;
import star.button_group;
import star.text_box_widget;
import star.base_script_pane;
import star.lua;
import star.lua_root;
import star.lua_components;
import star.lua_converters;
import star.lua_game_converters;
import star.map;
import star.pane;
import star.pane_manager;
import star.main_interface;
import star.assets;
import star.player_universe_map;
import star.uuid;
import star.game_types;
import star.file;
import star.random;
import star.algorithm;
import star.list_widget;
import star.songbook;
import star.songbook_lua_bindings;
import star.root_lua_bindings;
import star.player;
import star.player_factory;
import star.player_codexes;
import star.codex;
import star.player_lua_bindings;
import star.quests;
import star.quest_manager;
import star.quest_descriptor;
import star.versioning_database;
import star.image_widget;
import star.image_stretch_widget;
import star.progress_widget;
import star.math_common;
import star.item;
import star.item_database;
import star.drawable;
import star.cinematic;
import star.world_server;
import star.casting;
import star.vertical_layout;
import star.logging;
import star.game_timers;
import star.world_template;
import star.world_lua_bindings;
import star.celestial_parameters;
import star.celestial_coordinate;
import star.canvas_widget;
import star.camera_lua_bindings;
import star.widget_lua_bindings;
import star.world_camera;
import star.poly;
import star.json_extra;

#include "gtest/gtest.h"
#include "gtest/gtest-spi.h"

using namespace Star;

namespace {

std::optional<RectF> separateAgainst(List<RectF> const& obstacles, RectF box) {
  BubbleSeparator<int> separator(1.0f);
  int id = 0;
  for (auto const& obstacle : obstacles)
    separator.addBubble(obstacle.center(), obstacle, id++);
  auto prepared = separator.forEach([](BubbleState<int>&, int&) -> std::expected<void, StarException> {
    return {};
  });
  EXPECT_TRUE(prepared);
  separator.addBubble(box.center(), box, -1);
  auto matches = separator.filtered([](BubbleState<int> const&, int id) { return id == -1; });
  EXPECT_EQ(matches.size(), 1u);
  std::optional<RectF> result;
  for (auto const& match : matches)
    result = match.separatedBox;
  return result;
}

TEST(ChatBubbleSeparation, ContainingProjectionsNeedBothEdgeQueries) {
  RectF box{0, 2, 10, 8};
  RectF containing{-100, 0, 100, 10};
  // A nonoverlapping neighbor hides the containing rectangle in one ordering.
  EXPECT_EQ(separateAgainst({containing, {-20, 20, -10, 30}}, box), (RectF{0, 10, 10, 16}));
  EXPECT_EQ(separateAgainst({containing, {20, 20, 30, 30}}, box), (RectF{0, 10, 10, 16}));
}

TEST(ChatBubbleSeparation, EqualSizesRespectVerticalPriorityAndTouchingEdges) {
  RectF box{0, 0, 10, 10};
  EXPECT_EQ(separateAgainst({{0, 5, 10, 15}}, box), box);
  EXPECT_EQ(separateAgainst({{0, -5, 10, 5}}, box), (RectF{0, 10, 10, 20}));
  EXPECT_EQ(separateAgainst({box}, box), (RectF{0, 10, 10, 20}));
  EXPECT_EQ(separateAgainst({{10, 0, 20, 10}, {0, 10, 10, 20}}, box), box);
}

TEST(ChatBubbleSeparation, MixedSizesChainWithoutChangingDimensions) {
  RectF box{0, 0, 10, 10};
  auto result = separateAgainst({{-5, 0, 15, 10}, {-2, 12, 13, 22}, {0, 24, 10, 44}}, RectF{0, 4, 10, 14});
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, (RectF{0, 44, 10, 54}));
  EXPECT_EQ(result->size(), box.size());
  EXPECT_EQ(separateAgainst({{1, -5, 9, 50}}, box), box);
}

const GameTestRegistration keybindingSpecificity("KeyBindings", "MostSpecificModifiersAndEventTypes", [](GameTest& fixture) {
  auto config = Json::parseJson(R"JSON({
    "PlayerLeft": [{"type":"key","value":"A","mods":[]}],
    "PlayerRight": [{"type":"key","value":"A","mods":["LShift"]}],
    "PlayerUp": [{"type":"key","value":"A","mods":["LCtrl"]}],
    "PlayerDown": [{"type":"key","value":"A","mods":["LShift","LCtrl"]}],
    "PlayerJump": [{"type":"key","value":"A","mods":["LShift","LCtrl"]}]
  })JSON");
  ASSERT_TRUE(config) << config.error().what();
  auto bindings = KeyBindings::create(fixture.root(), *config);
  ASSERT_TRUE(bindings) << bindings.error().what();
  EXPECT_EQ(bindings->actions(Key::A), (Set<InterfaceAction>{InterfaceAction::PlayerLeft}));
  EXPECT_EQ(bindings->actions(KeyChord{Key::A, KeyMod::LShift}), (Set<InterfaceAction>{InterfaceAction::PlayerRight}));
  EXPECT_EQ(bindings->actions(KeyChord{Key::A, KeyMod::LCtrl}), (Set<InterfaceAction>{InterfaceAction::PlayerUp}));
  auto both = KeyMod::LShift | KeyMod::LCtrl;
  EXPECT_EQ(bindings->actions(KeyChord{Key::A, both}),
      (Set<InterfaceAction>{InterfaceAction::PlayerDown, InterfaceAction::PlayerJump}));
  EXPECT_EQ(bindings->actions(InputEvent{KeyDownEvent{Key::A, both | KeyMod::LAlt}}),
      (Set<InterfaceAction>{InterfaceAction::PlayerDown, InterfaceAction::PlayerJump}));
  EXPECT_TRUE(bindings->actions(InputEvent{KeyUpEvent{Key::A}}).empty());
  EXPECT_TRUE(bindings->actions(Key::B).empty());
  EXPECT_TRUE(bindings->actionsForKey(Key::B).empty());
  EXPECT_EQ(bindings->actionsForKey(Key::A), (Set<InterfaceAction>{InterfaceAction::PlayerLeft,
      InterfaceAction::PlayerRight, InterfaceAction::PlayerUp, InterfaceAction::PlayerDown, InterfaceAction::PlayerJump}));

  auto tiedConfig = Json::parseJson(R"JSON({
    "PlayerRight": [{"type":"key","value":"A","mods":["LShift"]}],
    "PlayerUp": [{"type":"key","value":"A","mods":["LCtrl"]}]
  })JSON");
  ASSERT_TRUE(tiedConfig) << tiedConfig.error().what();
  auto tied = KeyBindings::create(fixture.root(), *tiedConfig);
  ASSERT_TRUE(tied) << tied.error().what();
  EXPECT_EQ(tied->actions(KeyChord{Key::A, both}),
      (Set<InterfaceAction>{InterfaceAction::PlayerRight, InterfaceAction::PlayerUp}));
});

const GameTestRegistration widgetParentLifetime("WidgetBorrow", "ReparentAndDestructionDetachEscapedChild", [](GameTest& fixture) {
  auto configuration = fixture.root().configuration();
  ASSERT_TRUE(configuration);
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{}));
  auto context = GuiContext::create(fixture.root(), {}, {});
  ASSERT_TRUE(context) << context.error().what();

  auto first = std::make_shared<Widget>(**context);
  auto second = std::make_shared<Widget>(**context);
  auto child = std::make_shared<Widget>(**context);
  ASSERT_TRUE(first->addChild("old", child));
  ASSERT_TRUE(second->addChild("new", child));
  auto oldName = first->containsChild("old");
  ASSERT_TRUE(oldName);
  EXPECT_FALSE(*oldName);
  EXPECT_EQ(second->childPtr(*child), child);
  ASSERT_TRUE(child->parent());
  EXPECT_EQ(child->parent()->get().childPtr(*child), child);

  first.reset();
  EXPECT_TRUE(child->parent());
  second.reset();
  EXPECT_FALSE(child->parent());
  EXPECT_FALSE(child->window());
});

const GameTestRegistration buttonGroupLifetime("WidgetBorrow", "ButtonDestructionRemovesWeakGroupEntries", [](GameTest& fixture) {
  auto configuration = fixture.root().configuration();
  ASSERT_TRUE(configuration);
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{}));
  auto context = GuiContext::create(fixture.root(), {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto first = ButtonWidget::create(**context);
  auto second = ButtonWidget::create(**context);
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(second) << second.error().what();
  auto group = std::make_shared<ButtonGroup>();
  (*first)->setButtonGroup(group, 10);
  (*second)->setButtonGroup(group, 20);
  ASSERT_TRUE(group->select(10));
  EXPECT_TRUE((*first)->isChecked());
  EXPECT_FALSE((*second)->isChecked());

  first->reset();
  EXPECT_FALSE(group->button(10));
  EXPECT_EQ(1u, group->buttonCount());
  EXPECT_EQ(group->checkedId(), ButtonGroup::NoButton);
  ASSERT_TRUE(group->select(20));
  EXPECT_TRUE((*second)->isChecked());
  second->reset();
  EXPECT_TRUE(group->buttons().empty());
  EXPECT_EQ(0u, group->buttonCount());
  EXPECT_EQ(group->checkedId(), ButtonGroup::NoButton);
});

std::expected<std::shared_ptr<GuiContext>, StarException> textEditorContext(GameTest& fixture) {
  auto configuration = fixture.root().configuration();
  if (!configuration)
    return std::unexpected(std::move(configuration.error()));
  auto configured = (*configuration)->set("bindings", JsonObject{});
  if (!configured)
    return std::unexpected(std::move(configured.error()));
  return GuiContext::create(fixture.root(), {}, {});
}

std::expected<std::shared_ptr<TextBoxWidget>, StarException> textEditor(GuiContext& context, String const& text) {
  return TextBoxWidget::create(context, text, "", [](Widget&) -> std::expected<void, StarException> {
    return {};
  });
}

void sendEditorEvent(TextBoxWidget& editor, InputEvent const& event) {
  auto handled = editor.sendEvent(event);
  ASSERT_TRUE(handled) << handled.error().what();
  EXPECT_TRUE(*handled);
}

const GameTestRegistration textEditorUnicode("TextEditor", "CommittedUnicodeReplacesSelectionWithoutDrawing", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto created = textEditor(**context, "A中🙂Z");
  ASSERT_TRUE(created) << created.error().what();
  auto& editor = **created;
  editor.setEditorRendering(false);
  ASSERT_TRUE(editor.focus());
  auto capture = editor.keyboardCaptureMode();
  ASSERT_TRUE(capture) << capture.error().what();
  EXPECT_EQ(*capture, KeyboardCaptureMode::TextInput);
  EXPECT_EQ(editor.getCursorPosition(), 4);

  // Reversed endpoints are codepoint positions, not UTF-8 byte offsets.
  ASSERT_TRUE(editor.setSelection(3, 1));
  sendEditorEvent(editor, TextInputEvent{"é界"});
  EXPECT_EQ(editor.getText(), String("Aé界Z"));
  EXPECT_EQ(editor.getCursorPosition(), 3);
  // A second commit inserts rather than replacing the former selection.
  sendEditorEvent(editor, TextInputEvent{"🙂"});
  EXPECT_EQ(editor.getText(), String("Aé界🙂Z"));
  EXPECT_EQ(editor.getCursorPosition(), 4);
});

const GameTestRegistration textEditorSelectionDelete("TextEditor", "RealShiftNavigationAndBothDeletionKeys", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto created = textEditor(**context, "A中🙂Z");
  ASSERT_TRUE(created) << created.error().what();
  auto& editor = **created;
  ASSERT_TRUE(editor.focus());
  sendEditorEvent(editor, KeyDownEvent{Key::Home, KeyMod::NoMod});
  sendEditorEvent(editor, KeyDownEvent{Key::Right, KeyMod::NoMod});
  sendEditorEvent(editor, KeyDownEvent{Key::Right, KeyMod::LShift});
  sendEditorEvent(editor, KeyDownEvent{Key::Right, KeyMod::LShift});
  sendEditorEvent(editor, KeyDownEvent{Key::Delete, KeyMod::NoMod});
  EXPECT_EQ(editor.getText(), String("AZ"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  sendEditorEvent(editor, TextInputEvent{"界"});
  EXPECT_EQ(editor.getText(), String("A界Z"));

  auto reset = editor.setText("A中🙂Z");
  ASSERT_TRUE(reset) << reset.error().what();
  ASSERT_TRUE(*reset);
  ASSERT_TRUE(editor.setSelection(3, 1));
  sendEditorEvent(editor, KeyDownEvent{Key::Backspace, KeyMod::NoMod});
  EXPECT_EQ(editor.getText(), String("AZ"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  sendEditorEvent(editor, TextInputEvent{"🙂"});
  EXPECT_EQ(editor.getText(), String("A🙂Z"));
});

const GameTestRegistration textEditorRejectedEdit("TextEditor", "RejectedInputAndDeletionRetainSelection", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto created = textEditor(**context, "A中🙂Z");
  ASSERT_TRUE(created) << created.error().what();
  auto& editor = **created;
  ASSERT_TRUE(editor.focus());
  editor.setRegex("[A-Z中🙂]*");
  ASSERT_TRUE(editor.setSelection(3, 1));
  sendEditorEvent(editor, TextInputEvent{"1"});
  EXPECT_EQ(editor.getText(), String("A中🙂Z"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  // This succeeds only if the rejected commit retained both selection endpoints.
  sendEditorEvent(editor, TextInputEvent{"Q"});
  EXPECT_EQ(editor.getText(), String("AQZ"));
  EXPECT_EQ(editor.getCursorPosition(), 2);

  editor.setRegex("[A-Z中🙂]{3}");
  ASSERT_TRUE(editor.setSelection(1, 2));
  sendEditorEvent(editor, KeyDownEvent{Key::Delete, KeyMod::NoMod});
  EXPECT_EQ(editor.getText(), String("AQZ"));
  EXPECT_EQ(editor.getCursorPosition(), 2);
  // A rejected deletion must not turn this replacement into an insertion.
  sendEditorEvent(editor, TextInputEvent{"中"});
  EXPECT_EQ(editor.getText(), String("A中Z"));
  EXPECT_EQ(editor.getCursorPosition(), 2);

  // A rejected whole-field removal must not create a phantom selection.
  // The renderer-free fixture has no platform clipboard controller.
  editor.setRegex("[A-Z中🙂]{3,4}");
  ASSERT_TRUE(editor.setCursorPosition(1));
  auto rejected = editor.setText("");
  ASSERT_TRUE(rejected) << rejected.error().what();
  EXPECT_FALSE(*rejected);
  EXPECT_EQ(editor.getText(), String("A中Z"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  sendEditorEvent(editor, TextInputEvent{"Q"});
  EXPECT_EQ(editor.getText(), String("AQ中Z"));
  EXPECT_EQ(editor.getCursorPosition(), 2);
});

const GameTestRegistration textEditorLegacy("TextEditor", "UnselectedEditingFocusAndSubmitRemainUnchanged", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto created = textEditor(**context, "ab");
  ASSERT_TRUE(created) << created.error().what();
  auto& editor = **created;
  auto unfocused = editor.sendEvent(TextInputEvent{"ignored"});
  ASSERT_TRUE(unfocused) << unfocused.error().what();
  EXPECT_FALSE(*unfocused);
  EXPECT_EQ(editor.getText(), String("ab"));
  ASSERT_TRUE(editor.focus());

  sendEditorEvent(editor, TextInputEvent{"中"});
  sendEditorEvent(editor, KeyDownEvent{Key::Left, KeyMod::NoMod});
  sendEditorEvent(editor, KeyDownEvent{Key::Backspace, KeyMod::NoMod});
  EXPECT_EQ(editor.getText(), String("a中"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  sendEditorEvent(editor, KeyDownEvent{Key::Delete, KeyMod::NoMod});
  EXPECT_EQ(editor.getText(), String("a"));
  EXPECT_EQ(editor.getCursorPosition(), 1);
  sendEditorEvent(editor, KeyDownEvent{Key::Home, KeyMod::NoMod});
  sendEditorEvent(editor, TextInputEvent{"b"});
  EXPECT_EQ(editor.getText(), String("ba"));
  sendEditorEvent(editor, KeyDownEvent{Key::End, KeyMod::NoMod});
  EXPECT_EQ(editor.getCursorPosition(), 2);

  int submitted = 0;
  editor.setOnEnterKeyCallback([&](Widget&) -> std::expected<void, StarException> {
    ++submitted;
    return {};
  });
  sendEditorEvent(editor, KeyDownEvent{Key::Return, KeyMod::NoMod});
  EXPECT_EQ(submitted, 1);
  EXPECT_EQ(editor.getText(), String("ba"));
  sendEditorEvent(editor, KeyDownEvent{Key::Return, KeyMod::LShift});
  EXPECT_EQ(submitted, 1);
  EXPECT_EQ(editor.getText(), String("ba\n"));
  EXPECT_EQ(editor.getCursorPosition(), 3);
});

class ScriptTestPane : public BaseScriptPane {
public:
  static std::expected<std::shared_ptr<ScriptTestPane>, StarException> create(GuiContext& context, bool interactive = true) {
    return create(context, JsonObject{{"gui", JsonObject{}}, {"interactive", interactive}});
  }
  static std::expected<std::shared_ptr<ScriptTestPane>, StarException> create(GuiContext& context, Json config) {
    auto pane = std::shared_ptr<ScriptTestPane>(new ScriptTestPane(context));
    auto configured = pane->init(std::move(config));
    if (!configured)
      return std::unexpected(std::move(configured.error()));
    return pane;
  }
  std::expected<std::optional<Json>, StarException> evaluate(String const& code) {
    return m_script.eval<Json>(code);
  }
private:
  explicit ScriptTestPane(GuiContext& context) : BaseScriptPane(context) {}
};

const GameTestRegistration scriptStorage("ScriptPane", "StorageSurvivesLifecycleReloadAndEngineReplacement", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  auto& pane = **created;
  ASSERT_TRUE(pane.setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane.displayed());
  ASSERT_TRUE(pane.evaluate("storage.count = 17; storage.events = jarray(); table.insert(storage.events, false)"));
  ASSERT_TRUE(pane.dismissed());
  ASSERT_TRUE(pane.displayed());
  auto saved = pane.evaluate("return {storage.count, #storage.events, storage.events[1]}");
  ASSERT_TRUE(saved) << saved.error().what();
  ASSERT_TRUE(*saved);
  EXPECT_EQ(**saved, (Json(JsonArray{17, 1, false})));

  ASSERT_TRUE(fixture.root().reload());
  auto reloaded = pane.evaluate("return {storage.count, #storage.events, storage.events[1]}");
  ASSERT_TRUE(reloaded) << reloaded.error().what();
  ASSERT_TRUE(*reloaded);
  EXPECT_EQ(**reloaded, **saved);
  auto previousGeneration = (*luaRoot)->generation();
  ASSERT_TRUE((*luaRoot)->restart());
  EXPECT_GT((*luaRoot)->generation(), previousGeneration);
  auto restarted = pane.evaluate("return {storage.count, #storage.events, storage.events[1]}");
  ASSERT_TRUE(restarted) << restarted.error().what();
  ASSERT_TRUE(*restarted);
  EXPECT_EQ(**restarted, **saved);
  ASSERT_TRUE(pane.dismissed());
  auto stopped = pane.call("missing", {});
  ASSERT_TRUE(stopped) << stopped.error().what();
  EXPECT_FALSE(*stopped);
});

const GameTestRegistration
  canvasSessionLifetime("ScriptPane", "CanvasCapabilitiesRetireWithoutChangingSurfaceOwnership", [](GameTest& fixture) {
    auto context = textEditorContext(fixture);
    ASSERT_TRUE(context) << context.error().what();
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot) << luaRoot.error().what();
    auto created = ScriptTestPane::create(**context);
    ASSERT_TRUE(created) << created.error().what();
    auto pane = std::move(*created);
    ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
    auto surface = CanvasWidget::create(**context);
    ASSERT_TRUE(surface) << surface.error().what();
    (*surface)->setSize({31, 17});
    std::weak_ptr<CanvasWidget> ownedSurface = *surface;
    ASSERT_TRUE(pane->addChild("surface", *surface));
    List<LuaFunction> retained;
    LuaCallbacks probe;
    ASSERT_TRUE(probe.registerCallback("capture", [&](LuaFunction function) { retained.append(std::move(function)); }));
    ASSERT_TRUE(pane->addCallbacks("probe", std::move(probe)));
    ASSERT_TRUE(pane->displayed());
    String capture = R"LUA(
    local canvas = widget.bindCanvas("surface")
    assert(canvas.setPosition == nil and canvas.dismiss == nil)
    probe.capture(function() canvas:clear(); return canvas:size() end)
  )LUA";
    ASSERT_TRUE(pane->evaluate(capture));
    ASSERT_EQ(retained.size(), 1u);
    auto initial = retained[0].invoke<Vec2I>();
    ASSERT_TRUE(initial) << initial.error().what();
    EXPECT_EQ(*initial, (Vec2I{31, 17}));
    ASSERT_TRUE(pane->dismissed());
    EXPECT_FALSE(retained[0].invoke<Vec2I>());
    ASSERT_TRUE(pane->displayed());
    ASSERT_TRUE(pane->evaluate(capture));
    ASSERT_EQ(retained.size(), 2u);
    EXPECT_TRUE(retained[1].invoke<Vec2I>());
    EXPECT_FALSE(retained[0].invoke<Vec2I>());

    ASSERT_TRUE(fixture.root().reload());
    ASSERT_TRUE(pane->evaluate(capture));
    ASSERT_EQ(retained.size(), 3u);
    EXPECT_FALSE(retained[1].invoke<Vec2I>());
    EXPECT_TRUE(retained[2].invoke<Vec2I>());
    auto removed = pane->removeCallbacks("widget");
    ASSERT_TRUE(removed) << removed.error().what();
    EXPECT_TRUE(*removed);
    EXPECT_FALSE(retained[2].invoke<Vec2I>());
    auto callbacks = LuaBindings::makeWidgetCallbacks(**context, *pane);
    ASSERT_TRUE(callbacks) << callbacks.error().what();
    ASSERT_TRUE(pane->addCallbacks("widget", std::move(*callbacks)));
    ASSERT_TRUE(pane->evaluate(capture));
    ASSERT_EQ(retained.size(), 4u);
    EXPECT_TRUE(retained[3].invoke<Vec2I>());

    // Lua still owns the surface after its native parent releases it.
    EXPECT_TRUE(pane->removeChild("surface"));
    surface->reset();
    EXPECT_FALSE(ownedSurface.expired());
    EXPECT_TRUE(retained[3].invoke<Vec2I>());
    pane.reset();
    EXPECT_FALSE(retained[3].invoke<Vec2I>());
    retained.clear();
    ASSERT_TRUE((*luaRoot)->collectGarbage());
    EXPECT_TRUE(ownedSurface.expired());
  });

const GameTestRegistration
  canvasHostLifetime("WidgetBorrow", "CanvasRejectsDestroyedOwnerAndRetiredInterfaceHost", [](GameTest& fixture) {
    auto context = textEditorContext(fixture);
    ASSERT_TRUE(context) << context.error().what();
    auto engine = LuaEngine::create();
    ASSERT_TRUE(engine) << engine.error().what();
    auto lua = (*engine)->createContext();
    ASSERT_TRUE(lua) << lua.error().what();
    auto owner = std::make_shared<Widget>(**context);
    auto surface = CanvasWidget::create(**context);
    ASSERT_TRUE(surface) << surface.error().what();
    ASSERT_TRUE(owner->addChild("surface", *surface));
    auto callbacks = LuaBindings::makeWidgetCallbacks(**context, *owner);
    ASSERT_TRUE(callbacks) << callbacks.error().what();
    ASSERT_TRUE(lua->setCallbacks("widget", *callbacks));
    auto retained =
      lua->eval<LuaFunction>("local canvas = widget.bindCanvas('surface'); return function() return canvas:size() end");
    ASSERT_TRUE(retained) << retained.error().what();
    EXPECT_TRUE(retained->invoke<Vec2I>());
    owner.reset();
    EXPECT_FALSE(retained->invoke<Vec2I>());

    std::stop_source host;
    auto interfaceCanvas = (*engine)->createUserData(LuaCanvas{*surface, {}, host.get_token(), {}});
    ASSERT_TRUE(interfaceCanvas) << interfaceCanvas.error().what();
    ASSERT_TRUE(lua->set("surface", *interfaceCanvas));
    auto beforeStop = lua->eval<Json>("return surface:size()");
    ASSERT_TRUE(beforeStop) << beforeStop.error().what();
    ASSERT_TRUE(host.request_stop());
    EXPECT_FALSE(lua->eval<Json>("return surface:size()"));
  });

const GameTestRegistration widgetMemberLifetime(
  "ScriptPane", "RetiringMemberCallbacksReleasesLuaRootsAndLeavesOldRowsInert", [](GameTest& fixture) {
    auto context = textEditorContext(fixture);
    ASSERT_TRUE(context) << context.error().what();
    auto luaRoot = LuaRoot::create(fixture.root());
    ASSERT_TRUE(luaRoot) << luaRoot.error().what();
    auto created = ScriptTestPane::create(**context);
    ASSERT_TRUE(created) << created.error().what();
    auto pane = std::move(*created);
    ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
    auto schema = Json::parseJson(R"JSON({
    "spacing":[0,0], "memberSize":[80,12],
    "listTemplate":{"edit":{"type":"textbox","position":[0,0],"maxWidth":-1}}
  })JSON");
    ASSERT_TRUE(schema) << schema.error().what();
    auto list = ListWidget::create(**context, *schema);
    ASSERT_TRUE(list) << list.error().what();
    ASSERT_TRUE(pane->addChild("list", *list));
    int calls = 0;
    std::weak_ptr<int> sentinel;
    LuaCallbacks probe;
    ASSERT_TRUE(probe.registerCallback("sentinel", [&](LuaEngine& engine) -> std::expected<LuaUserData, StarException> {
      auto value = std::make_shared<int>(0);
      sentinel = value;
      return engine.createUserData(std::move(value));
    }));
    ASSERT_TRUE(probe.registerCallback("called", [&]() { ++calls; }));
    ASSERT_TRUE(probe.registerCallback("retire", [&]() -> std::expected<void, StarException> {
      auto removed = pane->removeCallbacks("widget");
      if (!removed)
        return std::unexpected(std::move(removed.error()));
      return {};
    }));
    ASSERT_TRUE(pane->addCallbacks("probe", std::move(probe)));
    ASSERT_TRUE(pane->displayed());
    String registerMember = R"LUA(
    local sentinel = probe.sentinel()
    widget.registerMemberCallback("list", "edit", function()
      assert(sentinel ~= nil)
      probe.called()
    end)
  )LUA";
    ASSERT_TRUE(pane->evaluate(registerMember));
    auto row = (*list)->addItem();
    ASSERT_TRUE(row) << row.error().what();
    auto oldEditor = (*row)->fetchChild<TextBoxWidget>("edit");
    ASSERT_TRUE(oldEditor) << oldEditor.error().what();
    ASSERT_TRUE(*oldEditor);
    ASSERT_TRUE((*oldEditor)->setText("active"));
    EXPECT_EQ(calls, 1);
    ASSERT_TRUE(pane->dismissed());
    ASSERT_TRUE((*luaRoot)->collectGarbage());
    EXPECT_TRUE(sentinel.expired());
    ASSERT_TRUE((*oldEditor)->setText("retired"));
    EXPECT_EQ(calls, 1);

    ASSERT_TRUE(pane->displayed());
    ASSERT_TRUE(pane->evaluate(registerMember));
    auto freshRow = (*list)->addItem();
    ASSERT_TRUE(freshRow) << freshRow.error().what();
    auto freshEditor = (*freshRow)->fetchChild<TextBoxWidget>("edit");
    ASSERT_TRUE(freshEditor) << freshEditor.error().what();
    ASSERT_TRUE(*freshEditor);
    ASSERT_TRUE((*freshEditor)->setText("fresh"));
    EXPECT_EQ(calls, 2);
    ASSERT_TRUE((*oldEditor)->setText("still retired"));
    EXPECT_EQ(calls, 2);
    ASSERT_TRUE(pane->evaluate(R"LUA(
    widget.registerMemberCallback("list", "edit", function()
      probe.called()
      probe.retire()
    end)
  )LUA"));
    auto retiringRow = (*list)->addItem();
    ASSERT_TRUE(retiringRow) << retiringRow.error().what();
    auto retiringEditor = (*retiringRow)->fetchChild<TextBoxWidget>("edit");
    ASSERT_TRUE(retiringEditor) << retiringEditor.error().what();
    ASSERT_TRUE(*retiringEditor);
    ASSERT_TRUE((*retiringEditor)->setText("retire inside callback"));
    EXPECT_EQ(calls, 3);
    ASSERT_TRUE((*retiringEditor)->setText("retired inside callback"));
    EXPECT_EQ(calls, 3);
    ASSERT_TRUE((*luaRoot)->collectGarbage());
    EXPECT_TRUE(sentinel.expired());
    ASSERT_TRUE(pane->dismissed());
  });

const GameTestRegistration componentCallbackOwnership("ScriptPane", "ComponentCallbacksReplaceInheritedTargetsWithoutRevivingRetiredOnes", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  std::stop_source previousLifetime;
  LuaCallbacks previous;
  ASSERT_TRUE(previous.registerCallback("session",
      [lifetime = previousLifetime.get_token()]() -> std::expected<String, StarException> {
    if (lifetime.stop_requested())
      return std::unexpected(StarException("Previous interface is detached"));
    return String("previous");
  }));
  (*luaRoot)->addCallbacks("interface", previous);
  (*luaRoot)->addCallbacks("world", previous);
  (*luaRoot)->addCallbacks("inheritedOnly", previous);

  LuaBaseComponent first(fixture.root());
  ASSERT_TRUE(first.addCallbacks("interface", previous));
  ASSERT_TRUE(first.setLuaRoot(*luaRoot));
  ASSERT_TRUE(first.init());
  auto firstSession = first.eval<String>("return interface.session()");
  ASSERT_TRUE(firstSession) << firstSession.error().what();
  ASSERT_TRUE(*firstSession);
  EXPECT_EQ(**firstSession, String("previous"));
  auto retainedSession = first.context()->getPath<LuaFunction>("interface.session");
  ASSERT_TRUE(retainedSession) << retainedSession.error().what();
  ASSERT_TRUE(first.uninit());
  ASSERT_TRUE(previousLifetime.request_stop());
  auto retired = retainedSession->invoke<String>();
  ASSERT_FALSE(retired);

  std::stop_source currentLifetime;
  LuaCallbacks current;
  ASSERT_TRUE(current.registerCallback("session",
      [lifetime = currentLifetime.get_token()]() -> std::expected<String, StarException> {
    if (lifetime.stop_requested())
      return std::unexpected(StarException("Current interface is detached"));
    return String("current");
  }));
  LuaBaseComponent second(fixture.root());
  ASSERT_TRUE(second.addCallbacks("interface", current));
  ASSERT_TRUE(second.addCallbacks("world", LuaCallbacks{}));
  ASSERT_TRUE(second.setLuaRoot(*luaRoot));
  ASSERT_TRUE(second.init());
  auto secondSession = second.eval<String>("return interface.session()");
  ASSERT_TRUE(secondSession) << secondSession.error().what();
  ASSERT_TRUE(*secondSession);
  EXPECT_EQ(**secondSession, String("current"));
  auto unavailableWorld = second.eval<bool>("return type(world) == 'table' and world.session == nil");
  ASSERT_TRUE(unavailableWorld) << unavailableWorld.error().what();
  EXPECT_TRUE(unavailableWorld->value_or(false));
  auto inherited = second.eval<bool>("return not pcall(inheritedOnly.session)");
  ASSERT_TRUE(inherited) << inherited.error().what();
  EXPECT_TRUE(inherited->value_or(false));
  ASSERT_TRUE(second.init());
  auto reinitialized = second.eval<String>("return interface.session()");
  ASSERT_TRUE(reinitialized) << reinitialized.error().what();
  ASSERT_TRUE(*reinitialized);
  EXPECT_EQ(**reinitialized, String("current"));
  ASSERT_TRUE(second.uninit());

  // Adding an explicit group to an already-running component has the same
  // precedence, without altering inherited tables in sibling contexts.
  LuaBaseComponent live(fixture.root());
  ASSERT_TRUE(live.setLuaRoot(*luaRoot));
  ASSERT_TRUE(live.init());
  auto inheritedBeforeBinding = live.eval<bool>("return not pcall(interface.session)");
  ASSERT_TRUE(inheritedBeforeBinding) << inheritedBeforeBinding.error().what();
  EXPECT_TRUE(inheritedBeforeBinding->value_or(false));
  ASSERT_TRUE(live.addCallbacks("interface", current));
  auto liveSession = live.eval<String>("return interface.session()");
  ASSERT_TRUE(liveSession) << liveSession.error().what();
  ASSERT_TRUE(*liveSession);
  EXPECT_EQ(**liveSession, String("current"));
  ASSERT_TRUE(live.addCallbacks("world", LuaCallbacks{}));
  auto shadowedWorld = live.eval<bool>("return type(world) == 'table' and world.session == nil");
  ASSERT_TRUE(shadowedWorld) << shadowedWorld.error().what();
  EXPECT_TRUE(shadowedWorld->value_or(false));
  ASSERT_TRUE(live.uninit());

  auto rootContext = (*luaRoot)->createContext(StringList{});
  ASSERT_TRUE(rootContext) << rootContext.error().what();
  auto stillRetired = rootContext->eval<bool>("return not pcall(interface.session) and not pcall(world.session)");
  ASSERT_TRUE(stillRetired) << stillRetired.error().what();
  EXPECT_TRUE(*stillRetired);
  auto retainedStillRetired = retainedSession->invoke<String>();
  EXPECT_FALSE(retainedStillRetired);
});

class ReentrantReloadScript : public LuaBaseComponent {
public:
  explicit ReentrantReloadScript(Root& root) : LuaBaseComponent(root) {}
protected:
  std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    auto defined = context()->eval<LuaValue>(R"LUA(
      function probe() return true end
      function init() host.reenter("init") end
      function uninit() host.reenter("uninit") end
      function dismissed() host.reenter("dismissed") end
    )LUA");
    if (!defined)
      return std::unexpected(std::move(defined.error()));
    return {};
  }
};

const GameTestRegistration scriptReloadReentry("ScriptPane", "ExplicitInitAndAutomaticReloadAllowHookReentry", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  ReentrantReloadScript script(fixture.root());
  ASSERT_TRUE(script.setLuaRoot(*luaRoot));
  script.setAutoReInit(true);
  int initialized = 0;
  int uninitialized = 0;
  int dismissed = 0;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("reenter", [&](String const& hook) -> std::expected<void, StarException> {
    if (hook == "init")
      ++initialized;
    else if (hook == "uninit")
      ++uninitialized;
    else
      ++dismissed;
    auto result = script.invoke<bool>("probe");
    if (!result)
      return std::unexpected(std::move(result.error()));
    if (!result->value_or(false))
      return std::unexpected(StarException("Reentrant hook lost its script context"));
    return {};
  }));
  ASSERT_TRUE(script.addCallbacks("host", std::move(callbacks)));
  ASSERT_TRUE(script.init());
  EXPECT_EQ(initialized, 1);
  ASSERT_TRUE(fixture.root().reload());
  ASSERT_TRUE(script.init());
  EXPECT_EQ(initialized, 2);
  EXPECT_EQ(uninitialized, 1);
  ASSERT_TRUE(fixture.root().reload());
  auto reloaded = script.invoke<bool>("probe");
  ASSERT_TRUE(reloaded) << reloaded.error().what();
  EXPECT_TRUE(reloaded->value_or(false));
  EXPECT_EQ(initialized, 3);
  EXPECT_EQ(uninitialized, 2);
  ASSERT_TRUE((*luaRoot)->restart());
  auto restarted = script.invoke<bool>("probe");
  ASSERT_TRUE(restarted) << restarted.error().what();
  EXPECT_TRUE(restarted->value_or(false));
  EXPECT_EQ(initialized, 4);
  EXPECT_EQ(uninitialized, 3);
  ASSERT_TRUE(fixture.root().reload());
  ASSERT_TRUE((*luaRoot)->restart());
  ASSERT_TRUE(script.uninit("dismissed"));
  EXPECT_EQ(initialized, 4);
  EXPECT_EQ(dismissed, 1);
  EXPECT_EQ(uninitialized, 4);
});

class ShutdownFailureScript : public LuaBaseComponent {
public:
  explicit ShutdownFailureScript(Root& root) : LuaBaseComponent(root) {}
  unsigned shutdowns = 0;
protected:
  std::expected<void, StarException> contextSetup() override {
    if (auto result = LuaBaseComponent::contextSetup(); !result)
      return std::unexpected(std::move(result.error()));
    auto defined = context()->eval<LuaValue>(R"LUA(
      function fail() error("shutdown reentry failure") end
      function dismissed() host.reenterFailure() end
      function uninit() host.uninitReached() end
    )LUA");
    if (!defined)
      return std::unexpected(std::move(defined.error()));
    return {};
  }
  std::expected<void, StarException> contextShutdown() override {
    ++shutdowns;
    return LuaBaseComponent::contextShutdown();
  }
};

const GameTestRegistration scriptShutdownFailure("ScriptPane", "ReentrantOwnerHookFailureShutsContextDownOnce", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  ShutdownFailureScript script(fixture.root());
  ASSERT_TRUE(script.setLuaRoot(*luaRoot));
  unsigned uninitReached = 0;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("reenterFailure", [&]() -> std::expected<void, StarException> {
    auto result = script.invoke("fail");
    if (!result)
      return std::unexpected(std::move(result.error()));
    return {};
  }));
  ASSERT_TRUE(callbacks.registerCallback("uninitReached", [&]() { ++uninitReached; }));
  ASSERT_TRUE(script.addCallbacks("host", std::move(callbacks)));
  ASSERT_TRUE(script.init());
  std::expected<void, StarException> stopped;
  testing::TestPartResultArray expectedErrors;
  {
    // The shared execution boundary logs the original reentrant failure once.
    testing::ScopedFakeTestPartResultReporter capture(
        testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &expectedErrors);
    stopped = script.uninit("dismissed");
  }
  ASSERT_EQ(expectedErrors.size(), 1);
  for (int i = 0; i < expectedErrors.size(); ++i) {
    auto const& result = expectedErrors.GetTestPartResult(i);
    EXPECT_EQ(result.type(), testing::TestPartResult::kNonFatalFailure);
    auto message = std::string_view(result.message());
    EXPECT_NE(message.find("shutdown reentry failure"), std::string_view::npos);
  }
  ASSERT_FALSE(stopped);
  EXPECT_TRUE(stopped.error().is<LuaException>());
  EXPECT_FALSE(script.initialized());
  EXPECT_EQ(script.shutdowns, 1u);
  EXPECT_EQ(uninitReached, 0u);
});

const GameTestRegistration scriptHiddenInput("ScriptPane", "HiddenDisplayedPaneRejectsInputBeforeScriptHook", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  auto& pane = **created;
  ASSERT_TRUE(pane.setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane.displayed());
  auto editor = textEditor(**context, "A中");
  ASSERT_TRUE(editor) << editor.error().what();
  (*editor)->setEditorRendering(false);
  ASSERT_TRUE(pane.addChild("editor", *editor));
  ASSERT_TRUE((*editor)->focus());
  ASSERT_TRUE(pane.evaluate(R"LUA(
    storage.inputs = 0
    function onInput(event)
      storage.inputs = storage.inputs + 1
      return true
    end
  )LUA"));
  ASSERT_TRUE(pane.hide());
  EXPECT_TRUE(pane.isDisplayed());
  for (InputEvent const& event : {
      InputEvent{MouseButtonDownEvent{MouseButton::Left, {1, 1}}},
      InputEvent{KeyDownEvent{Key::A, KeyMod::NoMod}},
      InputEvent{TextInputEvent{"🙂"}}}) {
    auto consumed = pane.sendEvent(event);
    ASSERT_TRUE(consumed) << consumed.error().what();
    EXPECT_FALSE(*consumed);
  }
  auto inputs = pane.evaluate("return storage.inputs");
  ASSERT_TRUE(inputs) << inputs.error().what();
  ASSERT_TRUE(*inputs);
  EXPECT_EQ(**inputs, Json(0));
  EXPECT_EQ((*editor)->getText(), String("A中"));
  ASSERT_TRUE(pane.dismissed());
});

const GameTestRegistration scriptTransparentInput("ScriptPane", "NoninteractivePaneRejectsInputAndHitTests", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto created = ScriptTestPane::create(**context, false);
  ASSERT_TRUE(created) << created.error().what();
  auto& pane = **created;
  pane.setAnchor(PaneAnchor::BottomLeft);
  pane.setSize({100, 100});
  ASSERT_TRUE(pane.setLuaRoot(*luaRoot));
  PaneManager manager(**context);
  ASSERT_TRUE(manager.displayPane(PaneLayer::Background, *created));
  ASSERT_TRUE(pane.evaluate(R"LUA(
    storage.inputs = 0
    function onInput(event)
      storage.inputs = storage.inputs + 1
      return true
    end
  )LUA"));
  EXPECT_TRUE(pane.isDisplayed());
  EXPECT_FALSE(manager.getPaneAt({1, 1}));
  EXPECT_FALSE(manager.getPaneAt({PaneLayer::Background}, {1, 1}));
  auto editor = textEditor(**context, "A中");
  ASSERT_TRUE(editor) << editor.error().what();
  ASSERT_TRUE(pane.addChild("editor", *editor));
  ASSERT_TRUE((*editor)->focus());
  auto captured = manager.keyboardCapturedPane();
  ASSERT_TRUE(captured) << captured.error().what();
  EXPECT_FALSE(*captured);
  auto capturedWidget = manager.keyboardCapturedWidget();
  ASSERT_TRUE(capturedWidget) << capturedWidget.error().what();
  EXPECT_FALSE(*capturedWidget);
  auto targets = manager.inputTargetPanes();
  ASSERT_TRUE(targets) << targets.error().what();
  EXPECT_TRUE(targets->empty());
  pane.setDragActive(true, {1, 1});
  for (InputEvent const& event : {
      InputEvent{MouseButtonDownEvent{MouseButton::Left, {1, 1}}},
      InputEvent{MouseButtonUpEvent{MouseButton::Left, {1, 1}}},
      InputEvent{KeyDownEvent{Key::A, KeyMod::NoMod}},
      InputEvent{TextInputEvent{"🙂"}}}) {
    auto directlyConsumed = pane.sendEvent(event);
    ASSERT_TRUE(directlyConsumed) << directlyConsumed.error().what();
    EXPECT_FALSE(*directlyConsumed);
    auto consumed = manager.sendInputEvent(event);
    ASSERT_TRUE(consumed) << consumed.error().what();
    EXPECT_FALSE(*consumed);
  }
  auto inputs = pane.evaluate("return storage.inputs");
  ASSERT_TRUE(inputs) << inputs.error().what();
  ASSERT_TRUE(*inputs);
  EXPECT_EQ(**inputs, Json(0));
  EXPECT_EQ((*editor)->getText(), String("A中"));

  // A normal pane still participates in hit testing and real keyboard capture.
  auto back = Pane::create(**context);
  ASSERT_TRUE(back) << back.error().what();
  (*back)->setAnchor(PaneAnchor::BottomLeft);
  (*back)->setSize({100, 100});
  auto backEditor = textEditor(**context, "B");
  ASSERT_TRUE(backEditor) << backEditor.error().what();
  ASSERT_TRUE((*back)->addChild("editor", *backEditor));
  ASSERT_TRUE(manager.displayPane(PaneLayer::Hud, *back));
  ASSERT_TRUE((*backEditor)->focus());
  EXPECT_EQ(manager.getPaneAt({1, 1}), *back);
  captured = manager.keyboardCapturedPane();
  ASSERT_TRUE(captured) << captured.error().what();
  EXPECT_EQ(*captured, *back);
  auto committed = manager.sendInputEvent(TextInputEvent{"界"});
  ASSERT_TRUE(committed) << committed.error().what();
  EXPECT_TRUE(*committed);
  EXPECT_EQ((*backEditor)->getText(), String("B界"));
  ASSERT_TRUE(manager.dismissAllPanes());
});

const GameTestRegistration scriptInputEditor("ScriptPane", "ConsumingInputPrecedesFocusedNativeEditor", [](GameTest& fixture) {
  auto configuration = fixture.root().configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  auto bindings = Json::parseJson(R"JSON({
    "ChatStop":[{"type":"key","value":"Backspace","mods":[]}],
    "GuiClose":[{"type":"key","value":"Esc","mods":[]}]
  })JSON");
  ASSERT_TRUE(bindings) << bindings.error().what();
  ASSERT_TRUE((*configuration)->set("bindings", *bindings));
  auto context = GuiContext::create(fixture.root(), {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  auto& pane = **created;
  ASSERT_TRUE(pane.setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane.displayed());
  ASSERT_TRUE(pane.evaluate(R"LUA(
    function onInput(event)
      storage.event = event
      return event.actions[1] == "ChatStop"
    end
    function read() return storage.event end
  )LUA"));
  auto editor = textEditor(**context, "A中");
  ASSERT_TRUE(editor) << editor.error().what();
  (*editor)->setEditorRendering(false);
  ASSERT_TRUE(pane.addChild("editor", *editor));
  ASSERT_TRUE((*editor)->focus());
  auto consumed = pane.sendEvent(KeyDownEvent{Key::Backspace, KeyMod::NoMod});
  ASSERT_TRUE(consumed) << consumed.error().what();
  EXPECT_TRUE(*consumed);
  EXPECT_EQ((*editor)->getText(), String("A中"));
  auto action = pane.call("read", {});
  ASSERT_TRUE(action) << action.error().what();
  ASSERT_TRUE(*action);
  auto actions = (**action).getArray("actions");
  ASSERT_TRUE(actions) << actions.error().what();
  EXPECT_EQ(*actions, (JsonArray{"ChatStop"}));
  auto release = pane.inputJson(KeyUpEvent{Key::Backspace});
  ASSERT_TRUE(release) << release.error().what();
  auto releaseActions = release->getArray("actions");
  ASSERT_TRUE(releaseActions) << releaseActions.error().what();
  EXPECT_TRUE(releaseActions->empty());
  auto committed = pane.sendEvent(TextInputEvent{"🙂"});
  ASSERT_TRUE(committed) << committed.error().what();
  EXPECT_TRUE(*committed);
  EXPECT_EQ((*editor)->getText(), String("A中🙂"));
  auto forwarded = pane.forwardEvent(KeyDownEvent{Key::Backspace, KeyMod::NoMod});
  ASSERT_TRUE(forwarded) << forwarded.error().what();
  EXPECT_TRUE(*forwarded);
  EXPECT_EQ((*editor)->getText(), String("A中"));
  ASSERT_TRUE(pane.evaluate("onInput = nil"));
  EXPECT_FALSE(pane.isDismissed());
  auto closed = pane.sendEvent(KeyDownEvent{Key::Escape, KeyMod::NoMod});
  ASSERT_TRUE(closed) << closed.error().what();
  EXPECT_TRUE(*closed);
  EXPECT_TRUE(pane.isDismissed());
  ASSERT_TRUE(pane.dismissed());
});


const GameTestRegistration scriptPaneBounds("ScriptPane", "WidgetOnlyPaneHasAutomaticOrExplicitRootBounds", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  JsonObject config{{"gui", JsonObject{{"canvas", JsonObject{
      {"type", "canvas"}, {"position", JsonArray{7, 9}}, {"size", JsonArray{23, 11}}}}}}};
  auto automatic = BaseScriptPane::create(**context, config, true);
  ASSERT_TRUE(automatic) << automatic.error().what();
  EXPECT_EQ((*automatic)->size(), (Vec2I{30, 20}));
  config.set("size", JsonArray{100, 40});
  auto explicitSize = BaseScriptPane::create(**context, config, true);
  ASSERT_TRUE(explicitSize) << explicitSize.error().what();
  EXPECT_EQ((*explicitSize)->size(), (Vec2I{100, 40}));
});

class InputOrderPane : public Pane {
public:
  static std::expected<std::shared_ptr<InputOrderPane>, StarException> create(GuiContext& context, bool consumes) {
    auto pane = std::shared_ptr<InputOrderPane>(new InputOrderPane(context, consumes));
    if (auto result = pane->initializePane(); !result)
      return std::unexpected(std::move(result.error()));
    pane->setAnchor(PaneAnchor::BottomLeft);
    pane->setSize({10, 10});
    return pane;
  }
  std::expected<bool, StarException> sendEvent(InputEvent const& event) override {
    return m_consumes && std::holds_alternative<MouseButtonDownEvent>(event);
  }
private:
  InputOrderPane(GuiContext& context, bool consumes) : Pane(context), m_consumes(consumes) {}
  bool m_consumes;
};

const GameTestRegistration stableHudOrder("ScriptedHud", "PointerConsumptionRaisesWindowsButNotFixedLayers", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  for (auto layer : {PaneLayer::Hud, PaneLayer::World, PaneLayer::Window}) {
    PaneManager manager(**context);
    auto back = InputOrderPane::create(**context, true);
    auto front = InputOrderPane::create(**context, false);
    ASSERT_TRUE(back) << back.error().what();
    ASSERT_TRUE(front) << front.error().what();
    ASSERT_TRUE(manager.displayPane(layer, *back, {}));
    ASSERT_TRUE(manager.displayPane(layer, *front, {}));
    EXPECT_EQ(manager.topPane({layer}), *front);
    auto consumed = manager.sendInputEvent(MouseButtonDownEvent{MouseButton::Left, {1, 1}});
    ASSERT_TRUE(consumed) << consumed.error().what();
    EXPECT_TRUE(*consumed);
    EXPECT_EQ(manager.topPane({layer}), layer == PaneLayer::Window ? *back : *front);
    ASSERT_TRUE(manager.dismissAllPanes());
  }
});

const GameTestRegistration uiWorldScope("LuaUiManager", "WorldAndPlayerTransitionsDropScopedEventsBeforeReleasingNativePane", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto script = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(script) << script.error().what();
  PaneManager panes(**context);
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  auto worldPane = std::move(*created);
  ASSERT_TRUE(worldPane->setLuaRoot(*luaRoot));
  worldPane->setAnchor(PaneAnchor::BottomLeft);
  auto displayed = panes.displayPane(PaneLayer::Window, worldPane, {});
  ASSERT_TRUE(displayed) << displayed.error().what();
  std::weak_ptr<Pane> released = worldPane;
  auto retained = worldPane->makePaneCallbacks();
  ASSERT_TRUE(retained) << retained.error().what();
  ASSERT_TRUE(script->setCallbacks("retained", *retained));
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  unsigned destroyed = 0;
  ASSERT_TRUE(callbacks.registerCallback("destroyPane", [&](String const& id) -> std::expected<void, StarException> {
    if ((id == "worldOpen" || id == "playerOpen") && worldPane) {
      if (auto dismissed = panes.dismissPane(worldPane); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
      worldPane.reset();
      ++destroyed;
    }
    return {};
  }));
  ASSERT_TRUE(callbacks.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (!worldPane) return std::optional<LuaCallbacks>{};
    auto bound = worldPane->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(callbacks.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (!worldPane) return std::unexpected(GuiException("Missing scoped pane"));
    return worldPane->show();
  }));
  ASSERT_TRUE(script->setCallbacks("interface", callbacks));
  // Suspension releases a real displayed Pane before any queued dispatch.
  // No callPane echo exists: dispatch before teardown fails.
  auto suspended = script->eval<Json>(R"LUA(
    local state = {
      session = {inWorld = true, worldGeneration = 1, playerGeneration = 1},
      open = {worldOpen = true, worldClosed = false, sessionPane = true}
    }
    local definitions = {
      worldOpen = {config = {}, scope = "world"},
      worldClosed = {config = {}, scope = "world"},
      sessionPane = {config = {}, scope = "session"}
    }
    manager = UiManager.new(state, definitions, state.session)
    manager:enqueue("worldOpen", "message", {old = true})
    manager:enqueue("worldClosed", "message", {old = true})
    manager:enqueue("$ui", "open", {id = "worldOpen"})
    manager:enqueue("$ui", "close", {id = "worldClosed"})
    manager:enqueue("sessionPane", "message", {keep = true})
    manager:enqueue("$ui", "open", {id = "sessionPane"})
    manager:enqueue("$ui", "register", {id = "worldOpen", definition = definitions.worldOpen})
    manager:suspend({inWorld = true, worldGeneration = 1, playerGeneration = 2})
    local alive = pcall(retained.isDisplayed)
    return {#state.queue, state.queue[1].id, state.queue[2].payload.id,
      state.queue[3].event, state.open.worldOpen == nil,
      state.open.worldClosed == nil, state.open.sessionPane, alive}
  )LUA");
  ASSERT_TRUE(suspended) << suspended.error().what();
  EXPECT_EQ(*suspended, (Json(JsonArray{3, "sessionPane", "sessionPane", "register", true, true, true, false})));
  EXPECT_EQ(destroyed, 1u);
  EXPECT_TRUE(released.expired());
  EXPECT_FALSE(panes.topPane());

  created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  worldPane = std::move(*created);
  ASSERT_TRUE(worldPane->setLuaRoot(*luaRoot));
  worldPane->setAnchor(PaneAnchor::BottomLeft);
  displayed = panes.displayPane(PaneLayer::Window, worldPane, {});
  ASSERT_TRUE(displayed) << displayed.error().what();
  released = worldPane;
  // Engine replacement can bypass suspend. Restoring the real Lua checkpoint
  // must apply the same old-world event filtering before adopting instances.
  auto restored = script->eval<Json>(R"LUA(
    local state = {
      session = {inWorld = true, worldGeneration = 1},
      open = {worldOpen = true},
      queue = jarray()
    }
    manager = UiManager.new(state, {worldOpen = {config = {}, scope = "world"}},
      {inWorld = true, worldGeneration = 2})
    manager:enqueue("worldOpen", "message", {old = true})
    manager:restore()
    return {#state.queue, state.open.worldOpen == nil}
  )LUA");
  ASSERT_TRUE(restored) << restored.error().what();
  EXPECT_EQ(*restored, (Json(JsonArray{0, true})));
  EXPECT_EQ(destroyed, 2u);
  EXPECT_TRUE(released.expired());
  EXPECT_FALSE(panes.topPane());

  // Player scope survives a warp (including controller restore) but must drop
  // both direct events and control requests before another player can see them.
  for (bool restoreTransition : {false, true}) {
    created = ScriptTestPane::create(**context);
    ASSERT_TRUE(created) << created.error().what();
    worldPane = std::move(*created);
    ASSERT_TRUE(worldPane->setLuaRoot(*luaRoot));
    worldPane->setAnchor(PaneAnchor::BottomLeft);
    ASSERT_TRUE(panes.displayPane(PaneLayer::Window, worldPane, {}));
    released = worldPane;
    auto survived = script->eval<Json>(R"LUA(
      checkpoint = {
        session = {hasPlayer = true, inWorld = true, worldGeneration = 1, playerGeneration = 1},
        open = {playerOpen = true},
        queue = jarray()
      }
      definitions = {playerOpen = {config = {}, scope = "player", open = false}}
      manager = UiManager.new(checkpoint, definitions, checkpoint.session)
      manager:enqueue("playerOpen", "show", {message = "old player"})
      manager:enqueue("$ui", "open", {id = "playerOpen"})
      manager:suspend({hasPlayer = true, inWorld = false, worldGeneration = 2, playerGeneration = 1})
      manager = UiManager.new(checkpoint, definitions,
        {hasPlayer = true, inWorld = true, worldGeneration = 3, playerGeneration = 1})
      manager:restore()
      return {checkpoint.open.playerOpen, #checkpoint.queue}
    )LUA");
    ASSERT_TRUE(survived) << survived.error().what();
    EXPECT_EQ(*survived, (Json(JsonArray{true, 2})));
    EXPECT_FALSE(released.expired());
    EXPECT_EQ(panes.topPane(), worldPane);
    auto dropped = script->eval<Json>(String(std::string_view(restoreTransition ? R"LUA(
      manager = UiManager.new(checkpoint, definitions,
        {hasPlayer = true, inWorld = true, worldGeneration = 3, playerGeneration = 2})
      manager:restore()
      return {checkpoint.open.playerOpen == nil, #checkpoint.queue}
    )LUA" : R"LUA(
      manager:suspend({hasPlayer = true, inWorld = true, worldGeneration = 3, playerGeneration = 2})
      return {checkpoint.open.playerOpen == nil, #checkpoint.queue}
    )LUA")));
    ASSERT_TRUE(dropped) << dropped.error().what();
    EXPECT_EQ(*dropped, (Json(JsonArray{true, 0})));
    EXPECT_TRUE(released.expired());
    EXPECT_FALSE(panes.topPane());
  }
  auto unavailable = script->eval<bool>(R"LUA(
    manager = UiManager.new({}, definitions, {hasPlayer = false, inWorld = false, playerGeneration = 2})
    return manager:open("playerOpen")
  )LUA");
  ASSERT_TRUE(unavailable) << unavailable.error().what();
  EXPECT_FALSE(*unavailable);
});

const GameTestRegistration uiInputAndQueue("LuaUiManager", "NativePriorityReloadPairingAndDeferredHandlerEvents", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  PaneManager panes(**context);
  Map<String, std::shared_ptr<ScriptTestPane>> instances;
  LuaUpdateScript controller(fixture.root());
  ASSERT_TRUE(controller.setScript("/interface/opensb/ui/manager.lua"));
  ASSERT_TRUE(controller.setLuaRoot(*luaRoot));
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(callbacks.registerCallback("bindRegisteredPane", [&](String const& id) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    auto instance = instances.maybeRef(id);
    if (!instance) return std::optional<LuaCallbacks>{};
    auto bound = instance->get()->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(callbacks.registerCallback("callPane", [&](String const& id, String const& method, JsonArray const& args) -> std::expected<std::optional<Json>, StarException> {
    auto instance = instances.maybeRef(id);
    if (!instance) return std::unexpected(GuiException("Missing test pane"));
    return instance->get()->call(method, args);
  }));
  ASSERT_TRUE(callbacks.registerCallback("displayRegisteredPane", [&](String const& id) -> std::expected<void, StarException> {
    auto instance = instances.maybeRef(id);
    if (!instance) return std::unexpected(GuiException("Missing test pane"));
    if (panes.isDisplayed(instance->get())) return instance->get()->show();
    return panes.displayPane(PaneLayer::Window, instance->get(), {});
  }));
  ASSERT_TRUE(callbacks.registerCallback("destroyPane", [&](String const& id) -> std::expected<void, StarException> {
    auto instance = instances.maybeRef(id);
    if (!instance) return {};
    if (panes.isDisplayed(instance->get())) {
      if (auto dismissed = panes.dismissPane(instance->get()); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    instances.remove(id);
    return {};
  }));
  ASSERT_TRUE(callbacks.registerCallback("paneLayerOccupied", [&](String const&) {
    return static_cast<bool>(panes.topPane({PaneLayer::ModalWindow}));
  }));
  ASSERT_TRUE(callbacks.registerCallback("paneInputTargets", [&]() -> std::expected<JsonArray, StarException> {
    auto targets = panes.inputTargetPanes();
    if (!targets) return std::unexpected(std::move(targets.error()));
    JsonArray ids;
    for (auto const& pane : *targets)
      for (auto const& entry : instances)
        if (entry.second == pane) ids.append(entry.first);
    return ids;
  }));
  ASSERT_TRUE(controller.addCallbacks("interface", callbacks));
  ASSERT_TRUE(controller.init());
  unsigned cancellations = 0;
  for (auto const& id : {String("a"), String("b")}) {
    auto created = ScriptTestPane::create(**context);
    ASSERT_TRUE(created) << created.error().what();
    auto pane = std::move(*created);
    pane->setAnchor(PaneAnchor::BottomLeft);
    ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
    LuaCallbacks host;
    ASSERT_TRUE(host.registerCallback("send", [&, id](String const& event, Json payload) -> std::expected<void, StarException> {
      auto queued = controller.invoke<Json>("enqueue", id, event, std::move(payload));
      if (!queued) return std::unexpected(std::move(queued.error()));
      return {};
    }));
    ASSERT_TRUE(host.registerCallback("cancelled", [&]() { ++cancellations; }));
    ASSERT_TRUE(pane->addCallbacks("host", std::move(host)));
    auto displayed = panes.displayPane(PaneLayer::Window, pane, {});
    ASSERT_TRUE(displayed) << displayed.error().what();
    ASSERT_TRUE(pane->evaluate(R"LUA(
      function onAction(action, pressed, event)
        storage.held = pressed
        if event and event.cancelled then host.cancelled() end
        return true
      end
      function onEvent(name, payload)
        storage.events = (storage.events or 0) + 1
        host.send(name, payload)
      end
    )LUA"));
    instances.set(id, std::move(pane));
  }
  String setup = R"LUA(
    local definitions = {
      a = {config = {}, scope = "session", order = 100, actions = {Probe = "hold"}},
      b = {config = {}, scope = "session", order = 0, actions = {Probe = "hold"}}
    }
    storage.open = storage.open or {a = true, b = true}
    manager = UiManager.new(storage, definitions, {inWorld = true, worldGeneration = 1})
    manager:restore()
    function enqueue(id, event, payload) manager:enqueue(id, event, payload) end
    return true
  )LUA";
  ASSERT_TRUE(controller.eval<Json>(setup));
  auto initial = controller.eval<Json>(R"LUA(
    local configured = pcall(UiManager.new, {}, {Inventory = {config = {}}}, {})
    local registered = pcall(function() manager:register("Inventory", {config = {}, open = true}) end)
    local handled = manager:input({type = "keyDown", key = 1, actions = {"Probe"}})
    return {configured, registered, manager.definitions.Inventory == nil,
      storage.registered.Inventory == nil, handled, storage.pressed["1"].id}
  )LUA");
  ASSERT_TRUE(initial) << initial.error().what();
  ASSERT_TRUE(*initial);
  EXPECT_EQ(**initial, (Json(JsonArray{false, false, true, true, true, "b"})));

  auto a = instances.value("a");
  auto b = instances.value("b");
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(panes.dismissPane(a));
  auto redisplayed = panes.displayPane(PaneLayer::Window, a, {});
  ASSERT_TRUE(redisplayed) << redisplayed.error().what();
  // Native redisplay raises A above B despite the fixed declaration order.
  ASSERT_TRUE(a->evaluate("function onAction(action, pressed, event) storage.held = pressed; if event and event.cancelled then host.cancelled() end; return true end"));
  auto raised = controller.eval<Json>("manager:input({type='keyDown', key=2, actions={'Probe'}}); return storage.pressed['2'].id");
  ASSERT_TRUE(raised) << raised.error().what();
  ASSERT_TRUE(*raised);
  EXPECT_EQ(**raised, Json("a"));
  auto modal = InputOrderPane::create(**context, false);
  ASSERT_TRUE(modal) << modal.error().what();
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, *modal, {}));
  auto blocked = controller.eval<Json>("return manager:input({type='keyDown', key=3, actions={'Probe'}})");
  ASSERT_TRUE(blocked) << blocked.error().what();
  ASSERT_TRUE(*blocked);
  EXPECT_EQ(**blocked, Json(false));
  ASSERT_TRUE(panes.dismissPane(*modal));
  auto editor = textEditor(**context, "");
  ASSERT_TRUE(editor) << editor.error().what();
  ASSERT_TRUE(b->addChild("editor", *editor));
  ASSERT_TRUE((*editor)->focus());
  auto captured = panes.inputTargetPanes();
  ASSERT_TRUE(captured) << captured.error().what();
  EXPECT_EQ(*captured, (List<std::shared_ptr<Pane>>{b}));
  ASSERT_TRUE((*editor)->blur());

  ASSERT_TRUE(controller.uninit());
  ASSERT_TRUE(controller.init());
  ASSERT_TRUE(controller.eval<Json>(setup));
  auto released = controller.eval<Json>(R"LUA(
    local paired = manager:input({type = "keyUp", key = 1, actions = {}})
    manager:close("a")
    return {paired, storage.pressed["1"] == nil, storage.pressed["2"] == nil}
  )LUA");
  ASSERT_TRUE(released) << released.error().what();
  ASSERT_TRUE(*released);
  EXPECT_EQ(**released, (Json(JsonArray{true, true, true})));
  EXPECT_EQ(cancellations, 1u);
  auto held = b->evaluate("return storage.held");
  ASSERT_TRUE(held) << held.error().what();
  ASSERT_TRUE(*held);
  EXPECT_EQ(**held, Json(false));

  auto first = controller.eval<Json>("manager:enqueue('b','pulse',{}); manager:update(); return #storage.queue");
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(*first);
  EXPECT_EQ(**first, Json(1));
  auto events = b->evaluate("return storage.events");
  ASSERT_TRUE(events) << events.error().what();
  ASSERT_TRUE(*events);
  EXPECT_EQ(**events, Json(1));
  auto second = controller.eval<Json>("manager:update(); return #storage.queue");
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(*second);
  EXPECT_EQ(**second, Json(1));
  events = b->evaluate("return storage.events");
  ASSERT_TRUE(events) << events.error().what();
  ASSERT_TRUE(*events);
  EXPECT_EQ(**events, Json(2));
  ASSERT_TRUE(controller.eval<Json>("manager:shutdown()"));
  ASSERT_TRUE(controller.uninit());
  EXPECT_TRUE(instances.empty());
  EXPECT_FALSE(panes.topPane());
});

const GameTestRegistration statusPresentation("LuaStatusPresentation", "LayoutMasksAndOverlappingTooltips", [](GameTest& fixture) {
  auto root = LuaRoot::create(fixture.root());
  ASSERT_TRUE(root) << root.error().what();
  auto script = (*root)->createContext("/interface/opensb/status/status.lua");
  ASSERT_TRUE(script) << script.error().what();
  // Exercise the production presentation algorithm independently of a display.
  // The GUI smoke separately checks these commands against real effect pixels.
  auto result = script->eval<Json>(R"LUA(
    local effects = {{"hidden", 1}, {"wide", 1}, {"timed", 0.45}}
    local definitions = {
      hidden = {},
      wide = {icon = "/wide.png", label = ""},
      timed = {icon = "/timed.png", label = "Timed"}
    }
    local dimensions = {["/wide.png"] = {16, 10}, ["/timed.png"] = {8, 6}}
    local frame, bounds, anchor
    local surface = {
      clear = function() frame = {} end,
      drawImage = function(_, image, position)
        frame[#frame + 1] = {image, {position[1], position[2]}}
      end
    }
    widget = {bindCanvas = function() return surface end, setSize = function() end}
    pane = {
      setSize = function(size) bounds = size end,
      setAnchorOffset = function(position) anchor = position end,
      getScreenPosition = function() return {100, 200} end
    }
    root = {
      assetJson = function(path)
        if path == "/interface.config:statusIconPos" then return {5, 20} end
        if path == "/interface.config:statusIconShift" then return {8, -2} end
        if path == "/interface.config:statusIconDarkenImage" then return "/dark.png" end
        error("Unexpected asset: " .. path)
      end,
      effectConfig = function(name) return definitions[name] end,
      imageSize = function(path) return dimensions[path] end
    }
    status = {activeUniqueStatusEffectSummary = function() return effects end}
    init()
    render()
    local initial = {bounds, anchor, frame,
      createTooltip({110, 203}), createTooltip({117, 203}) == nil}
    effects = {}
    update(0)
    render()
    local empty = {bounds, createTooltip({110, 203}) == nil}
    effects = {{"timed", 0}}
    update(0)
    render()
    local zero = frame[1][1]
    effects = {{"timed"}}
    update(0)
    render()
    local absent = {frame[1][1], createTooltip({108, 206})}
    -- This is the exact double representation of native float(0.7).
    -- float32 multiplication rounds to 7; double multiplication truncates to 6.
    effects = {{"wide", 0.699999988079071}}
    update(0)
    render()
    return {initial, empty, zero, absent[1], absent[2], frame[1][1]}
  )LUA");
  ASSERT_TRUE(result) << result.error().what();
  auto expected = Json::parseJson(R"JSON([
    [[16,12],[5,-10],[
      ["/wide.png?blendmult=/dark.png;0;-10",[0,2]],
      ["/timed.png?blendmult=/dark.png;0;-2",[8,0]]
    ],"Timed",true],
    [[0,0],true],
    "/timed.png?blendmult=/dark.png;0;0",
    "/timed.png",
    "Timed",
    "/wide.png?blendmult=/dark.png;0;-7"
  ])JSON");
  ASSERT_TRUE(expected) << expected.error().what();
  EXPECT_EQ(*result, *expected);
});

const GameTestRegistration breathPresentation("LuaBreathPresentation", "FloatBlockEdgesPhysicalGeometryAndWarningClock", [](GameTest& fixture) {
  auto root = LuaRoot::create(fixture.root());
  ASSERT_TRUE(root) << root.error().what();
  auto script = (*root)->createContext("/interface/opensb/breath/breath.lua");
  ASSERT_TRUE(script) << script.error().what();

  // Compare the production numeric policy against the former float renderer at
  // both sides of every half-block boundary, including float-rounding ties.
  for (float maximum : {1.0f, 100.0f, 137.3f}) {
    for (int block = 0; block < 10; ++block) {
      float edge = maximum * ((block + 0.5f) / 10.0f);
      for (float value : {std::nextafter(edge, 0.0f), edge, std::nextafter(edge, maximum)}) {
        auto blocks = script->invokePath<int>("breathPresentation.blocks", value, maximum);
        ASSERT_TRUE(blocks) << blocks.error().what();
        EXPECT_EQ(*blocks, static_cast<int>(std::round((10 * value) / maximum)))
            << "value=" << value << ", maximum=" << maximum;
      }
    }
    auto full = script->invokePath<int>("breathPresentation.blocks", maximum, maximum);
    ASSERT_TRUE(full) << full.error().what();
    EXPECT_EQ(*full, 10);
  }

  Vec2I position{3, 20};
  Vec2I barPosition{-17, -2};
  for (auto size : {Vec2U{801, 601}, Vec2U{800, 600}}) {
    for (float scale : {1.0f, 1.25f, 1.3f, 1.75f, 2.5f}) {
      auto layout = script->invokePath<Json>("breathPresentation.layout",
          JsonArray{size[0], size[1]}, scale, JsonArray{position[0], position[1]},
          JsonArray{barPosition[0], barPosition[1]}, 3);
      ASSERT_TRUE(layout) << layout.error().what();
      Vec2F center{size[0] * 0.5f + position[0] * scale, size[1] - position[1] * scale};
      Vec2F bar = center + Vec2F(barPosition * scale);
      Vec2I iconSize = Vec2I{3, 5} * scale;
      EXPECT_EQ(*layout, (JsonObject{
          {"center", JsonArray{center[0], center[1]}},
          {"bar", JsonArray{bar[0], bar[1]}},
          {"step", iconSize[0]}}));
    }
  }

  for (auto milliseconds : {0, 250, 251, 499, 500, 750, 751}) {
    auto warning = script->invokePath<bool>("breathPresentation.warning", milliseconds);
    ASSERT_TRUE(warning) << warning.error().what();
    EXPECT_EQ(*warning, milliseconds % 500 > 250);
  }
});

const GameTestRegistration bookmarkEditor("LuaBookmarkEditor", "NativeTextEditingIdentityPersistenceAndSessionGuards", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto config = (*assets)->json("/interface/opensb/bookmarks/bookmarkeditor.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig) << paneConfig.error().what();
  auto textboxLayout = (*assets)->json("/interface/windowconfig/editbookmark.config:paneLayout.name");
  ASSERT_TRUE(textboxLayout) << textboxLayout.error().what();
  // Keep the real configured TextBox and production script/domain model. The
  // display-only image/label widgets need a renderer and are covered by GUI smoke.
  paneConfig->set("gui", JsonObject{{"name", *textboxLayout}});
  PlayerUniverseMap model;
  model.setServerUuid(Uuid::nil());
  Json session = JsonObject{{"hasPlayer", true}, {"inWorld", true}, {"playerGeneration", 1}, {"worldGeneration", 1}};
  bool sourceExists = true;
  LuaCallbacks player;
  ASSERT_TRUE(player.registerCallback("teleportBookmarks", [&]() -> std::expected<JsonArray, StarException> {
    auto bookmarks = model.teleportBookmarks();
    if (!bookmarks) return std::unexpected(std::move(bookmarks.error()));
    JsonArray result;
    for (auto const& bookmark : *bookmarks) {
      auto json = bookmark.toJson();
      if (!json) return std::unexpected(std::move(json.error()));
      result.append(std::move(*json));
    }
    return result;
  }));
  ASSERT_TRUE(player.registerCallback("addTeleportBookmark", [&](Json const& value) -> std::expected<bool, StarException> {
    auto bookmark = TeleportBookmark::fromJson(value);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return model.addTeleportBookmark(std::move(*bookmark));
  }));
  ASSERT_TRUE(player.registerCallback("removeTeleportBookmark", [&](Json const& value) -> std::expected<bool, StarException> {
    auto bookmark = TeleportBookmark::fromJson(value);
    if (!bookmark) return std::unexpected(std::move(bookmark.error()));
    return model.removeTeleportBookmark(*bookmark);
  }));
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("session", [&]() { return session; }));
  LuaCallbacks world;
  ASSERT_TRUE(world.registerCallback("entityExists", [&](EntityId) { return sourceExists; }));
  ASSERT_TRUE(world.registerCallback("entityUniqueId", [](EntityId) { return String("local-source"); }));
  PaneManager panes(**context);
  std::shared_ptr<BaseScriptPane> editor;
  auto open = [&](Json bookmark) -> std::expected<void, StarException> {
    if (editor) {
      if (auto dismissed = panes.dismissPane(editor); !dismissed) return std::unexpected(std::move(dismissed.error()));
    }
    auto created = BaseScriptPane::create(**context, *paneConfig);
    if (!created) return std::unexpected(std::move(created.error()));
    editor = std::move(*created);
    editor->setAnchor(PaneAnchor::BottomLeft);
    if (auto added = editor->setLuaRoot(*luaRoot); !added) return std::unexpected(std::move(added.error()));
    auto textbox = editor->fetchChild<TextBoxWidget>("name");
    if (!textbox) return std::unexpected(std::move(textbox.error()));
    // No glyph measurement is available in this renderer-free input fixture.
    (*textbox)->setMaxWidth(-1);
    if (auto added = editor->addCallbacks("player", player); !added) return std::unexpected(std::move(added.error()));
    if (auto added = editor->addCallbacks("interface", interface); !added) return std::unexpected(std::move(added.error()));
    if (auto added = editor->addCallbacks("world", world); !added) return std::unexpected(std::move(added.error()));
    if (auto displayed = panes.displayPane(PaneLayer::ModalWindow, editor, {}); !displayed)
      return std::unexpected(std::move(displayed.error()));
    auto edited = editor->call("editBookmark", {JsonObject{
        {"bookmark", std::move(bookmark)}, {"sourceEntityId", 42}, {"sourceUniqueId", "local-source"}}});
    if (!edited) return std::unexpected(std::move(edited.error()));
    return {};
  };
  auto makeBookmark = [](String target, String name) {
    return Json(JsonObject{{"target", JsonArray{"InstanceWorld:bookmarktest:-:-", target}},
        {"targetName", "Remote Planet"}, {"bookmarkName", name}, {"icon", "default"}});
  };
  auto first = makeBookmark("first", "Duplicate");
  auto second = makeBookmark("second", "Duplicate");
  ASSERT_TRUE(open(first));
  auto textbox = editor->fetchChild<TextBoxWidget>("name");
  ASSERT_TRUE(textbox) << textbox.error().what();
  EXPECT_TRUE((*textbox)->hasFocus().value_or(false));
  ASSERT_TRUE((*textbox)->setSelection(0, 9));
  sendEditorEvent(**textbox, TextInputEvent{"Saved"});
  ASSERT_TRUE(fixture.root().reload());
  ASSERT_TRUE(editor->call("name", {}));
  EXPECT_EQ((*textbox)->getText(), String("Saved"));
  sendEditorEvent(**textbox, KeyDownEvent{Key::Return, KeyMod::NoMod});
  auto saved = model.teleportBookmarks();
  ASSERT_TRUE(saved) << saved.error().what();
  ASSERT_EQ(saved->size(), 1u);
  EXPECT_EQ((*saved)[0].bookmarkName, String("Saved"));
  ASSERT_TRUE(open(second));
  ASSERT_TRUE(editor->call("ok", {}));
  ASSERT_TRUE(open(first)); // The stored name wins over the caller's stale name.
  textbox = editor->fetchChild<TextBoxWidget>("name");
  ASSERT_TRUE(textbox);
  EXPECT_EQ((*textbox)->getText(), String("Saved"));
  ASSERT_TRUE((*textbox)->setText(""));
  sendEditorEvent(**textbox, KeyDownEvent{Key::Return, KeyMod::NoMod});
  saved = model.teleportBookmarks();
  ASSERT_TRUE(saved);
  ASSERT_EQ(saved->size(), 2u);
  auto firstTarget = TeleportBookmark::fromJson(first);
  auto secondTarget = TeleportBookmark::fromJson(second);
  ASSERT_TRUE(firstTarget);
  ASSERT_TRUE(secondTarget);
  for (auto const& bookmark : *saved) {
    EXPECT_EQ(bookmark.bookmarkName, bookmark == *firstTarget ? String("Remote Planet") : String("Duplicate"));
  }
  ASSERT_TRUE(open(first));
  textbox = editor->fetchChild<TextBoxWidget>("name");
  ASSERT_TRUE(textbox);
  ASSERT_TRUE((*textbox)->setText("Cancelled"));
  sendEditorEvent(**textbox, KeyDownEvent{Key::Escape, KeyMod::NoMod});
  auto stored = model.toJson();
  ASSERT_TRUE(stored) << stored.error().what();
  auto restored = PlayerUniverseMap::create(*stored);
  ASSERT_TRUE(restored) << restored.error().what();
  restored->setServerUuid(Uuid::nil());
  auto persisted = restored->teleportBookmarks();
  ASSERT_TRUE(persisted);
  ASSERT_EQ(persisted->size(), 2u);
  for (auto const& bookmark : *persisted)
    EXPECT_NE(bookmark.bookmarkName, String("Cancelled"));
  ASSERT_TRUE(open(first));
  session = JsonObject{{"hasPlayer", true}, {"inWorld", true}, {"playerGeneration", 2}, {"worldGeneration", 1}};
  ASSERT_TRUE(editor->call("remove", {}));
  saved = model.teleportBookmarks();
  ASSERT_TRUE(saved);
  EXPECT_EQ(saved->size(), 2u);
  ASSERT_TRUE(open(first));
  session = JsonObject{{"hasPlayer", true}, {"inWorld", true}, {"playerGeneration", 2}, {"worldGeneration", 2}};
  ASSERT_TRUE(editor->call("ok", {}));
  saved = model.teleportBookmarks();
  ASSERT_TRUE(saved);
  EXPECT_EQ(saved->size(), 2u);
  ASSERT_TRUE(open(first));
  sourceExists = false;
  ASSERT_TRUE(editor->call("ok", {}));
  saved = model.teleportBookmarks();
  ASSERT_TRUE(saved);
  EXPECT_EQ(saved->size(), 2u);
  sourceExists = true;
  ASSERT_TRUE(open(second));
  ASSERT_TRUE(editor->call("remove", {}));
  saved = model.teleportBookmarks();
  ASSERT_TRUE(saved);
  ASSERT_EQ(saved->size(), 1u);
  EXPECT_EQ((*saved)[0], *firstTarget);
  ASSERT_TRUE(panes.dismissAllPanes());
});

// Observe only the final speaker boundary; production Lua still chooses the
// sound, updates real native text/title state, and owns its reload checkpoint.
class PopupSoundTestPane : public BaseScriptPane {
public:
  static std::expected<std::shared_ptr<PopupSoundTestPane>, StarException> create(
      GuiContext& context, Json config, StringList& sounds) {
    auto pane = std::shared_ptr<PopupSoundTestPane>(new PopupSoundTestPane(context, sounds));
    if (auto initialized = pane->init(std::move(config)); !initialized)
      return std::unexpected(std::move(initialized.error()));
    return pane;
  }

  std::expected<LuaCallbacks, StarException> makePaneCallbacks() override {
    auto callbacks = BaseScriptPane::makePaneCallbacks();
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    callbacks->removeCallback("playSound");
    if (auto added = callbacks->registerCallback("playSound", [this](String const& sound) {
          m_sounds.append(sound);
        }); !added)
      return std::unexpected(std::move(added.error()));
    return callbacks;
  }

private:
  PopupSoundTestPane(GuiContext& context, StringList& sounds) : BaseScriptPane(context), m_sounds(sounds) {}
  StringList& m_sounds;
};

const GameTestRegistration popupPresentation("LuaPopup", "NativeTitleLatestTextSoundPrecedenceAndReloadLifecycle", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto configuration = fixture.root().configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{
      {"GuiClose", JsonArray{JsonObject{{"type", "key"}, {"value", "Esc"}, {"mods", JsonArray{}}}}}}));
  ASSERT_TRUE((*context)->refreshKeybindings());
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto config = (*assets)->json("/interface/opensb/popup/popup.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig) << paneConfig.error().what();
  // Glyph layout/images require the GUI fixture. Use a real native TextBox as
  // the text sink here; pane.setTitle remains the actual native Pane binding.
  paneConfig->set("gui", JsonObject{});
  StringList sounds;
  auto created = PopupSoundTestPane::create(**context, *paneConfig, sounds);
  ASSERT_TRUE(created) << created.error().what();
  auto popup = std::move(*created);
  ASSERT_TRUE(popup->setLuaRoot(*luaRoot));
  popup->setAnchor(PaneAnchor::BottomLeft);
  auto message = textEditor(**context, "");
  ASSERT_TRUE(message) << message.error().what();
  (*message)->setMaxWidth(-1);
  ASSERT_TRUE(popup->addChild("message", *message));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, popup, {}));
  ASSERT_TRUE(popup->call("showPopup", {JsonObject{
      {"message", "^red;First café Ω^reset;\nSecond line"}, {"title", "Native title"},
      {"subtitle", "Native subtitle"}, {"sound", ""}}}));
  EXPECT_EQ(popup->title(), String("Native title"));
  EXPECT_EQ(popup->subTitle(), String("Native subtitle"));
  EXPECT_EQ((*message)->getText(), String("^red;First café Ω^reset;\nSecond line"));
  EXPECT_TRUE(sounds.empty());

  auto back = ScriptTestPane::create(**context);
  ASSERT_TRUE(back) << back.error().what();
  (*back)->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE((*back)->setLuaRoot(*luaRoot));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, *back, {}));
  ASSERT_TRUE(popup->hide());
  ASSERT_TRUE(popup->call("showPopup", {JsonObject{{"message", "Latest"}, {"sound", "/override.ogg"}}}));
  EXPECT_EQ((*message)->getText(), String("Latest"));
  EXPECT_EQ(popup->title(), String(""));
  EXPECT_EQ(popup->subTitle(), String(""));
  EXPECT_TRUE(popup->visibility());
  EXPECT_EQ(panes.topPane(), *back); // Replacing a displayed popup does not raise it.
  EXPECT_EQ(sounds, (StringList{"/override.ogg"}));
  ASSERT_TRUE(popup->call("showPopup", {JsonObject{{"message", "Random default"}}}));
  auto defaults = (*assets)->json("/interface/windowconfig/popup.config:onShowSound");
  ASSERT_TRUE(defaults) << defaults.error().what();
  auto defaultSounds = defaults->toArray();
  ASSERT_TRUE(defaultSounds) << defaultSounds.error().what();
  if (defaultSounds->empty()) {
    EXPECT_EQ(sounds.size(), 1u);
  } else {
    ASSERT_EQ(sounds.size(), 2u);
    auto chosen = sounds.last();
    ASSERT_TRUE(chosen) << chosen.error().what();
    EXPECT_TRUE(defaultSounds->contains(Json(chosen->get())));
  }
  auto count = sounds.size();
  ASSERT_TRUE(popup->call("showPopup", {JsonObject{
      {"message", "Retained latest"}, {"title", "Reload title"}, {"subtitle", "Reload subtitle"}, {"sound", ""}}}));
  popup->setTitleString("Displaced title", "Displaced subtitle");
  ASSERT_TRUE((*message)->setText("Displaced text"));
  ASSERT_TRUE(fixture.root().reload());
  ASSERT_TRUE(popup->tick(0));
  EXPECT_EQ((*message)->getText(), String("Retained latest"));
  EXPECT_EQ(popup->title(), String("Reload title"));
  EXPECT_EQ(popup->subTitle(), String("Reload subtitle"));
  EXPECT_EQ(sounds.size(), count);
  popup->setTitleString("Displaced again", "");
  ASSERT_TRUE((*message)->setText("Displaced again"));
  ASSERT_TRUE((*luaRoot)->restart());
  ASSERT_TRUE(popup->tick(0));
  EXPECT_EQ((*message)->getText(), String("Retained latest"));
  EXPECT_EQ(popup->title(), String("Reload title"));
  EXPECT_EQ(sounds.size(), count);

  auto escaped = popup->sendEvent(KeyDownEvent{Key::Escape, KeyMod::NoMod});
  ASSERT_TRUE(escaped) << escaped.error().what();
  EXPECT_TRUE(*escaped);
  EXPECT_TRUE(popup->isDismissed());
  ASSERT_TRUE(panes.dismissPane(popup));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, popup, {}));
  EXPECT_EQ(panes.topPane(), popup); // A genuine reopen returns to the front.
  ASSERT_TRUE(popup->call("ok", {}));
  EXPECT_TRUE(popup->isDismissed());
  ASSERT_TRUE(panes.dismissPane(popup));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, popup, {}));
  ASSERT_TRUE(popup->call("showPopup", {JsonObject{{"message", "Close button"}, {"sound", ""}}}));
  ASSERT_TRUE(popup->call("close", {}));
  EXPECT_TRUE(popup->isDismissed());
  EXPECT_EQ(sounds.size(), count);
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration songbookPresentation("LuaSongbook", "NativeListLiteralFilteringResourceBytesBandAndReload", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "songs")));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "songs/Nested")));
  String resource = "/songs/zzsongbook[A].Café[A].abc";
  std::string bytes = "X:1\nT:Songbook bytes\nK:C\nCDEF|\n";
  bytes.append("\0tail", 5);
  String abc{std::string_view(bytes)};
  ASSERT_TRUE(File::writeFile(abc, File::relativeTo(overlay, "songs/zzsongbook[A].Café[A].abc")));
  ASSERT_TRUE(File::writeFile(String("X:2\nK:C\nC|\n"), File::relativeTo(overlay, "songs/Nested/zzsongbookB.abc")));
  ASSERT_TRUE(File::writeFile(String("X:3\nK:C\nD|\n"), File::relativeTo(overlay, "songs/zzsongbookZ.abc")));
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("songbook.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot) << createdRoot.error().what();
  auto& root = **createdRoot;
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{
      {"GuiClose", JsonArray{JsonObject{{"type", "key"}, {"value", "Esc"}, {"mods", JsonArray{}}}}}}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto assets = root.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto config = (*assets)->json("/interface/opensb/songbook/songbook.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig) << paneConfig.error().what();
  // Use production Lua, real asset services, ListWidget and TextBox bindings.
  // Renderer-only imagery/labels stay in the GUI smoke; row text uses TextBox.
  paneConfig->set("gui", JsonObject{});
  auto created = BaseScriptPane::create(**context, *paneConfig);
  ASSERT_TRUE(created) << created.error().what();
  auto pane = std::move(*created);
  pane->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  for (auto const& name : {String("group"), String("search")}) {
    auto textbox = textEditor(**context, "");
    ASSERT_TRUE(textbox) << textbox.error().what();
    (*textbox)->setMaxWidth(-1);
    ASSERT_TRUE(pane->addChild(name, *textbox));
  }
  auto songs = std::make_shared<Widget>(**context);
  auto schema = Json::parseJson(R"JSON({
    "spacing":[0,0], "memberSize":[200,12],
    "listTemplate":{"songName":{"type":"textbox","position":[0,0],"maxWidth":-1}}
  })JSON");
  ASSERT_TRUE(schema) << schema.error().what();
  auto list = ListWidget::create(**context, *schema);
  ASSERT_TRUE(list) << list.error().what();
  (*list)->registerMemberCallback("songName", [](Widget&) -> std::expected<void, StarException> { return {}; });
  ASSERT_TRUE(songs->addChild("list", *list));
  ASSERT_TRUE(pane->addChild("songs", songs));
  auto domain = Songbook::create(root, "human");
  ASSERT_TRUE(domain) << domain.error().what();
  auto callbacks = LuaBindings::makeSongbookCallbacks(**domain);
  ASSERT_TRUE(callbacks) << callbacks.error().what();
  ASSERT_TRUE(pane->addCallbacks("songbook", *callbacks));
  PaneManager panes(**context);
  auto displayed = panes.displayPane(PaneLayer::Window, pane, {});
  ASSERT_TRUE(displayed) << displayed.error().what();
  ASSERT_TRUE(pane->call("btnPlay", {}));
  EXPECT_FALSE(pane->isDismissed());
  EXPECT_FALSE((*domain)->active());
  auto search = pane->fetchChild<TextBoxWidget>("search");
  auto group = pane->fetchChild<TextBoxWidget>("group");
  ASSERT_TRUE(search) << search.error().what();
  ASSERT_TRUE(group) << group.error().what();
  ASSERT_TRUE((*search)->setText("zzsongbook"));
  ASSERT_TRUE(pane->call("search", {}));
  StringList visibleResources;
  for (std::size_t index = 0; index < (*list)->listSize(); ++index) {
    auto path = (*list)->itemAt(index)->data().toString();
    ASSERT_TRUE(path) << path.error().what();
    visibleResources.append(std::move(*path));
  }
  ASSERT_EQ(visibleResources, (StringList{"/songs/Nested/zzsongbookB.abc", resource, "/songs/zzsongbookZ.abc"}));
  ASSERT_TRUE((*search)->setText("CAFÉ"));
  ASSERT_TRUE(pane->call("search", {}));
  EXPECT_EQ((*list)->listSize(), 0u); // Native case folding is not Unicode folding.
  ASSERT_TRUE((*search)->setText("CAFé"));
  ASSERT_TRUE(pane->call("search", {}));
  EXPECT_EQ((*list)->listSize(), 1u);
  ASSERT_TRUE((*search)->setText("[a]"));
  ASSERT_TRUE(pane->call("search", {}));
  ASSERT_EQ((*list)->listSize(), 1u);
  auto row = (*list)->itemAt(0);
  EXPECT_EQ(row->data(), Json(resource));
  auto label = row->fetchChild<TextBoxWidget>("songName");
  ASSERT_TRUE(label) << label.error().what();
  EXPECT_EQ((*label)->getText(), String("^#bbb;zzsongbook^#7f7;[A]^#bbb;.Café^#7f7;[A]^#bbb;"));
  ASSERT_TRUE((*list)->setSelected(0));
  ASSERT_TRUE((*group)->setText("Band Ω"));
  ASSERT_TRUE(pane->call("btnPlay", {}));
  EXPECT_TRUE(pane->isDismissed());
  EXPECT_TRUE((*domain)->active());
  EXPECT_EQ((*domain)->song(), (Json(JsonObject{{"resource", resource}, {"abc", abc}})));
  EXPECT_EQ((*domain)->timeSource(), std::optional<String>("Band Ω"));
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  EXPECT_EQ((*search)->getText(), String("[a]"));
  EXPECT_EQ((*group)->getText(), String("Band Ω"));
  EXPECT_EQ((*list)->selectedWidget(), row);
  auto closed = pane->sendEvent(KeyDownEvent{Key::Escape, KeyMod::NoMod});
  ASSERT_TRUE(closed) << closed.error().what();
  EXPECT_TRUE(*closed);
  EXPECT_TRUE(pane->isDismissed());
  EXPECT_TRUE((*domain)->active());
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  ASSERT_TRUE((*search)->setText("zzsongbook"));
  ASSERT_TRUE(pane->call("search", {}));
  ASSERT_TRUE((*list)->setSelected(0));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->call("update", {}));
  EXPECT_FALSE((*list)->selectedWidget());
  EXPECT_EQ((*group)->getText(), String("Band Ω"));
  ASSERT_TRUE(File::writeFile(String("X:5\nK:C\nF|\n"), File::relativeTo(overlay, "songs/zzsongbookNew.abc")));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->call("update", {}));
  visibleResources.clear();
  for (std::size_t index = 0; index < (*list)->listSize(); ++index) {
    auto path = (*list)->itemAt(index)->data().toString();
    ASSERT_TRUE(path) << path.error().what();
    visibleResources.append(std::move(*path));
  }
  EXPECT_EQ(visibleResources, (StringList{"/songs/Nested/zzsongbookB.abc", resource,
      "/songs/zzsongbookNew.abc", "/songs/zzsongbookZ.abc"}));
  EXPECT_FALSE((*list)->selectedWidget());
  EXPECT_EQ((*domain)->song(), (Json(JsonObject{{"resource", resource}, {"abc", abc}})));
  EXPECT_EQ((*domain)->timeSource(), std::optional<String>("Band Ω"));
  ASSERT_TRUE((*list)->setSelected(0));
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(File::writeFile(String("X:6\nK:C\nG|\n"), File::relativeTo(overlay, "songs/zzsongbookClosed.abc")));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  visibleResources.clear();
  for (std::size_t index = 0; index < (*list)->listSize(); ++index) {
    auto path = (*list)->itemAt(index)->data().toString();
    ASSERT_TRUE(path);
    visibleResources.append(std::move(*path));
  }
  EXPECT_EQ(visibleResources,
            (StringList{"/songs/Nested/zzsongbookB.abc", resource, "/songs/zzsongbookClosed.abc",
                        "/songs/zzsongbookNew.abc", "/songs/zzsongbookZ.abc"}));
  EXPECT_FALSE((*list)->selectedWidget());
  EXPECT_EQ((*search)->getText(), String("zzsongbook"));
  EXPECT_EQ((*group)->getText(), String("Band Ω"));
  EXPECT_EQ((*domain)->song(), (Json(JsonObject{{"resource", resource}, {"abc", abc}})));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration uiRetainedWindow("LuaUiManager", "RetainedDismissalReopensNativeStateButPlayerExpiryAndShutdownDestroy", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto script = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(script) << script.error().what();
  auto rootCallbacks = LuaBindings::makeRootCallbacks(fixture.root());
  ASSERT_TRUE(rootCallbacks) << rootCallbacks.error().what();
  ASSERT_TRUE(script->setCallbacks("root", *rootCallbacks));
  PaneManager panes(**context);
  std::shared_ptr<ScriptTestPane> instance;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(callbacks.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (!instance) return std::optional<LuaCallbacks>{};
    auto bound = instance->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(callbacks.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (!instance) return std::unexpected(GuiException("Missing retained pane"));
    return panes.displayPane(PaneLayer::Window, instance, {});
  }));
  ASSERT_TRUE(callbacks.registerCallback("destroyPane", [&](String const&) -> std::expected<void, StarException> {
    if (!instance) return {};
    if (panes.isDisplayed(instance)) {
      if (auto dismissed = panes.dismissPane(instance); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    instance.reset();
    return {};
  }));
  ASSERT_TRUE(script->setCallbacks("interface", callbacks));
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  instance = std::move(*created);
  instance->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(instance->setLuaRoot(*luaRoot));
  auto textbox = textEditor(**context, "Retained band Ω");
  ASSERT_TRUE(textbox) << textbox.error().what();
  ASSERT_TRUE(instance->addChild("band", *textbox));
  std::weak_ptr<Pane> released = instance;
  auto opened = script->eval<bool>(R"LUA(
    local definition = root.assetJson("/interface.config:panes.songbook")
    checkpoint = {}
    definitions = {songbook = definition}
    session = {hasPlayer = true, inWorld = true, playerGeneration = 1, worldGeneration = 1}
    manager = UiManager.new(checkpoint, definitions, session)
    return manager:open("songbook")
  )LUA");
  ASSERT_TRUE(opened) << opened.error().what();
  EXPECT_TRUE(*opened);
  ASSERT_TRUE(panes.dismissPane(instance));
  ASSERT_TRUE(script->eval<LuaValue>("manager:update()"));
  EXPECT_FALSE(released.expired());
  EXPECT_FALSE(panes.topPane());
  auto reopened = script->eval<bool>(R"LUA(
    manager = UiManager.new(checkpoint, definitions,
      {hasPlayer = true, inWorld = true, playerGeneration = 1, worldGeneration = 2})
    manager:restore()
    assert(not checkpoint.open.songbook)
    return manager:open("songbook")
  )LUA");
  ASSERT_TRUE(reopened) << reopened.error().what();
  EXPECT_TRUE(*reopened);
  EXPECT_EQ((*textbox)->getText(), String("Retained band Ω"));
  EXPECT_EQ(panes.topPane(), instance);
  ASSERT_TRUE(panes.dismissPane(instance));
  ASSERT_TRUE(script->eval<LuaValue>("manager:update()"));
  auto expired = script->eval<bool>(R"LUA(
    manager:enqueue("$ui", "open", {id = "songbook"})
    manager:suspend({hasPlayer = true, inWorld = true, playerGeneration = 2, worldGeneration = 2})
    return checkpoint.open.songbook == nil and #checkpoint.queue == 0
  )LUA");
  ASSERT_TRUE(expired) << expired.error().what();
  EXPECT_TRUE(*expired);
  EXPECT_TRUE(released.expired());
  for (auto const& operation : {String("manager:close('songbook')"), String("manager:shutdown()"),
      String("manager:register('songbook', definitions.songbook)")}) {
    auto replacement = ScriptTestPane::create(**context);
    ASSERT_TRUE(replacement) << replacement.error().what();
    instance = std::move(*replacement);
    ASSERT_TRUE(instance->setLuaRoot(*luaRoot));
    released = instance;
    // This is an allocated but normally dismissed retained instance.
    ASSERT_TRUE(script->eval<LuaValue>("checkpoint.open.songbook = false"));
    ASSERT_TRUE(script->eval<LuaValue>(operation));
    EXPECT_TRUE(released.expired());
  }
});


const GameTestRegistration songbookCallbackScope("LuaSongbook", "EscapedNativeDomainAndLiveCallbacksRetireWithoutRevivingOnRebind", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto first = Songbook::create(fixture.root(), "human");
  auto second = Songbook::create(fixture.root(), "human");
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(second) << second.error().what();
  auto firstCallbacks = LuaBindings::makeSongbookCallbacks(**first);
  auto secondCallbacks = LuaBindings::makeSongbookCallbacks(**second);
  ASSERT_TRUE(firstCallbacks) << firstCallbacks.error().what();
  ASSERT_TRUE(secondCallbacks) << secondCallbacks.error().what();
  (*luaRoot)->addCallbacks("inheritedSongbook", *firstCallbacks);
  Json song = JsonObject{{"resource", "/songs/scope.abc"}, {"abc", "X:1\nK:C\nC|\n"}};
  LuaBaseComponent component(fixture.root());
  ASSERT_TRUE(component.setLuaRoot(*luaRoot));
  ASSERT_TRUE(component.addCallbacks("songbook", *firstCallbacks));
  ASSERT_TRUE(component.init());
  auto oldPlay = component.context()->getPath<LuaFunction>("songbook.play");
  auto inheritedPlay = component.context()->getPath<LuaFunction>("inheritedSongbook.play");
  ASSERT_TRUE(oldPlay) << oldPlay.error().what();
  ASSERT_TRUE(inheritedPlay) << inheritedPlay.error().what();
  ASSERT_TRUE(component.addCallbacks("liveSongbook", *firstCallbacks));
  auto livePlay = component.context()->getPath<LuaFunction>("liveSongbook.play");
  ASSERT_TRUE(livePlay) << livePlay.error().what();
  ASSERT_TRUE(component.eval<LuaValue>(R"LUA(
    function uninit()
      songbook.play({resource = "/songs/uninit.abc", abc = "X:2\nK:C\nD|\n"}, "uninit band")
    end
  )LUA"));
  ASSERT_TRUE(component.uninit());
  EXPECT_EQ((*first)->timeSource(), std::optional<String>("uninit band"));
  EXPECT_TRUE((*first)->active()); // Shutdown hooks run before scope retirement.
  EXPECT_FALSE(oldPlay->invoke<LuaValue>(song, "retired"));
  EXPECT_FALSE(livePlay->invoke<LuaValue>(song, "retired live"));
  EXPECT_EQ((*first)->timeSource(), std::optional<String>("uninit band"));
  EXPECT_FALSE(inheritedPlay->invoke<LuaValue>(song, "inherited"));
  EXPECT_EQ((*first)->timeSource(), std::optional<String>("uninit band"));
  ASSERT_TRUE(component.removeCallbacks("songbook"));
  ASSERT_TRUE(component.removeCallbacks("liveSongbook"));
  ASSERT_TRUE(component.addCallbacks("songbook", *secondCallbacks));
  ASSERT_TRUE(component.init());
  auto newPlay = component.context()->getPath<LuaFunction>("songbook.play");
  ASSERT_TRUE(newPlay) << newPlay.error().what();
  ASSERT_TRUE(newPlay->invoke<LuaValue>(song, "current"));
  EXPECT_EQ((*second)->song(), song);
  EXPECT_EQ((*second)->timeSource(), std::optional<String>("current"));
  EXPECT_FALSE(oldPlay->invoke<LuaValue>(song, "revived"));
  EXPECT_EQ((*first)->timeSource(), std::optional<String>("uninit band"));
  ASSERT_TRUE(component.eval<LuaValue>("function failScope() error('scope regression') end"));
  testing::TestPartResultArray errors;
  {
    testing::ScopedFakeTestPartResultReporter capture(
      testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &errors);
    auto failed = component.invoke<LuaValue>("failScope");
    ASSERT_FALSE(failed);
    EXPECT_TRUE(failed.error().is<LuaException>());
  }
  EXPECT_EQ(errors.size(), 1);
  EXPECT_FALSE(newPlay->invoke<LuaValue>(song, "error revived"));
  EXPECT_EQ((*second)->timeSource(), std::optional<String>("current"));
  ASSERT_TRUE(component.init());
  auto reinitializedPlay = component.context()->getPath<LuaFunction>("songbook.play");
  ASSERT_TRUE(reinitializedPlay) << reinitializedPlay.error().what();
  ASSERT_TRUE(component.init()); // Explicit reinit also retires escaped values.
  EXPECT_FALSE(reinitializedPlay->invoke<LuaValue>(song, "reinit revived"));
  ASSERT_TRUE(component.uninit());
  std::optional<LuaFunction> destroyedPlay;
  {
    LuaBaseComponent temporary(fixture.root());
    ASSERT_TRUE(temporary.setLuaRoot(*luaRoot));
    ASSERT_TRUE(temporary.addCallbacks("songbook", *secondCallbacks));
    ASSERT_TRUE(temporary.init());
    auto escaped = temporary.context()->getPath<LuaFunction>("songbook.play");
    ASSERT_TRUE(escaped) << escaped.error().what();
    destroyedPlay = std::move(*escaped);
  }
  EXPECT_FALSE(destroyedPlay->invoke<LuaValue>(song, "destroyed revived"));
  EXPECT_EQ((*second)->timeSource(), std::optional<String>("current"));
  LuaUpdateScript updating(fixture.root());
  ASSERT_TRUE(updating.setLuaRoot(*luaRoot));
  ASSERT_TRUE(updating.init());
  auto oldScript = updating.context()->getPath<LuaFunction>("script.setUpdateDelta");
  ASSERT_TRUE(oldScript) << oldScript.error().what();
  ASSERT_TRUE(updating.uninit());
  EXPECT_FALSE(oldScript->invoke<LuaValue>(99));
  ASSERT_TRUE(updating.init());
  auto currentScript = updating.context()->getPath<LuaFunction>("script.setUpdateDelta");
  ASSERT_TRUE(currentScript) << currentScript.error().what();
  ASSERT_TRUE(currentScript->invoke<LuaValue>(7));
  EXPECT_EQ(updating.updateDelta(), 7u);
  EXPECT_FALSE(oldScript->invoke<LuaValue>(99));
  EXPECT_EQ(updating.updateDelta(), 7u);
  ASSERT_TRUE(updating.uninit());
});

const GameTestRegistration scriptReloadDuringEditorCallback("ScriptPane", "ReloadDuringNativeTextChangeDefersRetirementUntilOuterScriptReturns", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto created = ScriptTestPane::create(**context);
  ASSERT_TRUE(created) << created.error().what();
  auto pane = std::move(*created);
  pane->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  std::optional<LuaFunction> escapedGetText;
  LuaCallbacks host;
  ASSERT_TRUE(host.registerCallback("reload", [&]() { return fixture.root().reload(); }));
  ASSERT_TRUE(host.registerCallback("retain", [&](LuaFunction function) {
    escapedGetText = std::move(function);
  }));
  ASSERT_TRUE(pane->addCallbacks("host", std::move(host)));
  auto editor = TextBoxWidget::create(**context, "before", "",
      [&pane](Widget&) -> std::expected<void, StarException> {
    // The same synchronous native callback boundary used by GuiReader's
    // scriptWidgetCallbacks: setText reenters the active pane's Lua component.
    auto changed = pane->call("inputChanged", {});
    if (!changed) return std::unexpected(std::move(changed.error()));
    return {};
  });
  ASSERT_TRUE(editor) << editor.error().what();
  (*editor)->setMaxWidth(-1);
  ASSERT_TRUE(pane->addChild("input", *editor));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  ASSERT_TRUE(pane->evaluate(R"LUA(
    storage.changes = 0
    function inputChanged()
      storage.changes = storage.changes + 1
      storage.nestedRead = widget.getText("input")
    end
    function submit()
      host.retain(widget.getText)
      host.reload()
      widget.setText("input", "after reload")
      storage.completed = widget.getText("input")
      return storage.completed
    end
  )LUA"));
  // Chat command submission reloads assets, then clears its real editor, whose
  // callback reenters Lua. The outer frame must finish using its valid lease.
  auto submitted = pane->call("submit", {});
  ASSERT_TRUE(submitted) << submitted.error().what();
  ASSERT_TRUE(*submitted);
  EXPECT_EQ(**submitted, Json("after reload"));
  ASSERT_TRUE(escapedGetText);
  EXPECT_EQ(escapedGetText->invoke<String>("input"), String("after reload"));
  // The next top-level entry consumes the pending reload. Checkpoint extraction
  // must see the completed outer operation, not an intermediate callback state.
  auto restored = pane->evaluate("return {storage.changes, storage.nestedRead, storage.completed}");
  ASSERT_TRUE(restored) << restored.error().what();
  ASSERT_TRUE(*restored);
  EXPECT_EQ(**restored, (Json(JsonArray{1, "after reload", "after reload"})));
  EXPECT_FALSE(escapedGetText->invoke<String>("input"));
  EXPECT_EQ((*editor)->getText(), String("after reload"));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration uiGlobalActions("LuaUiManager", "LazyGlobalToggleRespectsNativeModalFocusRepeatAndReload", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto script = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(script) << script.error().what();
  PaneManager panes(**context);
  std::shared_ptr<ScriptTestPane> instance;
  bool modalMenu = false;
  unsigned created = 0;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(callbacks.registerCallback("paneLayerOccupied", [&](String const&) {
    return static_cast<bool>(panes.topPane({PaneLayer::ModalWindow}));
  }));
  ASSERT_TRUE(callbacks.registerCallback("paneInputTargets", [&] {
    if (!instance || panes.topPane() != instance) return JsonArray{};
    return modalMenu ? JsonArray{"EscapeDialog"} : JsonArray{"probe"};
  }));
  ASSERT_TRUE(callbacks.registerCallback("createPane", [&](String const&, Json const&, String const&) -> std::expected<void, StarException> {
    auto result = ScriptTestPane::create(**context);
    if (!result) return std::unexpected(std::move(result.error()));
    instance = std::move(*result);
    instance->setAnchor(PaneAnchor::BottomLeft);
    ++created;
    return instance->setLuaRoot(*luaRoot);
  }));
  ASSERT_TRUE(callbacks.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (!instance) return std::optional<LuaCallbacks>{};
    auto bound = instance->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(callbacks.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (panes.isDisplayed(instance)) return instance->show();
    auto displayed = panes.displayPane(modalMenu ? PaneLayer::ModalWindow : PaneLayer::Window, instance, {});
    if (!displayed) return std::unexpected(std::move(displayed.error()));
    if (modalMenu) {
      // Exercise the production action handler without initializing renderer-backed labels.
      auto loaded = instance->evaluate("require '/interface/opensb/escapemenu/escapemenu.lua'");
      if (!loaded) return std::unexpected(std::move(loaded.error()));
    }
    return {};
  }));
  ASSERT_TRUE(callbacks.registerCallback("destroyPane", [&](String const&) -> std::expected<void, StarException> {
    if (instance && panes.isDisplayed(instance)) {
      if (auto dismissed = panes.dismissPane(instance); !dismissed)
        return std::unexpected(std::move(dismissed.error()));
    }
    instance.reset();
    return {};
  }));
  ASSERT_TRUE(callbacks.registerCallback("callPane", [&](String const&, String const& method, JsonArray const& arguments) {
    return instance->call(method, arguments);
  }));
  ASSERT_TRUE(script->setCallbacks("interface", callbacks));
  ASSERT_TRUE(script->eval<LuaValue>(R"LUA(
    state = {}
    definitions = {probe = {config = {}, scope = "player", retain = true, globalActions = {Probe = "toggle"}}}
    session = {hasPlayer = true, playerGeneration = 1, worldGeneration = 1}
    engineAction = "Probe"
    manager = UiManager.new(state, definitions, session)
    function press(captured)
      return manager:input({type = "keyDown", key = 7, keyboardCaptured = captured, actions = {engineAction}})
    end
    function release()
      return manager:input({type = "keyUp", key = 7, actions = {}})
    end
  )LUA"));
  auto modal = InputOrderPane::create(**context, false);
  ASSERT_TRUE(modal) << modal.error().what();
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, *modal, {}));
  auto blocked = script->eval<bool>("return press(false)");
  ASSERT_TRUE(blocked) << blocked.error().what();
  EXPECT_FALSE(*blocked);
  EXPECT_EQ(created, 0u);
  ASSERT_TRUE(panes.dismissPane(*modal));
  auto focused = InputOrderPane::create(**context, false);
  ASSERT_TRUE(focused) << focused.error().what();
  auto editor = textEditor(**context, "");
  ASSERT_TRUE(editor) << editor.error().what();
  ASSERT_TRUE((*focused)->addChild("editor", *editor));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, *focused, {}));
  ASSERT_TRUE((*editor)->focus());
  auto captured = panes.keyboardCapturedPane();
  ASSERT_TRUE(captured) << captured.error().what();
  EXPECT_EQ(*captured, *focused);
  blocked = script->eval<bool>("return press(true)");
  ASSERT_TRUE(blocked) << blocked.error().what();
  EXPECT_FALSE(*blocked);
  EXPECT_EQ(created, 0u);
  ASSERT_TRUE((*editor)->blur());
  auto opened = script->eval<bool>("return press(false)");
  ASSERT_TRUE(opened) << opened.error().what();
  EXPECT_TRUE(*opened);
  EXPECT_EQ(created, 1u);
  EXPECT_EQ(panes.topPane(), instance);
  // Reconstruct the production manager checkpoint while a key remains held.
  auto repeated = script->eval<bool>("manager = UiManager.new(state, definitions, session); manager:restore(); return press(false)");
  ASSERT_TRUE(repeated) << repeated.error().what();
  EXPECT_TRUE(*repeated);
  EXPECT_FALSE(instance->isDismissed());
  ASSERT_TRUE(script->eval<bool>("return release()"));
  ASSERT_TRUE(script->eval<bool>("return press(false)"));
  EXPECT_TRUE(instance->isDismissed());
  ASSERT_TRUE(panes.update(0));
  ASSERT_TRUE(script->eval<LuaValue>("manager:update(); release()"));
  ASSERT_TRUE(script->eval<bool>("return press(false)"));
  EXPECT_EQ(created, 1u);
  EXPECT_FALSE(instance->isDismissed());
  ASSERT_TRUE(script->eval<LuaValue>("release(); manager:suspend({hasPlayer=true, playerGeneration=2})"));
  EXPECT_FALSE(instance);
  EXPECT_EQ(panes.topPane(), *focused);
  ASSERT_TRUE(panes.dismissAllPanes());

  // A remapped opener need not also be GuiClose. Its local action must close
  // its own modal while unrelated modal barriers still block global opening.
  modalMenu = true;
  auto rootCallbacks = LuaBindings::makeRootCallbacks(fixture.root());
  ASSERT_TRUE(rootCallbacks);
  ASSERT_TRUE(script->setCallbacks("root", *rootCallbacks));
  ASSERT_TRUE(script->eval<LuaValue>(R"LUA(
    state = {}
    definitions = {EscapeDialog = root.assetJson("/interface.config:panes.EscapeDialog")}
    engineAction = "InterfaceEscapeMenu"
    manager = UiManager.new(state, definitions, session)
  )LUA"));
  auto menuOpened = script->eval<bool>("return press(false)");
  ASSERT_TRUE(menuOpened);
  EXPECT_TRUE(*menuOpened);
  EXPECT_FALSE(instance->isDismissed());
  ASSERT_TRUE(script->eval<bool>("return press(false)"));
  EXPECT_FALSE(instance->isDismissed());
  ASSERT_TRUE(script->eval<bool>("return release()"));
  auto menuClosed = script->eval<bool>("return press(false)");
  ASSERT_TRUE(menuClosed);
  EXPECT_TRUE(*menuClosed);
  EXPECT_TRUE(instance->isDismissed());
  ASSERT_TRUE(panes.update(0));
  ASSERT_TRUE(script->eval<LuaValue>("manager:update(); release()"));
  ASSERT_TRUE(script->eval<bool>("return press(false)"));
  EXPECT_FALSE(instance->isDismissed());
  EXPECT_EQ(created, 2u);
  ASSERT_TRUE(script->eval<LuaValue>("release(); manager:shutdown()"));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration uiPausePolicy("LuaUiManager", "OnlyDisplayedPauseMenusPauseRegardlessOfOtherModals", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto script = (*luaRoot)->createContext("/interface/opensb/ui/controller.lua");
  ASSERT_TRUE(script) << script.error().what();
  PaneManager panes(**context);
  std::shared_ptr<Pane> escape, options;
  LuaCallbacks callbacks;
  ASSERT_TRUE(callbacks.registerCallback("paneDisplayed", [&](String const& id) {
    auto pane = id == "EscapeDialog" ? escape : id == "Options" ? options : nullptr;
    return pane && pane->isDisplayed();
  }));
  ASSERT_TRUE(script->setCallbacks("interface", callbacks));
  auto checkPause = [&](bool expected) {
    auto paused = script->eval<bool>("return pauseRequested()");
    ASSERT_TRUE(paused) << paused.error().what();
    EXPECT_EQ(*paused, expected);
  };
  checkPause(false);

  auto createPane = [&]() -> std::expected<std::shared_ptr<Pane>, StarException> {
    auto pane = Pane::create(**context);
    if (!pane) return std::unexpected(std::move(pane.error()));
    (*pane)->setAnchor(PaneAnchor::BottomLeft);
    return *pane;
  };
  auto escapePane = createPane();
  auto optionsPane = createPane();
  auto otherModal = createPane();
  ASSERT_TRUE(escapePane);
  ASSERT_TRUE(optionsPane);
  ASSERT_TRUE(otherModal);
  escape = *escapePane;
  options = *optionsPane;
  checkPause(false);
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, *otherModal, {}));
  checkPause(false);

  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, escape, {}));
  checkPause(true);
  ASSERT_TRUE(escape->hide());
  checkPause(true);
  ASSERT_TRUE(panes.dismissPane(escape));
  checkPause(false);

  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, options, {}));
  checkPause(true);
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, escape, {}));
  checkPause(true);
  ASSERT_TRUE(panes.dismissPane(options));
  checkPause(true);
  ASSERT_TRUE(panes.dismissPane(escape));
  checkPause(false);

  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, options, {}));
  checkPause(true);
  ASSERT_TRUE(panes.dismissPane(options));
  checkPause(false);
  ASSERT_TRUE(panes.dismissPane(*otherModal));
});

const GameTestRegistration codexPresentation("LuaCodex", "DomainIdentityReadReorderPageBoundsModalDeferralAndReload", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "interface")));
  String firstPath = File::relativeTo(overlay, "interface/zzcodex-one.codex");
  ASSERT_TRUE(File::writeFile(String(R"JSON({
    "id":"zzcodex-one","title":"Duplicate","species":"apex","icon":"codex/iconBook.png",
    "contentPages":["First α","Second β"]
  })JSON"), firstPath));
  ASSERT_TRUE(File::writeFile(String(R"JSON({
    "id":"zzcodex-two","title":"Duplicate","species":"apex","contentPages":["Other book"]
  })JSON"), File::relativeTo(overlay, "interface/zzcodex-two.codex")));
  ASSERT_TRUE(File::writeFile(String(R"JSON({
    "id":"zzcodex-empty","title":"Empty","contentPages":[]
  })JSON"), File::relativeTo(overlay, "interface/zzcodex-empty.codex")));
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("codex.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot) << createdRoot.error().what();
  auto& root = **createdRoot;
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{
      {"GuiClose", JsonArray{JsonObject{{"type", "key"}, {"value", "Esc"}, {"mods", JsonArray{}}}}}}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto player = (*factory)->create();
  ASSERT_TRUE(player) << player.error().what();
  auto domain = (*player)->codexes();
  auto first = domain->learnCodex("zzcodex-one", false);
  auto second = domain->learnCodex("zzcodex-two", false);
  auto empty = domain->learnCodex("zzcodex-empty", true);
  ASSERT_TRUE(first) << first.error().what();
  ASSERT_TRUE(second) << second.error().what();
  ASSERT_TRUE(empty) << empty.error().what();
  ASSERT_TRUE(*first);
  ASSERT_TRUE(*second);
  ASSERT_TRUE(*empty);
  EXPECT_EQ((*first)->icon(), String("/interface/codex/iconBook.png"));
  auto playerCallbacks = LuaBindings::makePlayerCallbacks(root, **player);
  ASSERT_TRUE(playerCallbacks) << playerCallbacks.error().what();
  auto assets = root.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto config = (*assets)->json("/interface/opensb/codex/codex.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig) << paneConfig.error().what();
  paneConfig->set("gui", JsonObject{});
  auto createdPane = BaseScriptPane::create(**context, *paneConfig);
  ASSERT_TRUE(createdPane) << createdPane.error().what();
  auto pane = std::move(*createdPane);
  pane->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  // Actual native controls and domain callbacks; only renderer-dependent label
  // drawing is replaced by the same widget.setText-capable native TextBox.
  for (auto const& name : {String("pageText"), String("titleLabel"), String("pageLabel"), String("pageNum"), String("selectLabel")}) {
    auto text = textEditor(**context, "");
    ASSERT_TRUE(text) << text.error().what();
    (*text)->setMaxWidth(-1);
    ASSERT_TRUE(pane->addChild(name, *text));
  }
  for (auto const& name : {String("prevButton"), String("nextButton")}) {
    auto button = ButtonWidget::create(**context);
    ASSERT_TRUE(button) << button.error().what();
    ASSERT_TRUE(pane->addChild(name, *button));
  }
  auto group = std::make_shared<ButtonGroupWidget>(**context);
  for (auto const& entry : {std::pair{-1, String("apex")}, std::pair{5, String("other")}}) {
    auto button = ButtonWidget::create(**context);
    ASSERT_TRUE(button) << button.error().what();
    (*button)->setData(JsonObject{{"species", entry.second}});
    (*button)->setButtonGroup(group, entry.first);
    ASSERT_TRUE(group->addChild(entry.second, *button));
  }
  ASSERT_TRUE(group->select(-1));
  ASSERT_TRUE(pane->addChild("speciesTabs", group));
  group->setCallback([&](Widget&) -> std::expected<void, StarException> {
    auto result = pane->call("updateSpecies", {});
    if (!result) return std::unexpected(std::move(result.error()));
    return {};
  });
  auto scroll = std::make_shared<Widget>(**context);
  auto schema = Json::parseJson(R"JSON({
    "spacing":[0,0],"memberSize":[160,12],
    "listTemplate":{
      "bookName":{"type":"textbox","position":[0,0],"maxWidth":-1},
      "bookIcon":{"type":"image","position":[0,0],"file":""}
    }
  })JSON");
  ASSERT_TRUE(schema) << schema.error().what();
  auto list = ListWidget::create(**context, *schema);
  ASSERT_TRUE(list) << list.error().what();
  (*list)->registerMemberCallback("bookName", [](Widget&) -> std::expected<void, StarException> { return {}; });
  (*list)->setCallback([&](Widget&) -> std::expected<void, StarException> {
    auto result = pane->call("selectCodex", {});
    if (!result) return std::unexpected(std::move(result.error()));
    return {};
  });
  ASSERT_TRUE(scroll->addChild("bookList", *list));
  ASSERT_TRUE(pane->addChild("scrollArea", scroll));
  PaneManager panes(**context);
  auto controller = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(controller) << controller.error().what();
  auto rootCallbacks = LuaBindings::makeRootCallbacks(root);
  ASSERT_TRUE(rootCallbacks) << rootCallbacks.error().what();
  ASSERT_TRUE(controller->setCallbacks("root", *rootCallbacks));
  ASSERT_TRUE(controller->setCallbacks("player", *playerCallbacks));
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(interface.registerCallback("paneLayerOccupied", [&](String const&) {
    return static_cast<bool>(panes.topPane({PaneLayer::ModalWindow}));
  }));
  ASSERT_TRUE(interface.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    auto result = pane->makePaneCallbacks();
    if (!result) return std::unexpected(std::move(result.error()));
    return std::optional<LuaCallbacks>(std::move(*result));
  }));
  ASSERT_TRUE(interface.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (panes.isDisplayed(pane)) return pane->show();
    return panes.displayPane(PaneLayer::Window, pane, {});
  }));
  ASSERT_TRUE(interface.registerCallback("callPane", [&](String const&, String const& method, JsonArray const& args) {
    return pane->call(method, args);
  }));
  ASSERT_TRUE(controller->setCallbacks("interface", interface));
  auto initialized = controller->eval<LuaValue>(R"LUA(
    require "/interface/opensb/codex/unread.lua"
    definitions = {codex = root.assetJson("/interface.config:panes.codex")}
    session = {hasPlayer=true, playerGeneration=1, worldGeneration=1}
    checkpoint = {session = session}
    manager = UiManager.new(checkpoint, definitions, session)
    manager:restore()
  )LUA");
  ASSERT_TRUE(initialized) << initialized.error().what();
  auto modal = InputOrderPane::create(**context, false);
  ASSERT_TRUE(modal) << modal.error().what();
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, *modal, {}));
  auto pendingId = domain->firstNewCodex()->id();
  ASSERT_TRUE(controller->eval<LuaValue>("CodexUnread.update(manager)"));
  EXPECT_FALSE(panes.isDisplayed(pane));
  EXPECT_FALSE(domain->codexRead(pendingId));
  ASSERT_TRUE(panes.dismissPane(*modal));
  ASSERT_TRUE(controller->eval<LuaValue>("CodexUnread.update(manager)"));
  EXPECT_TRUE(panes.isDisplayed(pane));
  EXPECT_TRUE(domain->codexRead(pendingId));
  ASSERT_TRUE((*list)->selectedWidget());
  EXPECT_EQ((*list)->selectedWidget()->data(), Json(pendingId));
  // Selecting either identical title must retain ID, even when marking it read
  // changes unread-first domain order and synchronously rebuilds the real list.
  auto remainingId = domain->firstNewCodex()->id();
  ASSERT_EQ((*list)->listSize(), 2u);
  EXPECT_EQ((*list)->itemAt(0)->data(), Json(remainingId));
  ASSERT_TRUE((*list)->setSelected(0));
  EXPECT_TRUE(domain->codexRead(remainingId));
  ASSERT_TRUE((*list)->selectedWidget());
  EXPECT_EQ((*list)->selectedWidget()->data(), Json(remainingId));
  ASSERT_TRUE(pane->call("showCodex", {"zzcodex-one"}));
  EXPECT_TRUE(domain->codexRead("zzcodex-one"));
  ASSERT_TRUE((*list)->selectedWidget());
  EXPECT_EQ((*list)->selectedWidget()->data(), Json("zzcodex-one"));
  ASSERT_TRUE(pane->call("showCodex", {"zzcodex-two"}));
  EXPECT_TRUE(domain->codexRead("zzcodex-two"));
  EXPECT_EQ((*list)->selectedWidget()->data(), Json("zzcodex-two"));
  ASSERT_TRUE(pane->call("showCodex", {"zzcodex-one"}));
  ASSERT_TRUE(pane->call("nextButton", {}));
  ASSERT_TRUE(pane->call("nextButton", {}));
  auto text = pane->fetchChild<TextBoxWidget>("pageText");
  ASSERT_TRUE(text);
  EXPECT_EQ((*text)->getText(), String("Second β"));
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  EXPECT_EQ((*text)->getText(), String("Second β"));
  EXPECT_EQ((*list)->selectedWidget()->data(), Json("zzcodex-one"));
  // Scope survival on warp/controller reload cannot re-consume any read entry.
  ASSERT_TRUE(controller->eval<LuaValue>(R"LUA(
    manager = UiManager.new(checkpoint, definitions, {hasPlayer=true, playerGeneration=1, worldGeneration=2})
    manager:restore()
    CodexUnread.update(manager)
  )LUA"));
  EXPECT_EQ((*text)->getText(), String("Second β"));
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:update(); CodexUnread.update(manager)"));
  EXPECT_FALSE(panes.isDisplayed(pane));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:open('codex')"));
  ASSERT_TRUE(pane->call("showCodex", {"zzcodex-empty"}));
  ASSERT_TRUE(pane->call("nextButton", {}));
  ASSERT_TRUE(pane->call("prevButton", {}));
  EXPECT_EQ((*text)->getText(), String(""));
  auto pageNumber = pane->fetchChild<TextBoxWidget>("pageNum");
  ASSERT_TRUE(pageNumber) << pageNumber.error().what();
  EXPECT_EQ((*pageNumber)->getText(), String("0 of 0"));
  ASSERT_TRUE(group->select(-1));
  EXPECT_FALSE((*list)->selectedWidget());
  EXPECT_EQ((*text)->getText(), String(""));
  ASSERT_TRUE(pane->call("showCodex", {"zzcodex-one"}));
  ASSERT_TRUE(pane->call("nextButton", {}));
  ASSERT_TRUE(File::writeFile(String(R"JSON({
    "id":"zzcodex-one","title":"Duplicate","species":"apex","icon":"codex/iconBook.png",
    "contentPages":["Reloaded only page"]
  })JSON"), firstPath));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->call("update", {}));
  // Known content is held by the native player domain, just like the old
  // reader, rather than replaced by a newer database generation on reload.
  EXPECT_EQ((*text)->getText(), String("Second β"));
  EXPECT_TRUE(domain->codexRead("zzcodex-one"));
  // Compatibility API still returns title; the additive API returns identity.
  EXPECT_TRUE(domain->markCodexUnread("zzcodex-two"));
  auto stable = controller->eval<Json>("return {player.getNewCodex(), player.getNewCodexId(), player.getCodex('missing') == nil}");
  ASSERT_TRUE(stable) << stable.error().what();
  EXPECT_EQ(*stable, (Json(JsonArray{"Duplicate", "zzcodex-two", true})));
  ASSERT_TRUE(File::remove(File::relativeTo(overlay, "interface/zzcodex-two.codex")));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(controller->eval<LuaValue>("CodexUnread.update(manager)"));
  EXPECT_TRUE(domain->codexRead("zzcodex-two"));
  EXPECT_EQ((*text)->getText(), String("Other book"));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration questTrackerPresentation("LuaQuestTracker", "EffectiveDomainOptionalValuesNativeBoundsSpringAndReload", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "interface")));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "interface/questtracker")));
  ASSERT_TRUE(File::writeFile(String(R"JSON({
    "incompleteObjectiveTemplate":"P[<objective>] <objective>\n",
    "completeObjectiveTemplate":"C(<objective>)\n"
  })JSON"), File::relativeTo(overlay, "interface/questtracker/questtracker.config.patch")));
  ASSERT_TRUE(File::writeFile(String(R"JSON({"id":"zztracker","showInLog":true})JSON"),
      File::relativeTo(overlay, "visible.questtemplate")));
  ASSERT_TRUE(File::writeFile(String(R"JSON({"id":"zztracker-hidden","showInLog":false})JSON"),
      File::relativeTo(overlay, "hidden.questtemplate")));
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("tracker.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot) << createdRoot.error().what();
  auto& root = **createdRoot;
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration) << configuration.error().what();
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{
      {"GuiClose", JsonArray{JsonObject{{"type", "key"}, {"value", "Esc"}, {"mods", JsonArray{}}}}}}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto player = (*factory)->create();
  ASSERT_TRUE(player) << player.error().what();
  auto versions = root.versioningDatabase();
  ASSERT_TRUE(versions) << versions.error().what();
  JsonObject quests;
  for (auto const& templateId : {String("zztracker"), String("zztracker-hidden")}) {
    auto arc = QuestArcDescriptor::fromJson(root, JsonObject{{"quests", JsonArray{
        JsonObject{{"questId", templateId}, {"templateId", templateId}, {"parameters", JsonObject{}}}}}});
    ASSERT_TRUE(arc) << arc.error().what();
    auto storedArc = arc->diskStore(root);
    ASSERT_TRUE(storedArc) << storedArc.error().what();
    auto storedQuest = (*versions)->makeCurrentVersionedJson("Quest", JsonObject{
        {"state", "Active"}, {"arc", *storedArc}, {"arcPos", 0}, {"parameters", JsonObject{}},
        {"money", 0}, {"rewards", JsonArray{}}, {"lastUpdatedOn", 0}});
    ASSERT_TRUE(storedQuest) << storedQuest.error().what();
    quests.set(templateId, storedQuest->toJson());
  }
  auto manager = (*player)->questManager();
  ASSERT_TRUE(manager->diskLoad(JsonObject{{"quests", quests}, {"currentQuest", "zztracker"}}));
  auto quest = manager->getQuest("zztracker");
  ASSERT_TRUE(quest) << quest.error().what();
  JsonArray objectives{JsonArray{"Done <literal>", true}, JsonArray{"First pending", false}, JsonArray{"Later pending", false}};
  (*quest)->setObjectiveList(objectives);
  (*quest)->setProgress(0.0f);
  (*quest)->setCompassDirection(0.0f);
  auto playerCallbacks = LuaBindings::makePlayerCallbacks(root, **player);
  ASSERT_TRUE(playerCallbacks) << playerCallbacks.error().what();
  auto probe = (*luaRoot)->createContext(StringList{});
  ASSERT_TRUE(probe) << probe.error().what();
  ASSERT_TRUE(probe->setCallbacks("player", *playerCallbacks));
  auto compact = probe->eval<Json>("return player.currentQuestView()");
  ASSERT_TRUE(compact) << compact.error().what();
  EXPECT_EQ(*compact, (Json(JsonObject{{"id", "zztracker"}, {"objectives", objectives},
      {"progress", 0.0}, {"compassDirection", 0.0}})));
  // A tracked active quest may deliberately be excluded from the effective UI
  // selection. Do not substitute currentQuestId for QuestManager.currentQuest.
  manager->setAsTracked("zztracker-hidden");
  auto hidden = probe->eval<Json>("return {player.currentQuestId(), player.currentQuestView() == nil}");
  ASSERT_TRUE(hidden) << hidden.error().what();
  EXPECT_EQ(*hidden, (Json(JsonArray{"zztracker-hidden", true})));
  manager->setAsTracked("zztracker");

  auto assets = root.assets();
  ASSERT_TRUE(assets) << assets.error().what();
  auto style = (*assets)->json("/interface/questtracker/questtracker.config");
  ASSERT_TRUE(style) << style.error().what();
  auto layoutJson = style->getObject("paneLayout");
  ASSERT_TRUE(layoutJson) << layoutJson.error().what();
  auto progressLayout = style->query("paneLayout.questProgress");
  ASSERT_TRUE(progressLayout) << progressLayout.error().what();
  auto headlessProgress = progressLayout->set("background", "");
  ASSERT_TRUE(headlessProgress) << headlessProgress.error().what();
  layoutJson->set("questProgress", *headlessProgress);
  layoutJson->erase("panefeature");
  layoutJson->erase("lblQuestObjectiveList");
  auto config = (*assets)->json("/interface/opensb/questtracker/questtracker.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig) << paneConfig.error().what();
  paneConfig->set("gui", *layoutJson);
  auto createdPane = ScriptTestPane::create(**context, Json(*paneConfig));
  ASSERT_TRUE(createdPane) << createdPane.error().what();
  auto pane = std::move(*createdPane);
  pane->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  LuaCallbacks interface;
  bool hasPlayer = true;
  ASSERT_TRUE(interface.registerCallback("session", [&] { return JsonObject{{"hasPlayer", hasPlayer}}; }));
  ASSERT_TRUE(pane->addCallbacks("interface", interface));
  // Headless GuiContext has no font painter. Keep actual image/stretch/progress
  // widgets and a native text-capable TextBox; supply its measured bounds here.
  // Real label wrapping and pixels are covered by the renderer-backed GUI run.
  auto label = textEditor(**context, "");
  ASSERT_TRUE(label) << label.error().what();
  (*label)->setMaxWidth(-1);
  (*label)->setPosition({-125, -3});
  (*label)->setSize({110, 45});
  ASSERT_TRUE(pane->addChild("lblQuestObjectiveList", *label));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Hud, pane, {}));
  auto publicCallbacks = pane->makePaneCallbacks();
  ASSERT_TRUE(publicCallbacks) << publicCallbacks.error().what();
  ASSERT_TRUE(probe->setCallbacks("tracker", *publicCallbacks));
  ASSERT_TRUE(probe->eval<LuaValue>("cachedWidgets = tracker.toWidget()"));
  auto frame = pane->fetchChild<ImageWidget>("imgFrame");
  auto expanded = pane->fetchChild<ImageStretchWidget>("imgFrameExpanded");
  auto progress = pane->fetchChild<ProgressWidget>("questProgress");
  auto progressFrame = pane->fetchChild<ImageWidget>("imgProgressFrame");
  auto compassFrame = pane->fetchChild<ImageWidget>("imgCompassFrame");
  ASSERT_TRUE(frame && *frame);
  ASSERT_TRUE(expanded && *expanded);
  ASSERT_TRUE(progress && *progress);
  ASSERT_TRUE(progressFrame && *progressFrame);
  ASSERT_TRUE(compassFrame && *compassFrame);
  EXPECT_EQ((*label)->getText(), String("P[First pending] First pending\n"));
  EXPECT_TRUE((*progress)->visibility());
  EXPECT_TRUE((*compassFrame)->visibility());
  EXPECT_EQ((*progress)->drawingOffset(), (Vec2I{0, -(*frame)->size()[1]}));
  auto frameImage = (*compassFrame)->image();
  ASSERT_TRUE(frameImage) << frameImage.error().what();
  EXPECT_EQ(*frameImage, *style->getString("expandedCompassFrameImage"));

  // Widget hit testing uses the drawing area established by rendering. Exercise
  // real pointer boundaries in the GL smoke, not an undrawn headless widget.
  ASSERT_TRUE(pane->evaluate("storage.expanded = true"));
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_EQ((*label)->getText(), String("C(Done <literal>)\nP[First pending] First pending\nP[Later pending] Later pending\n"));
  EXPECT_FALSE((*frame)->visibility());
  EXPECT_TRUE((*expanded)->visibility());
  EXPECT_EQ((*expanded)->size()[1], 55);
  EXPECT_EQ((*expanded)->relativePosition(), (Vec2I{-131, -55}));
  EXPECT_EQ((*progress)->drawingOffset(), (Vec2I{0, -55}));
  (*label)->setSize({110, 75});
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_EQ((*expanded)->size()[1], 85);
  EXPECT_EQ((*expanded)->relativePosition()[1], -85);
  EXPECT_EQ((*progressFrame)->drawingOffset(), (Vec2I{0, -85}));
  (*quest)->setObjectiveList(JsonArray{});
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_FALSE((*label)->visibility());
  EXPECT_EQ((*expanded)->size()[1], 85); // Empty is not absent.
  (*quest)->setObjectiveList({});
  (*quest)->setProgress({});
  (*quest)->setCompassDirection({});
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_EQ((*label)->getText(), String(""));
  EXPECT_FALSE((*progress)->visibility());
  EXPECT_FALSE((*compassFrame)->visibility());
  auto absent = probe->eval<Json>("return player.currentQuestView()");
  ASSERT_TRUE(absent) << absent.error().what();
  EXPECT_EQ(*absent, (Json(JsonObject{{"id", "zztracker"}})));
  (*quest)->setObjectiveList(JsonArray{JsonArray{"First complete", true}, JsonArray{"Second complete", true}});
  ASSERT_TRUE(pane->tick(1.0f / 60));
  ASSERT_TRUE(pane->evaluate("storage.expanded = false"));
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_EQ((*label)->getText(), String("C(First complete)\n"));
  auto close = pane->sendEvent(KeyDownEvent{Key::Escape, KeyMod::NoMod});
  ASSERT_TRUE(close) << close.error().what();
  EXPECT_FALSE(*close);
  EXPECT_FALSE(pane->isDismissed());
  (*quest)->setCompassDirection(0.0f);
  ASSERT_TRUE(pane->tick(1.0f / 60));
  auto compactCompass = (*compassFrame)->image();
  ASSERT_TRUE(compactCompass) << compactCompass.error().what();
  EXPECT_EQ(*compactCompass, *style->getString("compassFrameImage"));

  auto acceleration = style->getFloat("compassAcceleration");
  auto friction = style->getFloat("compassFriction");
  ASSERT_TRUE(acceleration && friction);
  float direction = 0, speed = 0;
  for (float target : {6.2f, 0.01f, 3.2f, -3.2f, 0.0f}) {
    (*quest)->setCompassDirection(target);
    for (int step = 0; step < 13; ++step) {
      float diff = pfmod(target - direction, static_cast<float>(std::numbers::pi) * 2);
      if (std::abs(diff) < *acceleration && std::abs(speed) < *acceleration) {
        speed = 0;
        direction = target;
      } else {
        if (std::abs(diff) > std::numbers::pi)
          diff -= std::copysign(2 * std::numbers::pi, diff);
        float ratio = std::abs(diff) / std::numbers::pi;
        float change = *acceleration * ratio;
        speed += std::copysign(change, diff);
        speed -= speed * *friction;
        direction += speed;
      }
      if (step == 6)
        ASSERT_TRUE(root.reload());
      ASSERT_TRUE(pane->tick(step % 2 ? 0.001f : 0.1f));
      auto state = pane->evaluate("return {storage.compassDirection, storage.compassSpeed}");
      ASSERT_TRUE(state && *state);
      EXPECT_EQ(**state, (Json(JsonArray{direction, speed})));
    }
  }
  (*quest)->setCompassDirection({});
  ASSERT_TRUE(pane->evaluate("storage.expanded = true"));
  ASSERT_TRUE(pane->tick(1.0f / 60));
  auto spring = pane->evaluate("return {storage.compassDirection, storage.compassSpeed}");
  ASSERT_TRUE(spring && *spring);
  hasPlayer = false;
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_FALSE(pane->visibility());
  EXPECT_FALSE((*frame)->active());
  EXPECT_FALSE((*expanded)->active());
  EXPECT_FALSE((*progressFrame)->active());
  EXPECT_FALSE((*compassFrame)->active());
  // The same native object/children and escaped callbacks survive a domain
  // rebind, no-quest interval and script reload, as session overlays require.
  auto replacement = (*factory)->create();
  ASSERT_TRUE(replacement) << replacement.error().what();
  auto replacementCallbacks = LuaBindings::makePlayerCallbacks(root, **replacement);
  ASSERT_TRUE(replacementCallbacks) << replacementCallbacks.error().what();
  ASSERT_TRUE(pane->dismissed());
  ASSERT_TRUE(pane->removeCallbacks("player"));
  ASSERT_TRUE(pane->addCallbacks("player", *replacementCallbacks));
  hasPlayer = true;
  ASSERT_TRUE(pane->displayed());
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_FALSE(pane->visibility());
  auto restoredSpring = pane->evaluate("return {storage.compassDirection, storage.compassSpeed}");
  ASSERT_TRUE(restoredSpring && *restoredSpring);
  EXPECT_EQ(**restoredSpring, **spring);
  auto retainedPublicHandle = probe->eval<Json>(
      "return {tracker.isDisplayed(), cachedWidgets.active('imgFrame'), cachedWidgets.active('imgFrameExpanded')}");
  ASSERT_TRUE(retainedPublicHandle) << retainedPublicHandle.error().what();
  EXPECT_EQ(*retainedPublicHandle, (Json(JsonArray{true, false, false})));
  ASSERT_TRUE(pane->dismissed());
  ASSERT_TRUE(pane->removeCallbacks("player"));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  ASSERT_TRUE(pane->displayed());
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_TRUE(pane->visibility());
  EXPECT_EQ(pane->fetchChild<ImageWidget>("imgFrame")->get(), frame->get());
  EXPECT_EQ((*label)->getText(), String("C(First complete)\nC(Second complete)\n"));
  auto retainedExpanded = probe->eval<bool>("return cachedWidgets.active('imgFrameExpanded') and not cachedWidgets.active('imgFrame')");
  ASSERT_TRUE(retainedExpanded) << retainedExpanded.error().what();
  EXPECT_TRUE(*retainedExpanded);
  ASSERT_TRUE((*quest)->fail());
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_FALSE(pane->visibility());
  auto finished = probe->eval<Json>("return {player.currentQuestId(), player.currentQuestView() == nil}");
  ASSERT_TRUE(finished) << finished.error().what();
  EXPECT_EQ(*finished, (Json(JsonArray{"zztracker", true})));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration drawableGeometry("LuaDrawable", "SerializedImagesPreserveGeometryWithoutDoubleCentering", [](GameTest& fixture) {
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto context = (*luaRoot)->createContext(StringList{});
  ASSERT_TRUE(context) << context.error().what();
  auto metadata = fixture.root().imageMetadataDatabase();
  ASSERT_TRUE(metadata) << metadata.error().what();
  auto native = Drawable::create(**metadata, JsonObject{
      {"image", "/objects/outpost/techlab/techlabscientistportrait.png"},
      {"position", JsonArray{7, 11}}, {"mirrored", true}, {"scale", 2}});
  ASSERT_TRUE(native) << native.error().what();
  auto imagePoint = [](Drawable const& drawable, Vec2F point) {
    return std::get<Drawable::ImagePart>(drawable.part).transformation.transformVec2(point) + drawable.position;
  };
  auto expectGeometry = [&](Drawable const& actual) {
    ASSERT_TRUE(std::holds_alternative<Drawable::ImagePart>(actual.part));
    for (auto point : {Vec2F{0, 0}, Vec2F{36, 0}, Vec2F{0, 36}})
      EXPECT_EQ(imagePoint(actual, point), imagePoint(*native, point));
  };

  // QuestStarted portraits cross the native Drawable -> Lua -> Drawable boundary.
  ASSERT_TRUE(context->set("portrait", *native));
  auto portrait = context->eval<Drawable>("return portrait");
  ASSERT_TRUE(portrait) << portrait.error().what();
  expectGeometry(*portrait);

  // Quest detail JSON carries the same absolute image transformation.
  auto json = native->toJson();
  ASSERT_TRUE(json) << json.error().what();
  ASSERT_TRUE(context->set("portraitJson", *json));
  auto portraitJson = context->eval<Drawable>("return portraitJson");
  ASSERT_TRUE(portraitJson) << portraitJson.error().what();
  expectGeometry(*portraitJson);

  // Authored Lua without an explicit matrix must retain default centering.
  auto authored = context->eval<Drawable>(R"LUA(
    return {image="/objects/outpost/techlab/techlabscientistportrait.png",
      position={7,11}, mirrored=true, scale=2}
  )LUA");
  ASSERT_TRUE(authored) << authored.error().what();
  expectGeometry(*authored);
});

const GameTestRegistration questLogPresentation("LuaQuestLog", "StableSelectionReadOrderPresentationDeltasReplacementReloadAndPlayerScope", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory) << directory.error().what();
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  ASSERT_TRUE(File::writeFile(String(R"LUA(
    function mutate()
      quest.setTitle("Changed title")
      quest.setText("Changed body")
      quest.setPortrait("Objective", {{line={{1,2},{5,9}},width=2}})
      quest.setPortrait("QuestStarted", {{line={{3,4},{6,8}},width=1}})
      quest.setPortraitTitle("Objective", "Changed portrait")
      quest.addReward({name="dirtmaterial",count=1})
    end
    function unchanged()
      quest.setTitle("Changed title")
      quest.setText("Changed body")
      quest.setPortrait("Objective", {{line={{1,2},{5,9}},width=2}})
      quest.setPortrait("QuestStarted", {{line={{3,4},{6,8}},width=1}})
      quest.setPortraitTitle("Objective", "Changed portrait")
    end
    function textOnly() quest.setText("Text-only change") end
    function clearPortrait() quest.setPortrait("Objective", nil) end
    function enableAbandonError() storage.failAbandon = true end
    function questFail(abandoned)
      if abandoned and storage.failAbandon then error("questlog abandon regression") end
    end
  )LUA"), File::relativeTo(overlay, "zzlog.lua")));
  for (auto const& id : {String("zzlog-a"), String("zzlog-b"), String("zzlog-side"), String("zzlog-complete"), String("zzlog-hidden")}) {
    Json config = JsonObject{{"id", id}, {"title", "Duplicate"}, {"text", id},
        {"mainQuest", id == "zzlog-a" || id == "zzlog-b"},
        {"script", "/zzlog.lua"}, {"canBeAbandoned", id != "zzlog-a"}, {"showInLog", id != "zzlog-hidden"}};
    auto printed = config.printJson();
    ASSERT_TRUE(printed) << printed.error().what();
    ASSERT_TRUE(File::writeFile(*printed, File::relativeTo(overlay, id + ".questtemplate")));
  }
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("questlog.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot) << createdRoot.error().what();
  auto& root = **createdRoot;
  class Errors : public LogSink {
  public:
    Errors() { setLevel(LogLevel::Error); }
    std::expected<void, StarException> log(std::string_view message, LogLevel) override {
      messages.emplace_back(message);
      return {};
    }
    std::vector<std::string> messages;
  };
  auto errors = std::make_shared<Errors>();
  root.logger().addSink(errors);
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration);
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context) << context.error().what();
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot) << luaRoot.error().what();
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory) << factory.error().what();
  auto player = (*factory)->create();
  ASSERT_TRUE(player) << player.error().what();
  auto versions = root.versioningDatabase();
  ASSERT_TRUE(versions) << versions.error().what();
  auto items = root.itemDatabase();
  ASSERT_TRUE(items) << items.error().what();
  auto reward = (*items)->fromJson(JsonObject{{"name", "dirtmaterial"}, {"count", 2003}, {"parameters", JsonObject{}}});
  ASSERT_TRUE(reward && *reward);
  auto rewardStore = (*items)->diskStore(*reward);
  ASSERT_TRUE(rewardStore) << rewardStore.error().what();
  Json portrait = JsonObject{{"line", JsonArray{JsonArray{2, 3}, JsonArray{7, 11}}}, {"width", 1}};
  JsonObject storedQuests;
  int updated = 0;
  for (auto const& id : {String("zzlog-a"), String("zzlog-side"), String("zzlog-b"), String("zzlog-complete"), String("zzlog-hidden")}) {
    auto arc = QuestArcDescriptor::fromJson(root, JsonObject{{"quests", JsonArray{
        JsonObject{{"questId", id}, {"templateId", id}, {"parameters", JsonObject{}}}}}});
    ASSERT_TRUE(arc) << arc.error().what();
    auto storedArc = arc->diskStore(root);
    ASSERT_TRUE(storedArc) << storedArc.error().what();
    auto stored = (*versions)->makeCurrentVersionedJson("Quest", JsonObject{
        {"state", id == "zzlog-complete" ? Json("Complete") : Json("Active")}, {"arc", *storedArc},
        {"arcPos", 0}, {"parameters", JsonObject{}}, {"money", 0}, {"lastUpdatedOn", ++updated},
        {"rewards", JsonArray{*rewardStore, *rewardStore}},
        {"portraits", JsonObject{{"Objective", JsonArray{portrait}}, {"QuestStarted", JsonArray{portrait}}}},
        {"portraitTitles", JsonObject{{"Objective", id}}}});
    ASSERT_TRUE(stored) << stored.error().what();
    storedQuests.set(id, stored->toJson());
  }
  auto domain = (*player)->questManager();
  ASSERT_TRUE(domain->diskLoad(JsonObject{{"quests", storedQuests}, {"currentQuest", "zzlog-b"}}));
  auto playerCallbacks = LuaBindings::makePlayerCallbacks(root, **player);
  ASSERT_TRUE(playerCallbacks) << playerCallbacks.error().what();
  auto probe = (*luaRoot)->createContext(StringList{});
  ASSERT_TRUE(probe) << probe.error().what();
  ASSERT_TRUE(probe->setCallbacks("player", *playerCallbacks));
  auto entries = probe->eval<Json>("local s = player.questLogEntries(false); return {s.entries[1].id,s.entries[2].id,s.entries[3].id,#s.entries,s.current}");
  ASSERT_TRUE(entries) << entries.error().what();
  EXPECT_EQ(*entries, (Json(JsonArray{"zzlog-a", "zzlog-side", "zzlog-b", 3, "zzlog-b"})));
  auto blocked = probe->eval<Json>("return {player.abandonQuest('zzlog-a'),player.abandonQuest('missing'),player.abandonQuest('zzlog-complete')}");
  ASSERT_TRUE(blocked) << blocked.error().what();
  EXPECT_EQ(*blocked, (Json(JsonArray{false, false, false})));

  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto config = (*assets)->json("/interface/opensb/questlog/questlog.config");
  ASSERT_TRUE(config) << config.error().what();
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig);
  paneConfig->set("gui", JsonObject{});
  auto createdPane = ScriptTestPane::create(**context, *paneConfig);
  ASSERT_TRUE(createdPane) << createdPane.error().what();
  auto pane = std::move(*createdPane);
  pane->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  for (auto const& name : {String("lblQuestTitle"), String("lblQuestBody"), String("lblPortraitTitle")}) {
    auto text = textEditor(**context, "");
    ASSERT_TRUE(text);
    (*text)->setMaxWidth(-1);
    ASSERT_TRUE(pane->addChild(name, *text));
  }
  for (auto const& name : {String("btnToggleTracking"), String("btnAbandon")}) {
    auto button = ButtonWidget::create(**context);
    ASSERT_TRUE(button);
    ASSERT_TRUE(pane->addChild(name, *button));
  }
  for (auto const& name : {String("imgPortrait"), String("imgPolaroid"), String("imgPolaroidBack"), String("lblRewards")}) {
    auto image = ImageWidget::create(**context, "");
    ASSERT_TRUE(image);
    ASSERT_TRUE(pane->addChild(name, *image));
  }
  // Native ItemGrid construction requires renderInit even with empty slot art.
  // Real grid stacking, capacity and tooltips are exercised by the GL smoke.
  auto group = std::make_shared<ButtonGroupWidget>(**context);
  for (auto const& entry : {std::pair{0, String("inProgress")}, std::pair{1, String("completed")}}) {
    auto button = ButtonWidget::create(**context);
    ASSERT_TRUE(button);
    (*button)->setData(entry.second);
    (*button)->setButtonGroup(group, entry.first);
    ASSERT_TRUE(group->addChild(entry.second, *button));
  }
  ASSERT_TRUE(group->select(0));
  ASSERT_TRUE(pane->addChild("filter", group));
  auto scroll = std::make_shared<Widget>(**context);
  auto layout = std::make_shared<VerticalLayout>(**context);
  layout->setFillDown(true);
  auto schema = Json::parseJson(R"JSON({
    "spacing":[0,1],"memberSize":[150,19],
    "listTemplate":{
      "lblQuestEntry":{"type":"textbox","maxWidth":-1},
      "imgPortrait":{"type":"image","file":""},
      "imgNew":{"type":"image","file":""},
      "imgTracked":{"type":"image","file":""},
      "imgCurrent":{"type":"image","file":""}
    }
  })JSON");
  ASSERT_TRUE(schema);
  for (auto const& name : {String("mainQuestList"), String("sideQuestList")}) {
    auto list = ListWidget::create(**context, *schema);
    ASSERT_TRUE(list);
    (*list)->setPosition({0, name == "mainQuestList" ? 3 : 1});
    (*list)->registerMemberCallback("lblQuestEntry", [](Widget&) -> std::expected<void, StarException> { return {}; });
    (*list)->setCallback([&, name](Widget&) -> std::expected<void, StarException> {
      auto called = pane->call(name, {});
      if (!called) return std::unexpected(std::move(called.error()));
      return {};
    });
    ASSERT_TRUE(layout->addChild(name, *list));
  }
  auto mainHeader = std::make_shared<Widget>(**context);
  auto sideHeader = std::make_shared<Widget>(**context);
  mainHeader->setPosition({0, 4});
  sideHeader->setPosition({0, 2});
  mainHeader->setSize({100, 15});
  sideHeader->setSize({100, 15});
  ASSERT_TRUE(layout->addChild("mainQuestHeader", mainHeader));
  ASSERT_TRUE(layout->addChild("sideQuestHeader", sideHeader));
  ASSERT_TRUE(scroll->addChild("verticalLayout", layout));
  ASSERT_TRUE(pane->addChild("scrollArea", scroll));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Window, pane, {}));
  auto main = pane->fetchChild<ListWidget>("scrollArea.verticalLayout.mainQuestList");
  auto side = pane->fetchChild<ListWidget>("scrollArea.verticalLayout.sideQuestList");
  auto body = pane->fetchChild<TextBoxWidget>("lblQuestBody");
  auto portraitTitle = pane->fetchChild<TextBoxWidget>("lblPortraitTitle");
  ASSERT_TRUE(main && *main && side && *side && body && *body && portraitTitle && *portraitTitle);
  ASSERT_EQ((*main)->listSize(), 2u);
  ASSERT_EQ((*side)->listSize(), 1u);
  EXPECT_EQ((*main)->itemAt(0)->data(), Json("zzlog-a"));
  EXPECT_EQ((*main)->itemAt(1)->data(), Json("zzlog-b"));
  ASSERT_TRUE((*main)->selectedWidget());
  EXPECT_EQ((*main)->selectedWidget()->data(), Json("zzlog-b"));
  EXPECT_EQ((*body)->getText(), String("zzlog-b"));
  auto selectedQuest = domain->getQuest("zzlog-b");
  auto lockedQuest = domain->getQuest("zzlog-a");
  ASSERT_TRUE(selectedQuest && lockedQuest);
  EXPECT_FALSE((*selectedQuest)->unread());
  EXPECT_TRUE((*lockedQuest)->unread());
  ASSERT_TRUE(pane->tick(1.0f / 60));
  auto compact = probe->eval<Json>("local q = player.questLogDetails('zzlog-b'); local s = player.questLogDetails('zzlog-b',q.revision,q.portraitRevision,q.rewardsRevision); return {s.title == nil,s.rewards == nil,s.portrait == nil,s.active,s.tracked}");
  ASSERT_TRUE(compact) << compact.error().what();
  EXPECT_EQ(*compact, (Json(JsonArray{true, true, true, true, true})));
  ASSERT_TRUE((*main)->clearSelected());
  EXPECT_EQ((*body)->getText(), String(""));
  ASSERT_TRUE((*side)->setSelected(0));
  EXPECT_FALSE((*main)->selectedWidget());
  EXPECT_EQ((*body)->getText(), String("zzlog-side"));
  ASSERT_TRUE(pane->call("btnToggleTracking", {}));
  EXPECT_EQ(domain->currentQuestId(), std::optional<String>("zzlog-side"));
  ASSERT_TRUE(pane->call("btnToggleTracking", {}));
  EXPECT_FALSE(domain->currentQuestId());
  ASSERT_TRUE((*main)->setSelected(0));
  EXPECT_FALSE((*side)->selectedWidget());
  EXPECT_FALSE((*lockedQuest)->unread());
  ASSERT_TRUE(pane->call("btnAbandon", {}));
  EXPECT_TRUE(domain->isActive("zzlog-a"));
  {
    auto storage = File::ephemeralFile(root.random());
    ASSERT_TRUE(storage) << storage.error().what();
    auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
    ASSERT_TRUE(world) << world.error().what();
    auto closeWorld = finally([&] { EXPECT_TRUE((*world)->close()); });
    ASSERT_TRUE((*lockedQuest)->init(**player, **world, {}));
    auto uninitQuest = finally([&] { EXPECT_TRUE((*lockedQuest)->uninit()); });
    auto failingQuest = domain->getQuest("zzlog-side");
    ASSERT_TRUE(failingQuest);
    ASSERT_TRUE((*failingQuest)->init(**player, **world, {}));
    auto uninitFailingQuest = finally([&] { EXPECT_TRUE((*failingQuest)->uninit()); });
    ASSERT_TRUE((*failingQuest)->callScript("enableAbandonError", {}));
    auto rejected = probe->eval<Json>("local ok,err=pcall(player.abandonQuest,'zzlog-side'); return {ok,tostring(err):find('questlog abandon regression',1,true)~=nil}");
    ASSERT_TRUE(rejected) << rejected.error().what();
    EXPECT_EQ(*rejected, (Json(JsonArray{false, true})));
    EXPECT_EQ((*failingQuest)->state(), QuestState::Failed);
    ASSERT_EQ(errors->messages.size(), 1u);
    EXPECT_NE(errors->messages.front().find("questlog abandon regression"), std::string::npos);
    auto before = (*lockedQuest)->presentationRevision();
    ASSERT_TRUE((*lockedQuest)->callScript("mutate", {}));
    EXPECT_NE((*lockedQuest)->presentationRevision(), before);
    ASSERT_TRUE(pane->tick(1.0f / 60));
    EXPECT_EQ((*body)->getText(), String("Changed body"));
    EXPECT_EQ((*portraitTitle)->getText(), String("Changed portrait"));
    auto after = (*lockedQuest)->presentationRevision();
    ASSERT_TRUE((*lockedQuest)->callScript("unchanged", {}));
    EXPECT_EQ((*lockedQuest)->presentationRevision(), after);
    ASSERT_TRUE(pane->tick(1.0f / 60));
    ASSERT_TRUE(probe->eval<LuaValue>("prior = player.questLogDetails('zzlog-a')"));
    ASSERT_TRUE((*lockedQuest)->callScript("textOnly", {}));
    ASSERT_TRUE(pane->tick(1.0f / 60));
    EXPECT_EQ((*body)->getText(), String("Text-only change"));
    auto delta = probe->eval<Json>("local q=player.questLogDetails('zzlog-a',prior.revision,prior.portraitRevision,prior.rewardsRevision); return {q.text,q.rewards==nil,q.portrait==nil}");
    ASSERT_TRUE(delta) << delta.error().what();
    EXPECT_EQ(*delta, (Json(JsonArray{"Text-only change", true, true})));
    ASSERT_TRUE(pane->call("filter", {}));
    auto row = (*main)->selectedWidget()->fetchChild<TextBoxWidget>("lblQuestEntry");
    ASSERT_TRUE(row && *row);
    EXPECT_EQ((*row)->getText(), String("Changed title"));
    auto parsed = probe->eval<Json>("local q=player.questLogDetails('zzlog-a'); return {q.portrait[1].line,q.portraitTitle}");
    ASSERT_TRUE(parsed) << parsed.error().what();
    EXPECT_EQ(*parsed, (Json(JsonArray{JsonArray{JsonArray{1, 2}, JsonArray{5, 9}}, "Changed portrait"})));
    ASSERT_TRUE(probe->eval<LuaValue>("prior = player.questLogDetails('zzlog-a')"));
    ASSERT_TRUE((*lockedQuest)->callScript("clearPortrait", {}));
    ASSERT_TRUE(pane->tick(1.0f / 60));
    auto polaroid = pane->fetchChild("imgPolaroid");
    ASSERT_TRUE(polaroid && *polaroid);
    EXPECT_FALSE((*polaroid)->visibility());
    EXPECT_EQ((*portraitTitle)->getText(), String(""));
    auto portraitDelta = probe->eval<Json>(
        "local q=player.questLogDetails('zzlog-a',prior.revision,prior.portraitRevision,prior.rewardsRevision); "
        "return {q.title==nil,q.text==nil,q.portraitTitle,q.portrait==nil,q.rewards==nil}");
    ASSERT_TRUE(portraitDelta) << portraitDelta.error().what();
    EXPECT_EQ(*portraitDelta, (Json(JsonArray{true, true, "", true, true})));
  }

  // Stable identity, not title, survives reordered native rows. Replacing a
  // Quest allocation with the same ID must also invalidate revision caches.
  auto storedA = VersionedJson::fromJson(storedQuests["zzlog-a"]);
  ASSERT_TRUE(storedA) << storedA.error().what();
  auto aData = storedA->content.toObject();
  ASSERT_TRUE(aData);
  aData->set("text", "Replacement body");
  aData->set("portraitTitles", JsonObject{{"Objective", "Replacement portrait"}});
  aData->set("lastUpdatedOn", 100);
  aData->set("rewards", JsonArray{});
  auto replacementA = (*versions)->makeCurrentVersionedJson("Quest", *aData);
  ASSERT_TRUE(replacementA);
  storedQuests.set("zzlog-a", replacementA->toJson());
  domain->setAsTracked("zzlog-b");
  auto oldRevision = (*lockedQuest)->presentationRevision();
  ASSERT_TRUE(domain->diskLoad(JsonObject{{"quests", storedQuests}, {"currentQuest", "zzlog-b"}}));
  auto newA = domain->getQuest("zzlog-a");
  ASSERT_TRUE(newA);
  EXPECT_NE((*newA)->presentationRevision(), oldRevision);
  ASSERT_TRUE(pane->call("filter", {}));
  ASSERT_TRUE((*main)->selectedWidget());
  EXPECT_EQ((*main)->selectedWidget()->data(), Json("zzlog-a"));
  EXPECT_EQ((*main)->itemAt(0)->data(), Json("zzlog-b"));
  EXPECT_EQ((*body)->getText(), String("Replacement body"));
  EXPECT_EQ((*portraitTitle)->getText(), String("Replacement portrait"));
  auto clearedRewards = probe->eval<bool>("local q=player.questLogDetails('zzlog-a'); return q.rewards~=nil and #q.rewards==0");
  ASSERT_TRUE(clearedRewards) << clearedRewards.error().what();
  EXPECT_TRUE(*clearedRewards);
  ASSERT_TRUE(pane->dismissed());
  ASSERT_TRUE(pane->displayed());
  EXPECT_EQ((*body)->getText(), String("Replacement body"));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->tick(1.0f / 60));
  EXPECT_EQ((*body)->getText(), String("Replacement body"));
  ASSERT_TRUE(group->select(1));
  ASSERT_TRUE(pane->call("filter", {}));
  EXPECT_EQ((*body)->getText(), String(""));
  ASSERT_EQ((*side)->listSize(), 1u);
  EXPECT_EQ((*side)->itemAt(0)->data(), Json("zzlog-complete"));
  ASSERT_TRUE((*side)->setSelected(0));
  EXPECT_EQ((*body)->getText(), String("zzlog-complete"));
  auto inactive = probe->eval<Json>("local q = player.questLogDetails('zzlog-complete'); return {q.active,q.canAbandon,player.questLogDetails('missing') == nil}");
  ASSERT_TRUE(inactive);
  EXPECT_EQ(*inactive, (Json(JsonArray{false, false, true})));
  ASSERT_TRUE(group->select(0));
  ASSERT_TRUE(pane->call("filter", {}));
  EXPECT_EQ((*main)->selectedWidget()->data(), Json("zzlog-b"));
  ASSERT_TRUE((*side)->setSelected(0));
  ASSERT_TRUE(pane->call("btnAbandon", {}));
  EXPECT_FALSE(domain->isActive("zzlog-side"));
  EXPECT_EQ((*main)->selectedWidget()->data(), Json("zzlog-b"));

  // Production registration and manager retention/scope policy, using the
  // actual consumer pane and native pane stack rather than an echoed service.
  auto controller = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(controller);
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(interface.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (!pane) return std::optional<LuaCallbacks>{};
    auto bound = pane->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(interface.registerCallback("createPane", [](String const&, Json const&, String const&) -> std::expected<void, StarException> {
    return std::unexpected(StarException("Unexpected recreation of retained QuestLog"));
  }));
  ASSERT_TRUE(interface.registerCallback("destroyPane", [&](String const&) -> std::expected<void, StarException> {
    if (pane && panes.isDisplayed(pane)) {
      auto dismissed = panes.dismissPane(pane);
      if (!dismissed) return std::unexpected(std::move(dismissed.error()));
    }
    pane.reset();
    return {};
  }));
  ASSERT_TRUE(interface.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (panes.isDisplayed(pane)) return pane->show();
    return panes.displayPane(PaneLayer::Window, pane, {});
  }));
  ASSERT_TRUE(controller->setCallbacks("interface", interface));
  auto rootCallbacks = LuaBindings::makeRootCallbacks(root);
  ASSERT_TRUE(rootCallbacks);
  ASSERT_TRUE(controller->setCallbacks("root", *rootCallbacks));
  ASSERT_TRUE(controller->eval<LuaValue>(R"LUA(
    definitions = {QuestLog = root.assetJson('/interface.config:panes.QuestLog')}
    session = {hasPlayer=true,inWorld=true,playerGeneration=1,worldGeneration=1}
    checkpoint = {session=session}
    manager = UiManager.new(checkpoint, definitions, session)
    manager:open('QuestLog')
  )LUA"));
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:update()"));
  ASSERT_TRUE(pane);
  EXPECT_FALSE(panes.isDisplayed(pane));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:open('QuestLog')"));
  EXPECT_TRUE(panes.isDisplayed(pane));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:suspend({hasPlayer=true,inWorld=true,playerGeneration=1,worldGeneration=2})"));
  ASSERT_TRUE(pane);
  ASSERT_TRUE(controller->eval<LuaValue>("manager:suspend({hasPlayer=true,inWorld=true,playerGeneration=2,worldGeneration=2})"));
  EXPECT_FALSE(pane);
  EXPECT_EQ(errors->messages.size(), 1u);
});

const GameTestRegistration questDialogs("QuestDialogs", "LuaModalDecisionsReloadIdentityTemplatesAndCinema", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  ASSERT_TRUE(File::makeDirectory(File::relativeTo(overlay, "quests")));
  auto writeJson = [&](String const& path, Json const& json) -> std::expected<void, StarException> {
    auto text = json.printJson();
    if (!text) return std::unexpected(std::move(text.error()));
    return File::writeFile(*text, File::relativeTo(overlay, path));
  };
  // Real widgets and production Lua, with only renderer-dependent ItemGrid
  // omitted. Its native bag/capacity/tooltips are exercised in the GL client.
  JsonObject layout{{"panefeature", JsonObject{{"type", "panefeature"}, {"anchor", "bottomLeft"}}}};
  for (auto const& name : {String("marker"), String("lblQuestTitle"), String("lblQuestBody"),
      String("portraitTitle"), String("objectivePortraitTitle"), String("lblMoneyAmount")})
    layout.set(name, JsonObject{{"type", "textbox"}, {"callback", "null"}, {"maxWidth", -1}});
  for (auto const& name : {String("portraitImage"), String("objectivePortraitImage"),
      String("imgPolaroid"), String("imgPolaroidBack")})
    layout.set(name, JsonObject{{"type", "image"}, {"file", ""}});
  for (auto const& name : {String("short"), String("portrait"), String("complete"), String("failed"), String("custom")}) {
    auto gui = layout;
    gui.set("marker", JsonObject{{"type", "textbox"}, {"callback", "null"}, {"maxWidth", -1}, {"value", name}});
    if (name == "short") {
      gui.set("background", JsonObject{{"type", "background"}});
      gui.set("unusedMoney", JsonObject{{"type", "image"}, {"file", ""}, {"position", JsonArray{60, 700}}});
    }
    ASSERT_TRUE(writeJson("zzdialog-" + name + ".config", JsonObject{{"onShowSound", JsonArray{}}, {"paneLayout", gui}}));
  }
  ASSERT_TRUE(writeJson("quests/quests.config.patch", JsonArray{
      JsonObject{{"op", "replace"}, {"path", "/defaultGuiConfigs/newQuest"}, {"value", "/zzdialog-short.config"}},
      JsonObject{{"op", "replace"}, {"path", "/defaultGuiConfigs/newQuestPortrait"}, {"value", "/zzdialog-portrait.config"}},
      JsonObject{{"op", "replace"}, {"path", "/defaultGuiConfigs/questComplete"}, {"value", "/zzdialog-complete.config"}},
      JsonObject{{"op", "replace"}, {"path", "/defaultGuiConfigs/questFailed"}, {"value", "/zzdialog-failed.config"}}}));
  ASSERT_TRUE(File::writeFile(String(R"LUA(
    function questStart() player.setProperty("zzdialogDecision", "accept") end
    function questDecline() player.setProperty("zzdialogDecision", "decline") end
    function questCancel()
      player.setProperty("zzdialogDecision", "cancel")
      if player.getProperty("zzdialogThrow", false) then error("quest decision regression") end
    end
  )LUA"), File::relativeTo(overlay, "zzdialog.lua")));
  for (auto const& id : {String("zzdialog-failed"), String("zzdialog-complete"), String("zzdialog-offer"),
      String("zzdialog-locked"), String("zzdialog-custom"), String("zzdialog-missing"), String("zzdialog-malformed")}) {
    JsonObject config{{"id", id}, {"title", id}, {"text", "Offer body"}, {"completionText", "Completion body"},
        {"failureText", "Failure body"}, {"script", "/zzdialog.lua"}, {"canBeAbandoned", id != "zzdialog-locked"}};
    if (id == "zzdialog-complete") {
      config.set("completionCinema", "/zzdialog-<species>-<gender>.cinematic");
      config.set("ephemeral", true);
    }
    if (id == "zzdialog-custom")
      config.set("guiConfigs", JsonObject{{"newQuest", "/zzdialog-custom.config"},
          {"questComplete", "/zzdialog-custom.config"}, {"questFailed", "/zzdialog-custom.config"}});
    if (id == "zzdialog-missing") config.set("completionCinema", "/zzdialog-nonexistent.cinematic");
    if (id == "zzdialog-malformed") config.set("completionCinema", "/zzdialog-malformed.cinematic");
    ASSERT_TRUE(writeJson(id + ".questtemplate", config));
  }
  ASSERT_TRUE(writeJson("zzdialog-malformed.cinematic", JsonObject{{"panels", 7}}));
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("questdialogs.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot) << createdRoot.error().what();
  auto& root = **createdRoot;
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration);
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{{"GuiClose", JsonArray{
      JsonObject{{"type", "key"}, {"value", "Esc"}, {"mods", JsonArray{}}}}}}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context);
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto player = (*factory)->create();
  ASSERT_TRUE(player);
  auto versions = root.versioningDatabase();
  ASSERT_TRUE(versions);
  auto storedQuest = [&](String const& id, String const& templateId, String const& state, bool portrait) -> std::expected<Json, StarException> {
    auto arc = QuestArcDescriptor::fromJson(root, JsonObject{{"quests", JsonArray{
        JsonObject{{"questId", id}, {"templateId", templateId}, {"parameters", JsonObject{}}}}}});
    if (!arc) return std::unexpected(std::move(arc.error()));
    auto storedArc = arc->diskStore(root);
    if (!storedArc) return std::unexpected(std::move(storedArc.error()));
    JsonObject value{{"state", state}, {"arc", *storedArc}, {"arcPos", 0},
        {"parameters", JsonObject{}}, {"money", 12345}, {"rewards", JsonArray{}},
        {"lastUpdatedOn", 0}, {"showDialog", true}};
    if (portrait) value.set("portraits", JsonObject{{"Objective", JsonArray{
        JsonObject{{"line", JsonArray{JsonArray{2, 3}, JsonArray{7, 11}}}, {"width", 1}}}}});
    auto stored = (*versions)->makeCurrentVersionedJson("Quest", value);
    if (!stored) return std::unexpected(std::move(stored.error()));
    return stored->toJson();
  };
  JsonObject initial;
  for (auto const& entry : {std::pair{String("zzdialog-failed"), String("Failed")},
      std::pair{String("zzdialog-complete"), String("Complete")}, std::pair{String("zzdialog-offer"), String("Offer")}}) {
    auto stored = storedQuest(entry.first, entry.first, entry.second, entry.second == "Offer");
    ASSERT_TRUE(stored);
    initial.set(entry.first, *stored);
  }
  auto domain = (*player)->questManager();
  ASSERT_TRUE(domain->diskLoad(JsonObject{{"quests", initial}}));
  auto storage = File::ephemeralFile(root.random());
  ASSERT_TRUE(storage);
  auto world = WorldServer::create(root, Vec2U(2048, 2048), *storage);
  ASSERT_TRUE(world) << world.error().what();
  auto closeWorld = finally([&] { EXPECT_TRUE((*world)->close()); });
  ASSERT_TRUE(domain->init(**player, **world));
  auto uninitDomain = finally([&] { EXPECT_TRUE(domain->uninit()); });
  auto failed = domain->getQuest("zzdialog-failed");
  auto complete = domain->getQuest("zzdialog-complete");
  auto offered = domain->getQuest("zzdialog-offer");
  ASSERT_TRUE(failed && complete && offered);
  auto cinemaFile = "zzdialog-" + (*player)->species() + "-" + ((*player)->gender() == Gender::Male ? String("male") : String("female")) + ".cinematic";
  ASSERT_TRUE(writeJson(cinemaFile, JsonObject{
      {"panels", JsonArray{JsonObject{{"keyframes", JsonArray{
          JsonObject{{"timecode", 0}}, JsonObject{{"timecode", 1}}}}}}},
      {"audio", JsonArray{}}, {"suppressInput", true}}));
  // Discover the player's resolved species/gender asset before exercising
  // native fetchJson and real Cinematic::load.
  ASSERT_TRUE(root.reload());
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot);
  auto playerCallbacks = LuaBindings::makePlayerCallbacks(root, **player);
  ASSERT_TRUE(playerCallbacks);
  auto rootCallbacks = LuaBindings::makeRootCallbacks(root);
  ASSERT_TRUE(rootCallbacks);
  auto cinematic = Cinematic::create(**context);
  ASSERT_TRUE(cinematic);
  int cinemaLoads = 0;
  PaneManager panes(**context);
  std::shared_ptr<ScriptTestPane> pane;
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("paneIdAvailable", &MainInterface::paneIdAvailable));
  ASSERT_TRUE(interface.registerCallback("paneLayerOccupied", [&](String const&) {
    return static_cast<bool>(panes.topPane({PaneLayer::ModalWindow}));
  }));
  ASSERT_TRUE(interface.registerCallback("bindRegisteredPane", [&](String const&) -> std::expected<std::optional<LuaCallbacks>, StarException> {
    if (!pane) return std::optional<LuaCallbacks>{};
    auto bound = pane->makePaneCallbacks();
    if (!bound) return std::unexpected(std::move(bound.error()));
    return std::optional<LuaCallbacks>(std::move(*bound));
  }));
  ASSERT_TRUE(interface.registerCallback("playCinematic", [&](Json const& json) -> std::expected<void, StarException> {
    auto loaded = (*cinematic)->load(json);
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    ++cinemaLoads;
    return {};
  }));
  ASSERT_TRUE(interface.registerCallback("createPane", [&](String const&, Json config, String const&) -> std::expected<void, StarException> {
    auto created = ScriptTestPane::create(**context, std::move(config));
    if (!created) return std::unexpected(std::move(created.error()));
    pane = std::move(*created);
    auto button = ButtonWidget::create(**context);
    if (!button) return std::unexpected(std::move(button.error()));
    if (auto added = pane->addChild("btnDecline", *button); !added) return std::unexpected(std::move(added.error()));
    if (auto bound = pane->setLuaRoot(*luaRoot); !bound) return std::unexpected(std::move(bound.error()));
    if (auto bound = pane->addCallbacks("player", *playerCallbacks); !bound) return std::unexpected(std::move(bound.error()));
    return pane->addCallbacks("interface", interface);
  }));
  ASSERT_TRUE(interface.registerCallback("destroyPane", [&](String const&) -> std::expected<void, StarException> {
    if (pane && panes.isDisplayed(pane)) {
      auto dismissed = panes.dismissPane(pane);
      if (!dismissed) return std::unexpected(std::move(dismissed.error()));
    }
    pane.reset();
    return {};
  }));
  ASSERT_TRUE(interface.registerCallback("displayRegisteredPane", [&](String const&) -> std::expected<void, StarException> {
    if (panes.isDisplayed(pane)) {
      if (auto shown = pane->show(); !shown)
        return std::unexpected(std::move(shown.error()));
      return pane->resumeScripts();
    }
    return panes.displayPane(PaneLayer::ModalWindow, pane, {});
  }));
  auto controller = (*luaRoot)->createContext("/interface/opensb/ui/manager.lua");
  ASSERT_TRUE(controller);
  ASSERT_TRUE(controller->setCallbacks("root", *rootCallbacks));
  ASSERT_TRUE(controller->setCallbacks("player", *playerCallbacks));
  ASSERT_TRUE(controller->setCallbacks("interface", interface));
  ASSERT_TRUE(controller->eval<LuaValue>(R"LUA(
    require "/interface/opensb/questdialogs/pending.lua"
    session = {hasPlayer=true,inWorld=true,playerGeneration=1,worldGeneration=1}
    checkpoint = {session=session}
    manager = UiManager.new(checkpoint, {}, session)
  )LUA"));
  auto update = [&] { return controller->eval<LuaValue>("manager:update(); QuestDialogs.update(manager)"); };
  auto text = [&](String const& name) {
    auto child = pane->fetchChild<TextBoxWidget>(name);
    EXPECT_TRUE(child && *child);
    return child && *child ? (*child)->getText() : String{};
  };
  auto token = [&]() { return pane->config().query("questDialog.token").and_then([](Json const& value) { return value.toUInt(); }); };
  auto blocker = Pane::create(**context);
  ASSERT_TRUE(blocker);
  (*blocker)->setAnchor(PaneAnchor::BottomLeft);
  ASSERT_TRUE(panes.displayPane(PaneLayer::ModalWindow, *blocker, {}));
  ASSERT_TRUE(update());
  EXPECT_FALSE(pane);
  EXPECT_TRUE((*failed)->showDialog());
  EXPECT_TRUE((*complete)->showDialog());
  ASSERT_TRUE(panes.dismissPane(*blocker));
  ASSERT_TRUE(update());
  ASSERT_TRUE(pane);
  EXPECT_EQ(text("marker"), String("failed"));
  EXPECT_EQ(text("lblQuestBody"), String("Failure body"));
  EXPECT_FALSE((*failed)->showDialog());
  EXPECT_TRUE((*complete)->showDialog());
  auto failurePane = pane;
  ASSERT_TRUE(update());
  EXPECT_EQ(pane, failurePane);
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(update());
  EXPECT_EQ(text("marker"), String("complete"));
  EXPECT_EQ(text("lblMoneyAmount"), String("12345"));
  EXPECT_FALSE((*complete)->showDialog());
  // Acknowledgment can remove an ephemeral quest without invalidating its
  // captured completion transaction.
  auto completionToken = token();
  ASSERT_TRUE(completionToken);
  ASSERT_TRUE(domain->update(0));
  EXPECT_FALSE(domain->hasQuest("zzdialog-complete"));
  EXPECT_TRUE(domain->dialogValid(*completionToken));
  auto resolved = domain->dialogCinema(*completionToken);
  ASSERT_TRUE(resolved) << resolved.error().what();
  EXPECT_EQ(resolved->getBool("suppressInput"), true);
  ASSERT_TRUE(pane->call("close"));
  EXPECT_TRUE((*cinematic)->suppressInput());
  EXPECT_EQ(cinemaLoads, 1);
  ASSERT_TRUE(pane->call("btnAccept"));
  EXPECT_EQ(cinemaLoads, 1);
  (*cinematic)->setTime(2);
  EXPECT_FALSE((*cinematic)->suppressInput());
  ASSERT_TRUE(panes.dismissPane(pane));
  ASSERT_TRUE(update());
  EXPECT_EQ(text("marker"), String("portrait"));
  auto offerToken = token();
  ASSERT_TRUE(offerToken);
  auto repeatView = domain->pendingDialog("offer");
  ASSERT_TRUE(repeatView);
  EXPECT_EQ(repeatView->getUInt("token"), *offerToken);
  auto objective = pane->fetchChild<ImageWidget>("objectivePortraitImage");
  ASSERT_TRUE(objective && *objective);
  EXPECT_NE((*objective)->size(), Vec2I{});
  // This is the generic centralized capability-retirement primitive, not a
  // dialog-specific reload path. Native display membership survives it.
  ASSERT_TRUE(pane->stopScripts());
  EXPECT_TRUE(panes.isDisplayed(pane));
  EXPECT_EQ((*offered)->state(), QuestState::Offer);
  ASSERT_TRUE(pane->removeCallbacks("player"));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:open('QuestDialog')"));
  ASSERT_TRUE(pane->tick(0));
  ASSERT_TRUE(root.reload());
  ASSERT_TRUE(pane->tick(0));
  EXPECT_TRUE(domain->dialogValid(*offerToken));
  EXPECT_EQ((*offered)->state(), QuestState::Offer);
  auto shown = pane->evaluate("return storage.shown and not storage.finished");
  ASSERT_TRUE(shown && *shown);
  EXPECT_EQ(**shown, Json(true));
  ASSERT_TRUE(pane->call("btnAccept"));
  EXPECT_EQ((*offered)->state(), QuestState::Offer);
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ((*offered)->state(), QuestState::Active);
  auto duplicate = domain->finishDialog(*offerToken, "cancel");
  ASSERT_TRUE(duplicate);
  EXPECT_FALSE(*duplicate);
  auto decision = controller->eval<Json>("return player.getProperty('zzdialogDecision')");
  ASSERT_TRUE(decision);
  EXPECT_EQ(*decision, Json("accept"));

  auto replace = [&](String const& templateId, String const& state, bool portrait = false) -> std::expected<void, StarException> {
    auto stored = storedQuest("zzdialog-reused", templateId, state, portrait);
    if (!stored) return std::unexpected(std::move(stored.error()));
    auto loaded = domain->diskLoad(JsonObject{{"quests", JsonObject{{"zzdialog-reused", *stored}}}});
    if (!loaded) return std::unexpected(std::move(loaded.error()));
    return domain->init(**player, **world);
  };
  ASSERT_TRUE(replace("zzdialog-offer", "Offer"));
  ASSERT_TRUE(update());
  EXPECT_EQ(text("marker"), String("short"));
  // Native QuestPane centers the background bounds, ignoring parked controls.
  // Empty background art keeps this regression genuinely renderer-independent.
  EXPECT_EQ(pane->size(), Vec2I{});
  auto unusedMoney = pane->fetchChild<ImageWidget>("unusedMoney");
  ASSERT_TRUE(unusedMoney && *unusedMoney);
  EXPECT_EQ((*unusedMoney)->relativePosition(), (Vec2I{60, 700}));
  auto stale = token();
  ASSERT_TRUE(stale);
  auto oldQuest = domain->getQuest("zzdialog-reused");
  ASSERT_TRUE(oldQuest);
  auto lockedStore = storedQuest("zzdialog-reused", "zzdialog-locked", "Offer", false);
  ASSERT_TRUE(lockedStore);
  auto locked = Quest::create(root, *lockedStore);
  ASSERT_TRUE(locked);
  ASSERT_TRUE(domain->offer(*locked));
  ASSERT_TRUE(panes.dismissPane(pane));
  auto replacement = domain->getQuest("zzdialog-reused");
  ASSERT_TRUE(replacement);
  EXPECT_EQ((*replacement)->state(), QuestState::Offer);
  EXPECT_EQ((*oldQuest)->state(), QuestState::Offer);
  EXPECT_FALSE(domain->dialogValid(*stale));
  ASSERT_TRUE(update());
  auto declineButton = pane->fetchChild<ButtonWidget>("btnDecline");
  ASSERT_TRUE(declineButton && *declineButton);
  EXPECT_EQ(pane->config().query("questDialog.canDecline"), Json(false));
  ASSERT_TRUE(pane->call("btnDecline"));
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ((*replacement)->state(), QuestState::New);
  decision = controller->eval<Json>("return player.getProperty('zzdialogDecision')");
  ASSERT_TRUE(decision);
  EXPECT_EQ(*decision, Json("cancel"));
  ASSERT_TRUE(replace("zzdialog-custom", "Offer"));
  ASSERT_TRUE(update());
  EXPECT_EQ(text("marker"), String("custom"));
  ASSERT_TRUE(pane->call("btnDecline"));
  ASSERT_TRUE(panes.dismissPane(pane));
  decision = controller->eval<Json>("return player.getProperty('zzdialogDecision')");
  ASSERT_TRUE(decision);
  EXPECT_EQ(*decision, Json("decline"));
  // The same allocation returning to Offer is a new decision, not a revival
  // of an old response merely because its ID and current state match.
  ASSERT_TRUE(replace("zzdialog-offer", "Offer"));
  ASSERT_TRUE(update());
  auto priorOffer = token();
  ASSERT_TRUE(priorOffer);
  auto reoffered = domain->getQuest("zzdialog-reused");
  ASSERT_TRUE(reoffered);
  ASSERT_TRUE((*reoffered)->cancelOffer());
  ASSERT_TRUE((*reoffered)->offer());
  EXPECT_EQ((*reoffered)->state(), QuestState::Offer);
  EXPECT_FALSE(domain->dialogValid(*priorOffer));
  ASSERT_TRUE(pane->call("btnAccept"));
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ((*reoffered)->state(), QuestState::Offer);
  ASSERT_TRUE(update());
  auto renewedOffer = token();
  ASSERT_TRUE(renewedOffer);
  EXPECT_NE(*renewedOffer, *priorOffer);
  ASSERT_TRUE(pane->call("btnAccept"));
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ((*reoffered)->state(), QuestState::Active);
  for (auto const& state : {String("Complete"), String("Failed")}) {
    ASSERT_TRUE(replace("zzdialog-custom", state));
    ASSERT_TRUE(update());
    EXPECT_EQ(text("marker"), String("custom"));
    EXPECT_EQ(text("lblQuestBody"), state == "Complete" ? String("Completion body") : String("Failure body"));
    ASSERT_TRUE(panes.dismissPane(pane));
  }
  ASSERT_TRUE(replace("zzdialog-offer", "Offer"));
  ASSERT_TRUE(update());
  auto expired = domain->getQuest("zzdialog-reused");
  ASSERT_TRUE(expired);
  ASSERT_TRUE(controller->eval<LuaValue>("manager:suspend({hasPlayer=true,inWorld=true,playerGeneration=1,worldGeneration=2})"));
  EXPECT_FALSE(pane);
  EXPECT_EQ((*expired)->state(), QuestState::New);
  ASSERT_TRUE(replace("zzdialog-complete", "Complete"));
  ASSERT_TRUE(update());
  auto escapedToken = token();
  ASSERT_TRUE(escapedToken);
  auto escaped = pane->sendEvent(KeyDownEvent{Key::Escape, KeyMod::NoMod});
  ASSERT_TRUE(escaped && *escaped);
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_FALSE(domain->dialogValid(*escapedToken));
  EXPECT_EQ(cinemaLoads, 1);
  // Admin previews use the same real Lua layouts but cannot mutate a quest,
  // acknowledge a player's quest, or run its completion cinema.
  for (auto const& kind : {String("offer"), String("complete"), String("failed")}) {
    auto previewStore = storedQuest("zzdialog-preview", "zzdialog-custom", "Offer", false);
    ASSERT_TRUE(previewStore);
    auto preview = Quest::create(root, *previewStore);
    ASSERT_TRUE(preview);
    ASSERT_TRUE(domain->previewDialog(*preview, kind));
    ASSERT_TRUE(update());
    EXPECT_EQ(text("marker"), String("custom"));
    ASSERT_TRUE(pane->call("btnAccept"));
    ASSERT_TRUE(panes.dismissPane(pane));
    EXPECT_EQ((*preview)->state(), QuestState::Offer);
    EXPECT_EQ(cinemaLoads, 1);
  }
  EXPECT_FALSE(domain->pendingDialog("invalid"));
  EXPECT_FALSE(domain->finishDialog(0, "invalid"));

  // Generic dismissal never executes cinema; explicit close propagates both
  // asset-fetch and native Cinematic::load failures without dismissing.
  ASSERT_TRUE(replace("zzdialog-missing", "Complete"));
  ASSERT_TRUE(update());
  auto missingToken = token();
  ASSERT_TRUE(missingToken);
  EXPECT_FALSE(domain->dialogCinema(*missingToken));
  auto closeError = pane->evaluate("local ok = pcall(close); return ok");
  ASSERT_TRUE(closeError && *closeError);
  EXPECT_EQ(**closeError, Json(false));
  EXPECT_TRUE(pane->isDisplayed());
  EXPECT_TRUE(domain->dialogValid(*missingToken));
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ(cinemaLoads, 1);
  ASSERT_TRUE(replace("zzdialog-malformed", "Complete"));
  ASSERT_TRUE(update());
  closeError = pane->evaluate("local ok = pcall(btnAccept); return ok");
  ASSERT_TRUE(closeError && *closeError);
  EXPECT_EQ(**closeError, Json(false));
  EXPECT_TRUE(pane->isDisplayed());
  ASSERT_TRUE(panes.dismissPane(pane));
  EXPECT_EQ(cinemaLoads, 1);
  ASSERT_TRUE(replace("zzdialog-offer", "Offer"));
  auto errorView = domain->pendingDialog("offer");
  ASSERT_TRUE(errorView);
  auto errorToken = errorView->getUInt("token");
  ASSERT_TRUE(errorToken);
  ASSERT_TRUE(controller->eval<LuaValue>("player.setProperty('zzdialogThrow', true)"));
  // Invoke through Lua pcall so the intentional domain exception is observed,
  // not converted to a component error/log or silently suppressed.
  auto thrown = controller->eval<Json>(strf("local ok,err=pcall(player.finishQuestDialog,{},'cancel'); return {{ok,tostring(err):find('quest decision regression',1,true)~=nil}}", *errorToken));
  ASSERT_TRUE(thrown);
  EXPECT_EQ(*thrown, (Json(JsonArray{false, true})));
  EXPECT_FALSE(domain->dialogValid(*errorToken));
  ASSERT_TRUE(controller->eval<LuaValue>("player.setProperty('zzdialogThrow', false)"));
  ASSERT_TRUE(controller->eval<LuaValue>("manager:shutdown()"));
});

const GameTestRegistration planetTextPresentation("LuaPlanetText", "NativeTimerDomainAndPersistentSessionPresentation", [](GameTest& fixture) {
  auto directory = File::temporaryDirectory(fixture.root().random());
  ASSERT_TRUE(directory);
  auto cleanup = finally([&] { EXPECT_TRUE(File::removeDirectoryRecursive(*directory)); });
  auto overlay = File::relativeTo(*directory, "assets");
  ASSERT_TRUE(File::makeDirectory(overlay));
  auto patchPath = File::relativeTo(overlay, "interface.config.patch");
  auto writeStyle = [&](float duration, float fade) -> std::expected<void, StarException> {
    auto json = Json(JsonObject{
        {"planetNameTime", duration}, {"planetNameFadeTime", fade},
        {"planetNameFormatString", "[<worldName>] <worldName>"},
        {"planetTextOffset", JsonArray{-7, 121}},
        {"planetTextStyle", JsonObject{{"fontSize", 24}, {"shadow", "red"},
            {"backDirectives", "border=1;000;000"}, {"color", "blue"}}}}).repr();
    if (!json) return std::unexpected(std::move(json.error()));
    return File::writeFile(*json, patchPath);
  };
  ASSERT_TRUE(writeStyle(0.7f, 0.3f));
  auto settings = fixture.rootSettings();
  settings.assetSources.append(overlay);
  settings.storageDirectory = File::relativeTo(*directory, "storage");
  settings.runtimeConfigFile = String("planettext.config");
  settings.logDirectory = {};
  settings.logFile = {};
  settings.includeUGC = false;
  auto createdRoot = Root::create(settings);
  ASSERT_TRUE(createdRoot);
  auto& root = **createdRoot;
  auto configuration = root.configuration();
  ASSERT_TRUE(configuration);
  ASSERT_TRUE((*configuration)->set("bindings", JsonObject{}));
  auto context = GuiContext::create(root, {}, {});
  ASSERT_TRUE(context);
  auto assets = root.assets();
  ASSERT_TRUE(assets);
  auto available = MainInterface::paneIdAvailable("PlanetText");
  ASSERT_TRUE(available);
  EXPECT_TRUE(*available);
  auto config = (*assets)->json("/interface/opensb/planettext/planettext.config");
  ASSERT_TRUE(config);
  auto paneConfig = config->toObject();
  ASSERT_TRUE(paneConfig);
  // Real LabelWidget construction requires renderInit, even for an empty
  // string. Use a real native TextBox for text/visibility policy, not a mock
  // renderer or callback echo. Label metrics/style pixels are a GUI gate.
  paneConfig->set("gui", JsonObject{});
  auto createdPane = ScriptTestPane::create(**context, Json(*paneConfig));
  ASSERT_TRUE(createdPane);
  auto pane = *createdPane;
  pane->setAnchor(PaneAnchor::Center);
  auto text = textEditor(**context, "");
  ASSERT_TRUE(text);
  (*text)->setMaxWidth(-1);
  ASSERT_TRUE(pane->addChild("planetText", *text));
  auto luaRoot = LuaRoot::create(root);
  ASSERT_TRUE(luaRoot);
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  auto factory = root.playerFactory();
  ASSERT_TRUE(factory);
  auto player = (*factory)->create();
  ASSERT_TRUE(player);
  auto playerCallbacks = LuaBindings::makePlayerCallbacks(root, **player);
  ASSERT_TRUE(playerCallbacks);
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  auto rootCallbacks = LuaBindings::makeRootCallbacks(root);
  ASSERT_TRUE(rootCallbacks);
  ASSERT_TRUE(pane->addCallbacks("root", *rootCallbacks));
  auto cinematic = Cinematic::create(**context);
  ASSERT_TRUE(cinematic);
  // Cinematic uses a real clock. Keep it active through expensive Root reloads;
  // each transition below stops it explicitly instead of racing a one-second end.
  auto startCinema = [&] {
    return (*cinematic)->load(JsonObject{{"panels", JsonArray{JsonObject{
        {"keyframes", JsonArray{JsonObject{{"timecode", 0}},
            JsonObject{{"timecode", std::numeric_limits<float>::max()}}}}}}},
        {"audio", JsonArray{}}});
  };
  bool hasPlayer = true, worldAvailable = true;
  float scale = 1.25f;
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("session", [&] {
    return JsonObject{{"hasPlayer", hasPlayer}, {"inWorld", worldAvailable}};
  }));
  ASSERT_TRUE(interface.registerCallback("cinematicActive", [&] { return !(*cinematic)->completed(); }));
  ASSERT_TRUE(interface.registerCallback("scale", [&] { return scale; }));
  ASSERT_TRUE(pane->addCallbacks("interface", interface));

  // Stored native templates let the name query read the exact domain name,
  // including tag-like characters, without generating/serializing a planet.
  auto blankTemplate = WorldTemplate::create(root, Vec2U{2048, 2048});
  ASSERT_TRUE(blankTemplate);
  auto storedTemplate = (*blankTemplate)->store();
  ASSERT_TRUE(storedTemplate);
  auto namedTemplate = [&](String const& name) -> std::expected<std::shared_ptr<WorldTemplate>, StarException> {
    auto celestial = CelestialParameters::create(root, CelestialCoordinate{}, 1, name, JsonObject{});
    if (!celestial) return std::unexpected(std::move(celestial.error()));
    auto storedCelestial = celestial->diskStore();
    if (!storedCelestial) return std::unexpected(std::move(storedCelestial.error()));
    auto stored = storedTemplate->set("celestialParameters", *storedCelestial);
    if (!stored) return std::unexpected(std::move(stored.error()));
    return WorldTemplate::create(root, *stored);
  };
  auto templateA = namedTemplate("A <worldName> 中");
  auto templateB = namedTemplate("B");
  ASSERT_TRUE(templateA && templateB);
  auto makeWorld = [&](std::shared_ptr<WorldTemplate> worldTemplate) -> std::expected<std::shared_ptr<WorldServer>, StarException> {
    auto storage = File::ephemeralFile(root.random());
    if (!storage) return std::unexpected(std::move(storage.error()));
    return WorldServer::create(root, std::move(worldTemplate), *storage);
  };
  auto worldA = makeWorld(*templateA);
  auto worldB = makeWorld(*templateB);
  auto emptyWorld = makeWorld(*blankTemplate);
  ASSERT_TRUE(worldA && worldB && emptyWorld);
  auto closeWorlds = finally([&] {
    if ((*player)->inWorld()) EXPECT_TRUE((*player)->uninit());
    EXPECT_TRUE((*worldA)->close());
    EXPECT_TRUE((*worldB)->close());
    EXPECT_TRUE((*emptyWorld)->close());
  });
  auto bindWorld = [&](WorldServer& world) -> std::expected<void, StarException> {
    auto callbacks = LuaBindings::makeWorldCallbacks(root, world);
    if (!callbacks) return std::unexpected(std::move(callbacks.error()));
    auto removed = pane->removeCallbacks("world");
    if (!removed) return std::unexpected(std::move(removed.error()));
    return pane->addCallbacks("world", *callbacks);
  };
  ASSERT_TRUE(bindWorld(**worldA));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Hud, pane, {}));
  EXPECT_EQ(pane->size(), (Vec2I{0, 0}));
  EXPECT_FALSE(pane->interactive());
  EXPECT_FALSE(pane->visibility());
  auto publicCallbacks = pane->makePaneCallbacks();
  ASSERT_TRUE(publicCallbacks);
  auto probe = (*luaRoot)->createContext(StringList{});
  ASSERT_TRUE(probe);
  ASSERT_TRUE(probe->setCallbacks("planet", *publicCallbacks));
  ASSERT_TRUE(probe->eval<LuaValue>("cached = planet.toWidget()"));

  GameTimer expected(0.7f);
  float fade = 0.3f;
  auto activePlayer = *player;
  auto step = [&](float dt) {
    bool ready = false;
    if ((*cinematic)->completed())
      ready = expected.tick(dt);
    else if (!hasPlayer || !activePlayer->inWorld())
      expected.reset();
    auto ticked = pane->tick(dt);
    ASSERT_TRUE(ticked) << ticked.error().what();
    auto timer = pane->evaluate("return storage.timer");
    ASSERT_TRUE(timer && *timer);
    auto timerValue = (**timer).toFloat();
    ASSERT_TRUE(timerValue);
    EXPECT_EQ(*timerValue, expected.timer);
    if ((*cinematic)->completed() && !ready) {
      float alpha = expected.timer < fade ? expected.timer / fade : 1.0f;
      auto actual = pane->evaluate("return storage.alpha");
      ASSERT_TRUE(actual && *actual);
      auto actualAlpha = (**actual).toFloat();
      ASSERT_TRUE(actualAlpha);
      EXPECT_EQ(*actualAlpha, alpha);
    }
  };
  ASSERT_TRUE(startCinema());
  step(0.4f); // Off-world cinematic resets, without advancing.
  EXPECT_FALSE(pane->visibility());
  ASSERT_TRUE((*player)->init(**worldA, 1, EntityMode::Slave));
  step(0.2f); // In-world cinematic pauses; no show or color change.
  EXPECT_FALSE(pane->visibility());
  (*cinematic)->stop();
  step(0.1f);
  EXPECT_TRUE(pane->visibility());
  EXPECT_EQ((*text)->getText(), String("[A <worldName> 中] A <worldName> 中"));
  EXPECT_EQ(pane->relativePosition(), (Vec2I{-7, 121} * (2.f / scale)));
  ASSERT_TRUE(startCinema());
  step(2.0f); // An in-world cinematic does not expire an already visible name.
  EXPECT_TRUE(pane->visibility());
  (*cinematic)->stop();
  ASSERT_TRUE(bindWorld(**worldB)); // Animation-free world change does not reset.
  scale = 3.0f;
  step(0.1f);
  EXPECT_EQ((*text)->getText(), String("[B] B"));
  EXPECT_EQ(pane->relativePosition(), (Vec2I{-7, 121} * (2.f / scale)));
  ASSERT_TRUE(bindWorld(**emptyWorld));
  step(0.1f); // Empty name retains old displayed text.
  EXPECT_TRUE(pane->visibility());
  EXPECT_EQ((*text)->getText(), String("[B] B"));
  worldAvailable = false; // No world capability/template, actor still present.
  ASSERT_TRUE(pane->removeCallbacks("world"));
  ASSERT_TRUE(pane->addCallbacks("world", LuaCallbacks{}));
  step(0.1f);
  EXPECT_TRUE(pane->visibility());
  ASSERT_TRUE(root.reload());
  step(0.01f); // Reload preserves timer/text/handle, no init-time tick.
  EXPECT_EQ((*text)->getText(), String("[B] B"));
  ASSERT_TRUE(pane->stopScripts());
  auto replacement = (*factory)->create();
  ASSERT_TRUE(replacement);
  auto replacementCallbacks = LuaBindings::makePlayerCallbacks(root, **replacement);
  ASSERT_TRUE(replacementCallbacks);
  ASSERT_TRUE(pane->removeCallbacks("player"));
  ASSERT_TRUE(pane->addCallbacks("player", *replacementCallbacks));
  activePlayer = *replacement;
  ASSERT_TRUE(pane->resumeScripts());
  step(0.01f); // Off-world without a cinematic continues, never resets.
  EXPECT_TRUE(pane->visibility());
  step(1.0f);
  EXPECT_FALSE(pane->visibility());
  ASSERT_TRUE(startCinema());
  step(0.1f); // Replacement actor is off-world: reset and hide.
  ASSERT_TRUE(pane->stopScripts());
  ASSERT_TRUE(pane->removeCallbacks("player"));
  ASSERT_TRUE(pane->addCallbacks("player", *playerCallbacks));
  activePlayer = *player;
  ASSERT_TRUE(pane->resumeScripts());
  worldAvailable = true;
  ASSERT_TRUE(bindWorld(**worldA));
  (*cinematic)->stop();
  step(0.1f);
  EXPECT_TRUE(pane->visibility());
  auto retained = probe->eval<Json>("return {planet.isDisplayed(), cached.getText('planetText')}");
  ASSERT_TRUE(retained);
  EXPECT_EQ(*retained, (Json(JsonArray{true, "[A <worldName> 中] A <worldName> 中"})));
  EXPECT_EQ(pane->fetchChild<TextBoxWidget>("planetText")->get(), text->get());

  // Native approach handles signed duration/dt, and a zero/nonpositive fade
  // never divides when timer >= fade. Exercise these asset values via reload.
  ASSERT_TRUE((*player)->uninit());
  for (auto [duration, fadeTime] : {std::pair{0.0f, 0.0f}, std::pair{-0.5f, 0.3f},
      std::pair{0.7f, 0.0f}, std::pair{0.7f, -0.2f}, std::pair{0.7f, 1.3f}}) {
    ASSERT_TRUE(writeStyle(duration, fadeTime));
    ASSERT_TRUE(root.reload());
    expected = GameTimer(duration);
    fade = fadeTime;
    ASSERT_TRUE(startCinema());
    step(0.0f);
    (*cinematic)->stop();
    for (float dt : {0.0f, -0.1f, 0.1f, 0.2f, 1.0f})
      step(dt);
  }
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration notificationPresentation("LuaNotifications", "OverflowEvictsOldestAndPickupIdentitySurvivesReload", [](GameTest& fixture) {
  auto context = textEditorContext(fixture);
  ASSERT_TRUE(context);
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto config = (*assets)->json("/interface/opensb/notifications/notifications.config");
  ASSERT_TRUE(config);
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot);
  auto created = ScriptTestPane::create(**context, *config);
  ASSERT_TRUE(created);
  auto pane = *created;
  ASSERT_TRUE(pane->setLuaRoot(*luaRoot));
  auto rootCallbacks = LuaBindings::makeRootCallbacks(fixture.root());
  ASSERT_TRUE(rootCallbacks);
  ASSERT_TRUE(pane->addCallbacks("root", *rootCallbacks));
  WorldCamera camera;
  camera.setScreenSize({800, 600});
  auto cameraCallbacks = LuaBindings::makeCameraCallbacks(fixture.root(), camera);
  ASSERT_TRUE(cameraCallbacks);
  ASSERT_TRUE(pane->addCallbacks("camera", *cameraCallbacks));
  // Queue/storage behavior only: real Canvas commands, no renderer or drawing claims.
  LuaCallbacks interface;
  ASSERT_TRUE(interface.registerCallback("scale", [] { return 1.25f; }));
  ASSERT_TRUE(pane->addCallbacks("interface", interface));
  PaneManager panes(**context);
  ASSERT_TRUE(panes.displayPane(PaneLayer::Background, pane, {}));

  ASSERT_TRUE(pane->call("push", {JsonObject{{"text", "expires"}, {"cooldown", 0.5}}}));
  ASSERT_TRUE(pane->tick(0.5f));
  auto zero = pane->evaluate("return {#storage.messages, storage.messages[1].cooldown}");
  ASSERT_TRUE(zero && *zero);
  EXPECT_EQ(**zero, (JsonArray{1, 0}));
  ASSERT_TRUE(pane->tick(0.125f));
  auto expired = pane->evaluate("return #storage.messages");
  ASSERT_TRUE(expired && *expired);
  EXPECT_EQ(**expired, Json(0));

  auto maximum = (*assets)->json("/interface.config:maxMessageCount");
  ASSERT_TRUE(maximum);
  auto capacity = maximum->toUInt();
  ASSERT_TRUE(capacity);
  ASSERT_TRUE(pane->evaluate(strf(
      "for index=1,{} do push({{text=tostring(index),cooldown=100}}) end", *capacity + 3)));
  for (unsigned i = 0; i < 3; ++i)
    ASSERT_TRUE(pane->tick(0.125f));
  auto overflow = pane->evaluate(R"LUA(
    local remaining = jarray()
    for _, message in ipairs(storage.messages) do
      if not message.overflow then remaining[#remaining + 1] = message.text end
    end
    return {#storage.messages, storage.overflowCount, remaining}
  )LUA");
  ASSERT_TRUE(overflow && *overflow);
  JsonArray remaining;
  for (unsigned i = 4; i <= *capacity + 3; ++i)
    remaining.append(strf("{}", i));
  EXPECT_EQ(**overflow, (JsonArray{*capacity + 1, 3, remaining}));
  ASSERT_TRUE(fixture.root().reload());
  ASSERT_TRUE(pane->tick(0));
  auto restored = pane->evaluate("return {#storage.messages, storage.overflowCount, storage.messages[1].overflow}");
  ASSERT_TRUE(restored && *restored);
  EXPECT_EQ(**restored, (JsonArray{*capacity + 1, 3, true}));
  ASSERT_TRUE(pane->tick(1000));
  ASSERT_TRUE(pane->tick(0));

  auto pickup = [&](int variant, int count) {
    return pane->call("pickup", {JsonObject{{"name", "石块"},
        {"count", count}, {"descriptor", JsonObject{{"name", "cobblestonematerial"},
            {"count", 1}, {"parameters", JsonObject{{"variant", variant}}}}}}});
  };
  ASSERT_TRUE(pickup(1, 2));
  ASSERT_TRUE(pickup(1, 3));
  ASSERT_TRUE(pickup(2, 7));
  auto merged = pane->evaluate("return {#storage.messages,storage.messages[1].count,storage.messages[2].count}");
  ASSERT_TRUE(merged && *merged);
  EXPECT_EQ(**merged, (JsonArray{2, 5, 7}));
  ASSERT_TRUE(pane->tick(1000));
  ASSERT_TRUE(pickup(1, 2));
  auto renewed = pane->evaluate("return {#storage.messages,storage.messages[1].count}");
  ASSERT_TRUE(renewed && *renewed);
  EXPECT_EQ(**renewed, (JsonArray{1, 2}));
  ASSERT_TRUE(panes.dismissAllPanes());
});

const GameTestRegistration mainbarGeometry("LuaMainBar", "PhysicalPolygonTransformsPreserveNativeBoundaryRules", [](GameTest& fixture) {
  auto assets = fixture.root().assets();
  ASSERT_TRUE(assets);
  auto config = (*assets)->json("/interface.config:mainBar");
  ASSERT_TRUE(config);
  auto sizeJson = config->get("size");
  ASSERT_TRUE(sizeJson);
  auto barSize = jsonToVec2I(*sizeJson);
  ASSERT_TRUE(barSize);
  auto luaRoot = LuaRoot::create(fixture.root());
  ASSERT_TRUE(luaRoot);
  auto script = (*luaRoot)->createContext("/interface/opensb/mainbar/geometry.lua");
  ASSERT_TRUE(script);
  auto callbacks = LuaBindings::makeRootCallbacks(fixture.root());
  ASSERT_TRUE(callbacks);
  ASSERT_TRUE(script->setCallbacks("root", *callbacks));
  for (Vec2U screen : {Vec2U{800, 600}, Vec2U{801, 601}}) {
    for (float scale : {1.0f, 1.25f, 3.0f}) {
      auto layout = script->eval<Json>(strf(
          "layout=mainbarPresentation.layout(root.assetJson('/interface.config:mainBar'), {{{},{}}}, {}); return layout",
          screen[0], screen[1], scale));
      ASSERT_TRUE(layout) << layout.error().what();
      Vec2F origin = Vec2F(screen) - Vec2F(*barSize) * scale;
      for (String const& name : StringList{"inventory", "craft", "codex", "deploy", "beam", "questLog", "mmUpgrade", "collections"}) {
        SCOPED_TRACE(strf("{} {}x{} scale{}", name, screen[0], screen[1], scale));
        auto button = config->get(name);
        ASSERT_TRUE(button);
        auto polygonJson = button->get("poly");
        ASSERT_TRUE(polygonJson);
        auto original = jsonToPolyI(*polygonJson);
        ASSERT_TRUE(original);
        PolyF polygon(*original);
        polygon.translate(origin);
        polygon.scale(scale, origin);
        auto bounds = polygon.boundBox();
        for (Vec2F point : {bounds.center(), bounds.min(), bounds.max(),
            Vec2F{bounds.xMin(), bounds.center().y()}, Vec2F{bounds.xMax(), bounds.center().y()},
            Vec2F{bounds.center().x(), bounds.yMin()}, Vec2F{bounds.center().x(), bounds.yMax()},
            Vec2F{bounds.xMin() - 0.25f, bounds.center().y()}, Vec2F{bounds.xMax() + 0.25f, bounds.center().y()}}) {
          auto hit = script->eval<bool>(strf(
              "return mainbarPresentation.contains(layout.polygons.{}, {{{},{}}})", name, point[0], point[1]));
          ASSERT_TRUE(hit) << hit.error().what();
          EXPECT_EQ(*hit, polygon.contains(point));
        }
      }
    }
  }
});

}
