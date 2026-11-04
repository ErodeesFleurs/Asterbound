module;
import std;
import star.format;
import star.json;
import star.id_map;
import star.game_types;
import star.vector;
import star.casting;
import star.input_event;
import star.color;
import star.list;
import star.rect;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.bi_map;
import star.string;
import star.text;
import star.poly;
import star.asset_path;
import star.listener;
import star.set;
import star.audio;
import star.map;
import star.strong_typedef;
import star.rpc_promise;
import star.ordered_map;
import star.lua_components;


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
import star.inventory_types;
import star.item_descriptor;

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

import star.container_entity;

import star.container_interactor;

import star.game_timers;

import star.inventory;

import star.animation;

import star.interface_cursor;

import star.pane_manager;
import star.registered_pane_manager;

import star.main_interface_types;
import star.uuid;
import star.warping;
import star.chat_types;

namespace Star {
enum class P2PJoinRequestReply;
class InventoryPane;
class ItemSlotWidget;
}

namespace Star {

class UniverseClient;
class WorldPainter;
class Item;
class PortraitEntity;
class BaseScriptPane;
class LuaRoot;
class WorldClient;
class TeamClient;
class ClientCommandProcessor;
class OptionsMenu;
class WirePane;
class ContainerPane;
class CraftingPane;
class MerchantPane;
class Songbook;
class AiInterface;
class ConfirmationDialog;
class JoinRequestDialog;
class TeleportDialog;
class LabelWidget;
class Cinematic;
class NameplatePainter;
class QuestIndicatorPainter;
class ContainerInteractor;
class ScriptPane;
class ChatBubbleManager;
class CanvasWidget;


class MainInterface {
public:
  enum RunningState {
    Running,
    ReturnToTitle
  };

  [[nodiscard]] static std::expected<std::unique_ptr<MainInterface>, StarException> create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, std::shared_ptr<WorldPainter> painter, std::shared_ptr<Cinematic> cinematicOverlay);

  ~MainInterface();
  [[nodiscard]] std::expected<void, StarException> close();

  RunningState currentState() const;
  void returnToTitle();

  MainInterfacePaneManager& paneManager();
  GuiContext& context() const;
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> itemTooltip(std::shared_ptr<Item> const& item);
  [[nodiscard]] std::expected<void, StarException> playCinematic(Json const& config);
  bool cinematicActive() const;
  [[nodiscard]] std::expected<Json, StarException> travelState() const;
  [[nodiscard]] std::expected<bool, StarException> travel(String const& operation, std::optional<Uuid> const& member);
  std::shared_ptr<PortraitEntity> portraitEntity(std::optional<EntityId> id) const;
  Json cursorView() const;
  bool inventoryChanged() const;
  // Escaped callbacks borrow this interface only until terminal close.
  std::stop_token callbackStopToken() const;

  [[nodiscard]] std::expected<bool, StarException> pauseRequested() const;

  [[nodiscard]] std::expected<void, StarException> openCraftingWindow(Json const& config, EntityId sourceEntityId = NullEntityId);
  [[nodiscard]] std::expected<void, StarException> openMerchantWindow(Json const& config, EntityId sourceEntityId = NullEntityId);
  [[nodiscard]] std::expected<void, StarException> togglePlainCraftingWindow();

  bool windowsOpen() const;

  std::shared_ptr<MerchantPane> activeMerchantPane() const;

  // Return true if this event was consumed or should be handled elsewhere.
  [[nodiscard]] std::expected<bool, StarException> handleInputEvent(InputEvent const& event);
  // Return true if mouse / keyboard events are currently locked here
  [[nodiscard]] std::expected<bool, StarException> inputFocus() const;
  // If input is focused, should MainInterface also accept text input events?
  [[nodiscard]] std::expected<bool, StarException> textInputActive() const;
  [[nodiscard]] std::expected<void, StarException> handleInteractAction(InteractAction interactAction);

  [[nodiscard]] std::expected<void, StarException> preUpdate(float dt);
  // Handles incoming client messages, aims main player, etc.
  [[nodiscard]] std::expected<void, StarException> update(float dt);

  // Render things e.g. quest indicators that should be drawn in the world
  // behind interface e.g. chat bubbles
  [[nodiscard]] std::expected<void, StarException> renderInWorldElements(float interpolation);
  [[nodiscard]] std::expected<void, StarException> render();

  Vec2F cursorWorldPosition() const;

  bool isDebugDisplayed();

