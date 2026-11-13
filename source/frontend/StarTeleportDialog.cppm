module;
import std;
import star.type_identity;
import star.json;
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
import star.strong_typedef;
import star.random;
import star.weighted_pool;
import star.either;



import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.sky_parameters;
import star.system_world;




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
import star.uuid;
import star.celestial_coordinate;
import star.warping;
import star.player_universe_map;
import star.format;

namespace Star {

class UniverseClient;

class TeleportDialog : public Pane {
public:
  void const* query_type(TypeKey key) const noexcept override { return query_type_of<TeleportDialog, Pane>(this, key); }
  std::string_view dynamic_type_name() const noexcept override { return Star::type_name<TeleportDialog>(); }

  using SendPaneEvent = std::function<std::expected<void, StarException>(String const&, String const&, Json)>;
  [[nodiscard]] static std::expected<std::shared_ptr<TeleportDialog>, StarException> create(GuiContext& guiContext, std::shared_ptr<UniverseClient> client,
      SendPaneEvent sendPaneEvent, Json config, EntityId sourceEntityId, TeleportBookmark currentLocation);
  void detachInterface() { m_sendPaneEvent = {}; }

  [[nodiscard]] std::expected<void, StarException> tick(float dt) override;

  [[nodiscard]] std::expected<void, StarException> selectDestination();
  [[nodiscard]] std::expected<void, StarException> teleport();
  [[nodiscard]] std::expected<void, StarException> editBookmark(Widget& button);

private:
  TeleportDialog(GuiContext& guiContext, std::shared_ptr<UniverseClient> client, SendPaneEvent sendPaneEvent, EntityId sourceEntityId, TeleportBookmark currentLocation);
  [[nodiscard]] std::expected<void, StarException> initialize(Json config);
  EntityId m_sourceEntityId;
  std::shared_ptr<UniverseClient> m_client;
  SendPaneEvent m_sendPaneEvent;
  List<std::pair<WarpAction, bool>> m_destinations;
  TeleportBookmark m_currentLocation;
};

}

export module star.teleport_dialog;

export namespace Star {
  using ::Star::UniverseClient;
  using ::Star::TeleportDialog;
}
