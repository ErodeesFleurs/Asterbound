module;

import std;
import star.format;

import star.byte_array;
import star.list;
import star.string;
import star.strong_typedef;
import star.rpc_promise;

import star.host_address;

namespace Star {

enum class P2PJoinRequestReply {
  No,
  Yes,
  Ignore,
};

// P2P networking is assumed to be guaranteed in order delivery of arbitrarily
// sized messages.  Neither the P2PSocket or the P2PNetworkingService are
// assumed to be thread safe interfaces, but access to independent P2PSockets
// from different threads or access to a P2PSocket and the P2PNetworkingService
// from different threads is assumed to be safe.
class P2PSocket {
public:
  virtual ~P2PSocket() = default;

  virtual bool isOpen() = 0;
  [[nodiscard]] virtual std::expected<bool, StarException> sendMessage(ByteArray const& message) = 0;
  [[nodiscard]] virtual std::expected<std::optional<ByteArray>, StarException> receiveMessage() = 0;
};

template <typename BaseType>
struct P2PNetworkingPeerIdWrapper : BaseType {
  using BaseType::BaseType;

  P2PNetworkingPeerIdWrapper() : BaseType() {}

  P2PNetworkingPeerIdWrapper(P2PNetworkingPeerIdWrapper const& nt) : BaseType(nt) {}

  P2PNetworkingPeerIdWrapper(P2PNetworkingPeerIdWrapper&& nt) : BaseType(std::move(nt)) {}

  explicit P2PNetworkingPeerIdWrapper(BaseType const& bt) : BaseType(bt) {}

  explicit P2PNetworkingPeerIdWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

  P2PNetworkingPeerIdWrapper& operator=(P2PNetworkingPeerIdWrapper const& rhs) {
    BaseType::operator=(rhs);
    return *this;
  }

  P2PNetworkingPeerIdWrapper& operator=(P2PNetworkingPeerIdWrapper&& rhs) {
    BaseType::operator=(std::move(rhs));
    return *this;
  }

  template <class Arg>
  P2PNetworkingPeerIdWrapper& operator=(Arg&& other) {
    static_assert(
      std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
        || std::is_same_v<P2PNetworkingPeerIdWrapper, std::decay_t<Arg>>,
      "P2PNetworkingPeerId can not implicitly be assigned from String-derived classes or strong String typedefs");

    BaseType::operator=(std::forward<Arg>(other));
    return *this;
  }
};
using P2PNetworkingPeerId = P2PNetworkingPeerIdWrapper<String>;

// API for platform specific peer to peer multiplayer services.
class P2PNetworkingService {
public:
  virtual ~P2PNetworkingService() = default;

  // P2P friends cannot join this player
  [[nodiscard]] virtual std::expected<void, StarException> setJoinUnavailable() = 0;
  // P2P friends can join this player's local game
  [[nodiscard]] virtual std::expected<void, StarException> setJoinLocal(std::uint32_t capacity) = 0;
  // P2P friends can join this player at the given remote server
  [[nodiscard]] virtual std::expected<void, StarException> setJoinRemote(HostAddressWithPort location) = 0;
  // Updates rich presence activity info
  [[nodiscard]] virtual std::expected<void, StarException> setActivityData(std::string_view title, std::string_view details, std::int64_t startTime, std::optional<std::pair<std::uint16_t, std::uint16_t>>) = 0;

  // If this player joins another peer's game using the P2P UI, this will return
  // a pending join location
  virtual std::variant<std::monostate, P2PNetworkingPeerId, HostAddressWithPort> pullPendingJoin() = 0;
  // This will return a username and a promise keeper to respond to the join request
  virtual std::optional<std::pair<String, RpcPromiseKeeper<P2PJoinRequestReply>>> pullJoinRequest() = 0;

  virtual void setAcceptingP2PConnections(bool acceptingP2PConnections) = 0;
  virtual List<std::unique_ptr<P2PSocket>> acceptP2PConnections() = 0;
  [[nodiscard]] virtual std::expected<void, StarException> update() = 0;

  [[nodiscard]] virtual std::expected<std::unique_ptr<P2PSocket>, StarException> connectToPeer(P2PNetworkingPeerId peerId) = 0;
};

};

export module star.p2p_networking_service;

export namespace Star {
  using ::Star::P2PSocket;
  using ::Star::P2PNetworkingService;
  using ::Star::P2PJoinRequestReply;
  using ::Star::P2PNetworkingPeerId;
  using ::Star::P2PNetworkingPeerIdWrapper;
}
