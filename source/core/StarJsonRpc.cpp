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

JsonRpc::JsonRpc(Logger& logger) : m_logger(logger) {}

std::expected<void, StarException> JsonRpc::register_handler(String const& handler, JsonRpcRemoteFunction func) {
  if (!func)
    return std::unexpected(JsonRpcException::format("Empty handler '{}'", handler));
  if (!m_handlers.try_emplace(handler, std::move(func)).second)
    return std::unexpected(JsonRpcException::format("Handler by that name already exists '{}'", handler));
  return {};
}

std::expected<void, StarException> JsonRpc::register_handlers(JsonRpcHandlers const& handlers) {
  for (auto const& pair : handlers) {
    if (auto status = register_handler(pair.first, pair.second); !status)
      return std::unexpected(std::move(status.error()));
  }
  return {};
}

std::expected<void, StarException> JsonRpc::remove_handler(String const& handler) {
  if (!m_handlers.remove(handler))
    return std::unexpected(JsonRpcException::format("No such handler by the name '{}'", handler));
  return {};
}

void JsonRpc::clear_handlers() {
  m_handlers.clear();
}

std::expected<RpcPromise<Json>, StarException> JsonRpc::invoke_remote(String const& handler, Json const& arguments) {
  auto id = m_request_id;
  auto pair = RpcPromise<Json>::make_pair();
  auto added = m_pending_response.add(id, std::move(pair.second));
  if (!added)
    return std::unexpected(std::move(added.error()));
  ++m_request_id;
  m_pending.push_back(JsonObject{{"command", "request"}, {"id", id}, {"handler", handler}, {"arguments", arguments}});
  return std::move(pair.first);
}

bool JsonRpc::has_pending() const {
  return !m_pending.empty();
}

std::expected<ByteArray, StarException> JsonRpc::send() {
  if (m_pending.empty())
    return ByteArray{};
  DataStreamBuffer buffer;
  if (auto status = buffer.write_container(m_pending); !status)
    return std::unexpected(std::move(status.error()));
  m_pending.clear();
  return buffer.take_data();
}

std::expected<void, StarException> JsonRpc::receive(ByteArray const& inbuffer) {
  if (inbuffer.empty())
    return {};
  DataStreamReader buffer(inbuffer.span());
  List<Json> inbound;
  if (auto status = buffer.read_container(inbound); !status)
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
        auto handler = m_handlers.find(*name);
        if (handler == m_handlers.end())
          return std::unexpected(JsonRpcException::format("Unknown handler '{}'", *name));
        auto arguments = request.get("arguments");
        if (!arguments)
          return std::unexpected(std::move(arguments.error()));
        auto active = handler->second.target;
        return (*active)(*arguments);
      };
      auto result = invoke();
      if (!result) {
        auto logged = m_logger.error("Error while handling variant rpc request handler call. {}", Star::output_exception(ExceptionDiagnostic(result.error()), false));
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
        auto result = *command == "fail" ? std::expected<Json, StarException>(std::in_place) : request.get("result");
        if (!result)
          return std::unexpected(std::move(result.error()));
        auto keeper = m_pending_response.extract_value(*id);
        if (!keeper)
          return std::unexpected(std::move(keeper.error()));
        return keeper->fulfill(std::move(*result));
      };
      if (auto status = complete(); !status) {
        if (auto logged = m_logger.error("Error while handling variant rpc response handler call. {}", Star::output_exception(ExceptionDiagnostic(status.error()), true)); !logged)
          return std::unexpected(std::move(logged.error()));
      }
    }
  }
  return {};
}

}