  [[nodiscard]] std::expected<StringList, StarException> executeClientCommand(String const& command);
  [[nodiscard]] std::expected<void, StarException> addChatMessages(List<ChatReceivedMessage> const& messages, bool showPane = true);
  [[nodiscard]] std::expected<std::optional<Json>, StarException> callPane(String const& id, String const& method, JsonArray const& arguments = {});
  [[nodiscard]] std::expected<std::optional<Json>, StarException> callUi(String const& id, String const& method, JsonArray const& arguments = {});
  [[nodiscard]] std::expected<void, StarException> sendPaneEvent(String const& id, String const& event, Json payload);
  [[nodiscard]] std::expected<void, StarException> createPane(String const& id, Json config, String const& layer);
  [[nodiscard]] std::expected<void, StarException> destroyPane(String const& id);
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeClientCallbacks();
  Json uiSession() const;
  [[nodiscard]] static std::expected<bool, StarException> paneIdAvailable(String const& id);
  [[nodiscard]] std::expected<JsonArray, StarException> paneInputTargets() const;
  [[nodiscard]] std::expected<bool, StarException> paneLayerOccupied(String const& layer) const;
  [[nodiscard]] std::expected<std::shared_ptr<Pane>, StarException> findRegisteredPane(String const& id);
  [[nodiscard]] std::expected<void, StarException> displayRegisteredPane(String const& id);
  [[nodiscard]] std::expected<void, StarException> togglePane(String const& id);

  [[nodiscard]] std::expected<void, StarException> queueMessage(String const& message, std::optional<float> cooldown, float spring);
  [[nodiscard]] std::expected<void, StarException> queueMessage(String const& message);

  [[nodiscard]] std::expected<void, StarException> queueItemPickupText(std::shared_ptr<Item> const& item);
  [[nodiscard]] std::expected<void, StarException> queueJoinRequest(std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>> request);

  bool fixedCamera() const;
  bool hudVisible() const;
  void setHudVisible(bool visible = true);

  [[nodiscard]] std::expected<void, StarException> warpToOrbitedWorld(bool deploy = false);
  [[nodiscard]] std::expected<void, StarException> warpToOwnShip();
  [[nodiscard]] std::expected<void, StarException> warpTo(WarpAction const& warpAction);

  [[nodiscard]] std::expected<std::shared_ptr<CanvasWidget>, StarException> fetchCanvas(String const& canvasName, bool ignoreInterfaceScale = false);

  std::shared_ptr<ClientCommandProcessor> commandProcessor() const;

  struct ScriptPaneInfo {
    std::shared_ptr<ScriptPane> scriptPane;
    Json config;
    EntityId sourceEntityId;
    bool visible;
    Vec2I position;
  };

  [[nodiscard]] std::expected<void, StarException> takeScriptPanes(List<ScriptPaneInfo>& out);
  [[nodiscard]] std::expected<void, StarException> reviveScriptPanes(List<ScriptPaneInfo>& panes);
private:
  MainInterface(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, WorldPainter& painter, std::shared_ptr<Cinematic> cinematicOverlay);
  [[nodiscard]] std::expected<void, StarException> initialize(std::shared_ptr<WorldPainter> painter);
  [[nodiscard]] std::expected<void, StarException> initMainBarPanes();
  [[nodiscard]] std::expected<void, StarException> initScriptAndHudPanes(std::shared_ptr<WorldPainter> painter);
  [[nodiscard]] std::expected<void, StarException> initUiController();
  [[nodiscard]] std::expected<void, StarException> synchronizeUiSession();
  [[nodiscard]] std::expected<void, StarException> initCharacterSwap();
  [[nodiscard]] std::expected<void, StarException> initWorldOverlays();
  [[nodiscard]] std::expected<void, StarException> handleInventoryDismissed();
  [[nodiscard]] std::expected<void, StarException> initHttpTrustDialog();

  float interfaceScale() const;
  [[nodiscard]] std::expected<unsigned, StarException> windowHeight() const;
  [[nodiscard]] std::expected<unsigned, StarException> windowWidth() const;
  [[nodiscard]] std::expected<void, StarException> renderWindows();
  [[nodiscard]] std::expected<void, StarException> renderDebug();

  [[nodiscard]] std::expected<void, StarException> updateCursor();
  [[nodiscard]] std::expected<void, StarException> renderCursor();


  [[nodiscard]] std::expected<void, StarException> displayScriptPane(std::shared_ptr<ScriptPane>& scriptPane, EntityId sourceEntity);

