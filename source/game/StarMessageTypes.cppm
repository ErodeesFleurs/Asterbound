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

// Only promise-returning APIs need to box an immediate value. Packet replies
// can consume the same JSON directly without allocating promise state.
[[nodiscard]] inline std::expected<RpcPromise<Json>, StarException>
to_message_promise(MessageResponseResult response, std::string_view unhandled_reason) {
  if (!response)
    return std::unexpected(std::move(response.error()));
  if (!*response)
    return RpcPromise<Json>::make_failed(String(unhandled_reason));
  auto& value = **response;
  if (std::holds_alternative<Json>(value))
    return RpcPromise<Json>::make_fulfilled(std::get<Json>(std::move(value)));
  return std::get<RpcPromise<Json>>(std::move(value));
}
}

export module star.message_types;

export namespace Star {
  using ::Star::ServerCommandResult;
  using ::Star::ChainableJsonMessageResponse;
  using ::Star::MessageResponseResult;
  using ::Star::to_message_promise;
}
