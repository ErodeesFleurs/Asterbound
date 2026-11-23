module;
import std;
import star.format;
import star.json;
import star.rpc_promise;
import star.string;

namespace Star {
using ServerCommandResult = std::variant<String, RpcPromise<String>>;
using ChainableJsonMessageResponse = std::variant<Json, RpcPromise<Json>>;
using MessageResponseResult = std::expected<std::optional<ChainableJsonMessageResponse>, StarException>;
}

export module star.message_types;

export namespace Star {
  using ::Star::ServerCommandResult;
  using ::Star::ChainableJsonMessageResponse;
  using ::Star::MessageResponseResult;
}
