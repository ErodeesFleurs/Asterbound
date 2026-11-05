module;
import std;
import star.string;
import star.list;
import star.set;
import star.json;
import star.format;

import star.game_types;
import star.byte_array;
import star.map;
import star.net_compatibility;
import star.strong_typedef;
import star.data_stream;
import star.uuid;
import star.vector;
import star.celestial_coordinate;
import star.warping;
import star.thread;
import star.rpc_promise;

namespace Star {

class WorldServer;
struct Packet;
enum class WorldServerFidelity;
typedef HashMap<ByteArray, std::optional<ByteArray>> WorldChunks;

// Runs a WorldServer in a separate thread. World failures are logged at the
// original recovery boundaries and are also returned to synchronous callers.
class WorldServerThread : public Thread {
public:
  struct Message {
    String message;
    JsonArray args;
    RpcPromiseKeeper<Json> promise;
  };

  typedef std::function<std::expected<void, StarException>(WorldServerThread&, WorldServer&)> WorldServerAction;

  [[nodiscard]]
  static std::expected<std::shared_ptr<WorldServerThread>, StarException> create(std::shared_ptr<WorldServer> server, WorldId worldId);
  ~WorldServerThread();

  WorldId worldId() const;

  [[nodiscard]]
  std::expected<void, StarException> start();
  // Signals the WorldServerThread to stop and then joins it.
  [[nodiscard]]
  std::expected<void, StarException> stop();
  // Final teardown after client migration and chunk export have completed.
  // Unlike stop(), this removes clients and closes storage.
  [[nodiscard]]
  std::expected<void, StarException> close();
  void setPause(std::shared_ptr<const std::atomic<bool>> pause);

  // A world failure has been logged and stopped the server thread.
  bool serverErrorOccurred();
  bool shouldExpire();

  [[nodiscard]]
  std::expected<bool, StarException> spawnTargetValid(SpawnTarget const& spawnTarget);

  [[nodiscard]]
  std::expected<bool, StarException> addClient(ConnectionId clientId, SpawnTarget const& spawnTarget, bool isLocal, bool isAdmin = false, NetCompatibilityRules netRules = {}, ClientSubWorldId subWorldId = MainClientWorldId);
  // Returns final outgoing packets
  [[nodiscard]]
  std::expected<List<std::shared_ptr<Packet>>, StarException> removeClient(ConnectionId clientId);

  List<ConnectionId> clients() const;
  bool hasClient(ConnectionId clientId) const;
  bool noClients() const;
  ClientSubWorldId clientSubWorld(ConnectionId clientId) const;

  // Clients that have caused an error with incoming packets are removed from
  // the world and no further packets are handled from them.  They are still
  // added to this WorldServerThread, and must be removed and the final
  // outgoing packets should be sent to them.
  List<ConnectionId> erroredClients() const;

  void pushIncomingPackets(ConnectionId clientId, List<std::shared_ptr<Packet>> packets);
  List<std::shared_ptr<Packet>> pullOutgoingPackets(ConnectionId clientId);

  [[nodiscard]]
  std::expected<std::optional<Vec2F>, StarException> playerRevivePosition(ConnectionId clientId) const;

  // Worlds use this to notify the universe server that their celestial type should change
  [[nodiscard]]
  std::expected<std::optional<std::pair<String, String>>, StarException> pullNewPlanetType();

  // Executes an action under the world lock; returns its failure to the caller.
  [[nodiscard]]
  std::expected<void, StarException> executeAction(WorldServerAction action);

  // If a callback is set here, then this is called after every world update,
  // also in a thread safe context.
  void setUpdateAction(WorldServerAction updateAction);

  // 
  void passMessages(List<Message>&& messages);

  [[nodiscard]]
  std::expected<void, StarException> unloadAll(bool force = false);

  // Syncs all active sectors to disk and reads the full content of the world
  // into memory, useful for the ship.
  [[nodiscard]]
  std::expected<WorldChunks, StarException> readChunks();

protected:
  [[nodiscard]]
  std::expected<void, StarException> run() override;

private:
  WorldServerThread(std::shared_ptr<WorldServer> server, WorldId worldId, String worldName);
  [[nodiscard]]
  std::expected<void, StarException> recordError(StarException const& error) const;
  [[nodiscard]]
  std::expected<void, StarException> runLoop(std::stop_token stopToken);
  [[nodiscard]]
  std::expected<void, StarException> drainMessages();
  [[nodiscard]]
  std::expected<void, StarException> update(WorldServerFidelity fidelity);
  [[nodiscard]]
  std::expected<void, StarException> sync();

  mutable std::recursive_mutex m_mutex;

  HashSet<ConnectionId> m_clients;
  // A failed removal may be retried, but its incoming packets must not run again.
  HashSet<ConnectionId> m_removingClients;

  std::shared_ptr<WorldServer> m_worldServer;
  WorldId m_worldId;
  String m_worldName;
  WorldServerAction m_updateAction;

  mutable std::recursive_mutex m_queueMutex;
  Map<ConnectionId, List<std::shared_ptr<Packet>>> m_incomingPacketQueue;
  Map<ConnectionId, List<std::shared_ptr<Packet>>> m_outgoingPacketQueue;

  mutable std::recursive_mutex m_messageMutex;
  List<Message> m_messages;

  std::stop_source m_stopSource;
  std::shared_ptr<const std::atomic<bool>> m_pause;
  mutable std::atomic<bool> m_errorOccurred;
  mutable std::atomic<bool> m_shouldExpire;
};

}

export module star.world_server_thread;

export namespace Star {
  using ::Star::WorldChunks;
  using ::Star::WorldServerThread;
}
