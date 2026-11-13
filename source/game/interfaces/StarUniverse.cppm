module;
import star.json;
import std;
import star.format;
import star.type_identity;
import star.string;
import star.rpc_promise;
import star.game_types;
import star.data_stream;

import star.uuid;

namespace Star {

class CelestialDatabase;

// Shared base class for UniverseClient and UniverseServer
class Universe {
public:
  virtual void const* query_type(TypeKey key) const noexcept { return query_type_of<Universe>(this, key); }
  virtual std::string_view dynamic_type_name() const noexcept { return Star::type_name<Universe>(); }

  struct Message {
    String message;
    JsonArray args;
    std::variant<std::pair<Uuid, ConnectionId>, RpcPromiseKeeper<Json>> keeper;
  };

  virtual ~Universe() = default;
  
  virtual std::shared_ptr<CelestialDatabase> celestialDatabase() = 0;
  
  [[nodiscard]] virtual std::expected<void, StarException> passMessage(Message&& message) = 0;
  // Sends a universe message to the target universe's main universe script context.
  [[nodiscard]] virtual std::expected<RpcPromise<Json>, StarException> sendUniverseMessage(ConnectionId const& connectionId, String const& message, JsonArray const& args = {}) = 0;

  [[nodiscard]] virtual std::expected<std::optional<ChainableJsonMessageResponse>, StarException> receiveMessage(String const& message, bool const& local, JsonArray const& args) = 0;
};

}

export module star.universe;

export namespace Star {
  using ::Star::Universe;
  using ::Star::CelestialDatabase;
}
