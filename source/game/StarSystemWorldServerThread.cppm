module;
import star.format;
import std;
import star.string;
import star.map;
import star.set;
import star.json;
import star.vector;
import star.game_types;
import star.color;
import star.bi_map;
import star.random;
import star.weighted_pool;
import star.list;
import star.image_processing;
import star.hash;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.strong_typedef;
import star.either;
import star.net_compatibility;


import star.celestial_coordinate;
import star.sky_types;
import star.animation;
import star.particle;
import star.weather_types;
import star.force_regions;
import star.world_parameters;
import star.celestial_parameters;
import star.uuid;
import star.warping;
import star.sky_parameters;
import star.system_world;

namespace Star {

struct Packet;

using ClientShipAction =
  std::function<std::expected<void, StarException>(std::optional<std::reference_wrapper<SystemClientShip>>)>;

class SystemWorldServerThread : public Thread {
public:
  SystemWorldServerThread(Vec3I const& location, std::shared_ptr<SystemWorldServer> systemWorld, String storageFile);
  ~SystemWorldServerThread() override;

  Vec3I location() const;

  List<ConnectionId> clients();
  [[nodiscard]] std::expected<void, StarException> addClient(ConnectionId clientId, Uuid const& uuid, float shipSpeed, SystemLocation const& location);
  [[nodiscard]] std::expected<void, StarException> removeClient(ConnectionId clientId);

  void setPause(std::shared_ptr<const std::atomic<bool>> pause);
  [[nodiscard]] std::expected<bool, StarException> start();
  [[nodiscard]] std::expected<void, StarException> run() override;
  void stop();

  [[nodiscard]] std::expected<void, StarException> update();

  void setClientDestination(ConnectionId clientId, SystemLocation const& location);
  [[nodiscard]] std::expected<void, StarException> executeClientShipAction(ConnectionId clientId, ClientShipAction action);

  [[nodiscard]] std::expected<SystemLocation, StarException> clientShipLocation(ConnectionId clientId);
  std::optional<std::pair<WarpAction, WarpMode>> clientWarpAction(ConnectionId clientId);
  [[nodiscard]] std::expected<SkyParameters, StarException> clientSkyParameters(ConnectionId clientId);

  List<InstanceWorldId> activeInstanceWorlds() const;

  // callback to be run after update in the server thread
  void setUpdateAction(std::function<std::expected<void, StarException>(SystemWorldServerThread&)> updateAction);
  void pushIncomingPacket(ConnectionId clientId, std::shared_ptr<Packet> packet);
  List<std::shared_ptr<Packet>> pullOutgoingPackets(ConnectionId clientId);

  [[nodiscard]] std::expected<void, StarException> store();

private:
  Vec3I m_systemLocation;
  std::shared_ptr<SystemWorldServer> m_systemWorld;

  std::stop_source m_stopSource{std::nostopstate};
  float m_periodicStorage{300.0f};
  bool m_triggerStorage{ false};
  String m_storageFile;

  std::shared_ptr<const std::atomic<bool>> m_pause;
  std::function<std::expected<void, StarException>(SystemWorldServerThread&)> m_updateAction;

  ReadersWriterMutex m_mutex;
  ReadersWriterMutex m_queueMutex;

  HashSet<ConnectionId> m_clients;
  HashMap<ConnectionId, SystemLocation> m_clientShipDestinations;
  HashMap<ConnectionId, std::pair<SystemLocation, SkyParameters>> m_clientShipLocations;
  HashMap<ConnectionId, std::pair<WarpAction, WarpMode>> m_clientWarpActions;
  List<std::pair<ConnectionId, ClientShipAction>> m_clientShipActions;
  List<InstanceWorldId> m_activeInstanceWorlds;
  Map<ConnectionId, List<std::shared_ptr<Packet>>> m_outgoingPacketQueue;
  List<std::pair<ConnectionId, std::shared_ptr<Packet>>> m_incomingPacketQueue;
};


}

export module star.system_world_server_thread;

export namespace Star {
  using ::Star::ClientShipAction;
  using ::Star::SystemWorldServerThread;
}
