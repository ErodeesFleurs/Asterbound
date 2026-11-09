module;
import std;
import star.string;
import star.list;
import star.map;
import star.format;
import star.json;
import star.byte_array;
import star.rpc_promise;
import star.logging;

namespace Star {

struct JsonRpcExceptionTag {
  static constexpr std::string_view name() { return "JsonRpcException"; }
};
using JsonRpcException = StarError<JsonRpcExceptionTag, StarException>;

using JsonRpcRemoteFunction = std::function<std::expected<Json, StarException>(Json const&)>;

using JsonRpcHandlers = StringMap<JsonRpcRemoteFunction>;

// Simple interface to just the method invocation part of JsonRpc.
class JsonRpcInterface {
public:
  virtual ~JsonRpcInterface();
  [[nodiscard]] virtual std::expected<RpcPromise<Json>, StarException> invokeRemote(String const& handler, Json const& arguments) = 0;
};

// Simple class to handle remote methods based on Json types.  Does not
// handle any of the network details, simply turns rpc calls into ByteArray
// messages to be sent and received.
class JsonRpc : public JsonRpcInterface {
public:
  explicit JsonRpc(Logger& logger);

  [[nodiscard]] std::expected<void, StarException> registerHandler(String const& handler, JsonRpcRemoteFunction func);
  [[nodiscard]] std::expected<void, StarException> registerHandlers(JsonRpcHandlers const& handlers);

  [[nodiscard]] std::expected<void, StarException> removeHandler(String const& handler);
  void clearHandlers();

  [[nodiscard]] std::expected<RpcPromise<Json>, StarException> invokeRemote(String const& handler, Json const& arguments) override;

  bool sendPending() const;
  [[nodiscard]] std::expected<ByteArray, StarException> send();
  [[nodiscard]] std::expected<void, StarException> receive(ByteArray const& inbuffer);

private:
  Logger& m_logger;
  // Handlers may register more methods while their own callable is borrowed.
  StableStringMap<JsonRpcRemoteFunction> m_handlers;
  Map<std::uint64_t, RpcPromiseKeeper<Json>> m_pendingResponse;
  List<Json> m_pending;
  std::uint64_t m_requestId;
};

}

export module star.json_rpc;

export namespace Star {
using ::Star::JsonRpcInterface;
using ::Star::JsonRpc;
using ::Star::JsonRpcExceptionTag;
using ::Star::JsonRpcException;
using ::Star::JsonRpcRemoteFunction;
using ::Star::JsonRpcHandlers;
}
