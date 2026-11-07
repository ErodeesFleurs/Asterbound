module;
import std;
import star.format;
import star.json;

import star.poly;
import star.bi_map;
import star.game_types;
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
import star.string;
import star.text;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.ordered_map;


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
import star.game_timers;
import star.pane_manager;
import star.registered_pane_manager;

namespace Star {

enum class MainInterfacePanes {
  Inventory,
  Cockpit,
  Tech,
  Ai,
  Confirmation,
  HttpTrustDialog, // sorry i need it here
  JoinRequest,
  Options,
  WireInterface,
  CraftingPlain,
  MmUpgrade,
  Collections,
  CharacterSwap
};

extern std::expected<EnumMap<MainInterfacePanes>, StarException> const MainInterfacePanesNames;

using MainInterfacePaneManager = RegisteredPaneManager<MainInterfacePanes>;

struct MainInterfaceConfig {
  [[nodiscard]] static std::expected<std::unique_ptr<MainInterfaceConfig>, StarException> loadFromAssets(GuiContext& context);

  TextStyle textStyle;


  Vec2I itemCountRightAnchor;
  Vec2I inventoryItemMouseOffset;


  bool renderVirtualCursor;
  Json cursorItemSlot;

  Vec2I debugOffset;
  TextStyle debugTextStyle;
  float debugSpatialClearTime;
  float debugMapClearTime;
  Color debugBackgroundColor;
  int debugBackgroundPad;

  StringMap<StringList> macroCommands;
};

}

export module star.main_interface_types;

export namespace Star {
  using ::Star::MainInterfaceConfig;
  using ::Star::MainInterfacePanes;
  using ::Star::MainInterfacePanesNames;
  using ::Star::MainInterfacePaneManager;
}
