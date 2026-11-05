module;
import star.xx_hash;
import star.json;
import star.format;
import star.id_map;
import star.game_types;
import star.image;
import star.list;
import star.bi_map;
import star.version;
import star.color;
import star.string;
import star.data_stream;
import star.image_processing;
import star.hash;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.either;
import star.random;
import star.weighted_pool;
import star.casting;
import star.strong_typedef;
import star.audio;
import star.map;
import star.set;
import star.rpc_promise;
import star.ordered_map;
import star.net_compatibility;
import std;
import star.worker_pool;

import star.sector_array_2d;
import star.perlin;
import star.btree;
import star.lru_cache;
import star.data_stream_devices;
import star.btree_database;
import star.ordered_set;

import star.collision_block;
import star.liquid_types;
import star.tile_damage;
import star.tile_sector_array;
import star.animation;
import star.particle;
import star.weather_types;
import star.celestial_coordinate;
import star.sky_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.world_layout;
import star.collision_generator;
import star.world_tiles;
import star.item_descriptor;
import star.celestial_types;
import star.chat_types;
import star.uuid;
import star.tile_modification;
import star.damage_types;
import star.status_types;
import star.damage;
import star.light_source;
import star.entity;
import star.interaction_types;
import star.warping;
import star.world_geometry;
import star.wiring;
import star.quest_descriptor;
import star.interactive_entity;
import star.tile_entity;
import star.inspectable_entity;
import star.plant;
import star.plant_database;
import star.biome_placement;
import star.lua_root;
import star.versioning_database;
import star.world_storage;
import star.player_types;
import star.sky_parameters;
import star.system_world;
import star.damage_manager;
import star.net_packets;
import star.drawable;
import star.entity_rendering_types;
import star.sky_render_data;
import star.parallax;
import star.cellular_light_array;
import star.cellular_lighting;
import star.world_render_data;
import star.world_structure;
import star.chat_action;
import star.mixer;
import star.entity_rendering;
import star.world;
import star.lua_components;
import star.world_client_state;
import star.interpolation_tracker;
import star.ambient;
import star.weather;
import star.game_timers;
import star.world_client;

namespace Star {
class Root;

class UniverseClient;

// Runs a WorldThreadedClient in a separate thread.
class WorldClientThread : public Thread {
public:
  struct Message {
    String message;
    JsonArray args;
    RpcPromiseKeeper<Json> promise;
  };

  typedef std::function<std::expected<void, StarException>(WorldClientThread&, WorldClient&)> WorldClientAction;

  [[nodiscard]]
  static std::expected<std::shared_ptr<WorldClientThread>, StarException> create(Root& root, ClientSubWorldId subWorldId, UniverseClient& universe);
  ~WorldClientThread();

  ClientSubWorldId subWorldId() const;

  [[nodiscard]]
  std::expected<void, StarException> start();
  // Signals the WorldClientThread to stop and then joins it.
  [[nodiscard]]
  std::expected<void, StarException> stop();
  void setPause(std::shared_ptr<const std::atomic<bool>> pause);

  // A world failure has been logged and stopped the client thread.
  bool errorOccurred();
  bool shouldExpire();

  // Clients that have caused an error with incoming packets are removed from
  // the world and no further packets are handled from them.  They are still
  // added to this WorldClientThread, and must be removed and the final
  // outgoing packets should be sent to them.

  void pushIncomingPackets(List<std::shared_ptr<Packet>> packets);
  List<std::shared_ptr<Packet>> pullOutgoingPackets();

  // Executes an action under the world lock; returns its failure to the caller.
  [[nodiscard]]
  std::expected<void, StarException> executeAction(WorldClientAction action);

  // If a callback is set here, then this is called after every world update,
  // also in a thread safe context.
  void setUpdateAction(WorldClientAction updateAction);

  // 
  void passMessage(Message&& message);
  [[nodiscard]]
  std::expected<void, StarException> clearMessages();

protected:
  [[nodiscard]]
  std::expected<void, StarException> run() override;

private:
  WorldClientThread(ClientSubWorldId subWorldId, std::shared_ptr<WorldClient> worldClient);
  [[nodiscard]]
  std::expected<void, StarException> update();
  [[nodiscard]]
  std::expected<void, StarException> runLoop(std::stop_token stopToken);

  mutable std::recursive_mutex m_mutex;

  std::shared_ptr<WorldClient> m_worldClient;
  ClientSubWorldId m_subWorldId;
  WorldClientAction m_updateAction;

  mutable std::recursive_mutex m_queueMutex;
  List<std::shared_ptr<Packet>> m_incomingPacketQueue;
  List<std::shared_ptr<Packet>> m_outgoingPacketQueue;

  mutable std::recursive_mutex m_messageMutex;
  List<Message> m_messages;

  std::stop_source m_stopSource;
  std::shared_ptr<const std::atomic<bool>> m_pause;
  mutable std::atomic<bool> m_errorOccurred;
  mutable std::atomic<bool> m_shouldExpire;
};

}

export module star.world_client_thread;

export namespace Star {
  using ::Star::WorldClientThread;
  using ::Star::UniverseClient;
}