  std::reference_wrapper<GuiContext> m_guiContext;
  std::unique_ptr<MainInterfaceConfig const> m_config;
  std::shared_ptr<InterfaceCursor> m_cursor;

  RunningState m_state{Running};

  std::shared_ptr<UniverseClient> m_client;
  // ClientApplication retains the painter; the escapable WirePane separately shares it.
  std::reference_wrapper<WorldPainter> m_worldPainter;
  std::shared_ptr<Cinematic> m_cinematicOverlay;

  MainInterfacePaneManager m_paneManager;
  std::stop_source m_callbackStopSource;
  std::vector<std::weak_ptr<TeleportDialog>> m_teleportDialogs;


  std::shared_ptr<InventoryPane> m_inventoryWindow;
  std::shared_ptr<CraftingPane> m_plainCraftingWindow;
  std::shared_ptr<CraftingPane> m_craftingWindow;
  std::shared_ptr<MerchantPane> m_merchantWindow;
  std::shared_ptr<OptionsMenu> m_optionsMenu;
  std::shared_ptr<ContainerPane> m_containerPane;
  std::shared_ptr<ConfirmationDialog> m_confirmationDialog;
  std::shared_ptr<JoinRequestDialog> m_joinRequestDialog;
  std::shared_ptr<TeleportDialog> m_teleportDialog;
  std::shared_ptr<ScriptPane> m_mmUpgrade;
  std::shared_ptr<ScriptPane> m_collections;
  Map<EntityId, std::shared_ptr<Pane>> m_interactionScriptPanes;

  StringMap<std::shared_ptr<CanvasWidget>> m_canvases;

  Map<String, std::pair<std::shared_ptr<BaseScriptPane>, PaneLayer>> m_paneInstances;
  std::unique_ptr<LuaUpdateScript> m_uiController;
  Map<String, LuaCallbacks> m_uiCapabilities;
  std::shared_ptr<Player> m_uiPlayer;
  std::optional<Uuid> m_uiPlayerUuid;
  std::shared_ptr<Songbook> m_uiSongbook;
  std::shared_ptr<WorldClient> m_uiWorld;
  std::shared_ptr<TeamClient> m_uiTeam;
  std::shared_ptr<LuaRoot> m_uiRoot;
  WorldId m_uiWorldId;
  std::uint64_t m_uiRootGeneration = 0;
  std::uint64_t m_uiWorldGeneration = 0;
  std::uint64_t m_uiPlayerGeneration = 0;
  bool m_uiInWorld = false;
  std::shared_ptr<ClientCommandProcessor> m_clientCommandProcessor;
  std::shared_ptr<WirePane> m_wireInterface;

  Vec2F m_cursorScreenPos{};
  Vec2I m_cursorScreenIPos{};
  std::shared_ptr<ItemSlotWidget> m_cursorItem;

  GameTimer m_debugSpatialClearTimer;
  GameTimer m_debugMapClearTimer;
  bool m_debugSpatialClearDue = false;
  bool m_debugMapClearDue = false;
  RectF m_debugTextRect{RectF::null()};

  std::unique_ptr<NameplatePainter> m_nameplatePainter;
  std::unique_ptr<QuestIndicatorPainter> m_questIndicatorPainter;
  std::unique_ptr<ChatBubbleManager> m_chatBubbleManager;

  bool m_disableHud = false;

  String m_lastCommand;
  bool m_commandInput = false;


  List<std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>>> m_queuedJoinRequests;


  std::shared_ptr<ContainerInteractor> m_containerInteractor;
};

}

export module star.main_interface;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::WorldPainter;
  using ::Star::Item;
  using ::Star::BaseScriptPane;
  using ::Star::ClientCommandProcessor;
  using ::Star::OptionsMenu;
  using ::Star::WirePane;
  using ::Star::ContainerPane;
  using ::Star::CraftingPane;
  using ::Star::MerchantPane;
  using ::Star::Songbook;
  using ::Star::AiInterface;
  using ::Star::ConfirmationDialog;
  using ::Star::JoinRequestDialog;
  using ::Star::TeleportDialog;
  using ::Star::LabelWidget;
  using ::Star::Cinematic;
  using ::Star::NameplatePainter;
  using ::Star::QuestIndicatorPainter;
  using ::Star::ContainerInteractor;
  using ::Star::ScriptPane;
  using ::Star::ChatBubbleManager;
  using ::Star::CanvasWidget;
  using ::Star::MainInterface;
}
