import std;
import star.string;
import star.list;
import star.format;
import star.output_proxy;
import star.json;
import star.byte_array;
import star.rpc_promise;
import star.json_rpc;
import star.data_stream_devices;
import star.logging;

namespace Star {

JsonRpcInterface::~JsonRpcInterface() = default;

JsonRpc::JsonRpc(Logger& logger) : m_logger(logger), m_requestId(0) {}

std::expected<void, StarException> JsonRpc::registerHandler(String const& handler, JsonRpcRemoteFunction func) {
  if (!func)
    return std::unexpected(JsonRpcException::format("Empty handler '{}'", handler));
  if (m_handlers.contains(handler))
    return std::unexpected(JsonRpcException::format("Handler by that name already exists '{}'", handler));
  auto added = m_handlers.add(handler, std::move(func));
  if (!added)
    return std::unexpected(std::move(added.error()));
  return {};
}

std::expected<void, StarException> JsonRpc::registerHandlers(JsonRpcHandlers const& handlers) {
  for (auto const& pair : handlers) {
    if (auto status = registerHandler(pair.first, pair.second); !status)
      return std::unexpected(std::move(status.error()));
  }
  return {};
}

std::expected<void, StarException> JsonRpc::removeHandler(String const& handler) {
  if (!m_handlers.contains(handler))
    return std::unexpected(JsonRpcException::format("No such handler by the name '{}'", handler));
  m_handlers.remove(handler);
  return {};
}

void JsonRpc::clearHandlers() {
  m_handlers.clear();
}

std::expected<RpcPromise<Json>, StarException> JsonRpc::invokeRemote(String const& handler, Json const& arguments) {
  auto id = m_requestId;
  auto pair = RpcPromise<Json>::createPair();
  auto added = m_pendingResponse.add(id, pair.second);
  if (!added)
    return std::unexpected(std::move(added.error()));
  ++m_requestId;
  m_pending.push_back(JsonObject{{"command", "request"}, {"id", id}, {"handler", handler}, {"arguments", arguments}});
  return pair.first;
}

bool JsonRpc::sendPending() const {
  return !m_pending.empty();
}

std::expected<ByteArray, StarException> JsonRpc::send() {
  if (m_pending.empty())
    return ByteArray{};
  DataStreamBuffer buffer;
  if (auto status = buffer.writeContainer(m_pending); !status)
    return std::unexpected(std::move(status.error()));
  m_pending.clear();
  return buffer.takeData();
}

std::expected<void, StarException> JsonRpc::receive(ByteArray const& inbuffer) {
  if (inbuffer.empty())
    return {};
  DataStreamExternalBuffer buffer(inbuffer.span());
  List<Json> inbound;
  if (auto status = buffer.readContainer(inbound); !status)
    return std::unexpected(std::move(status.error()));

  for (auto const& request : inbound) {
    auto command = request.getString("command");
    if (!command)
      return std::unexpected(std::move(command.error()));
    if (*command == "request") {
      // A request needs an ID even when its handler fails, so validate that
      // envelope before entering the existing per-handler recovery boundary.
      auto id = request.get("id");
      if (!id)
        return std::unexpected(std::move(id.error()));
      auto invoke = [&]() -> std::expected<Json, StarException> {
        auto name = request.getString("handler");
        if (!name)
          return std::unexpected(std::move(name.error()));
        auto handler = m_handlers.find_ref(*name);
        if (!handler)
          return std::unexpected(JsonRpcException::format("Unknown handler '{}'", *name));
        auto arguments = request.get("arguments");
        if (!arguments)
          return std::unexpected(std::move(arguments.error()));
        return handler->get()(*arguments);
      };
      auto result = invoke();
      if (!result) {
        auto logged = m_logger.error("Error while handling variant rpc request handler call. {}", outputException(ExceptionDiagnostic(result.error()), false));
        m_pending.push_back(JsonObject{{"command", "fail"}, {"id", *id}});
        if (!logged)
          return std::unexpected(std::move(logged.error()));
      } else {
        m_pending.push_back(JsonObject{{"command", "response"}, {"id", *id}, {"result", std::move(*result)}});
      }
    } else if (*command == "response" || *command == "fail") {
      auto complete = [&]() -> std::expected<void, StarException> {
        auto id = request.getUInt("id");
        if (!id)
          return std::unexpected(std::move(id.error()));
        auto keeper = m_pendingResponse.extract_value(*id);
        if (!keeper)
          return std::unexpected(std::move(keeper.error()));
        if (*command == "fail")
          return keeper->fulfill(Json{});
        auto result = request.get("result");
        if (!result)
          return std::unexpected(std::move(result.error()));
        return keeper->fulfill(std::move(*result));
      };
      if (auto status = complete(); !status) {
        if (auto logged = m_logger.error("Error while handling variant rpc response handler call. {}", outputException(ExceptionDiagnostic(status.error()), true)); !logged)
          return std::unexpected(std::move(logged.error()));
      }
    }
  }
  return {};
}

}
