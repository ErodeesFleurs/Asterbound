import std;
import star.json;
import star.lua;
import star.lua_converters;
import star.thread;
import star.memory;
import star.string;
import star.map;
import star.format;
import star.list;
import star.logging;
import star.output_proxy;
import star.rpc_promise;
import star.configuration;
import star.worker_pool;
import star.lua_game_converters;
import star.root;
import star.http_client;
import star.lua_http_bindings;

namespace Star {

struct LuaHttpResponse {
  int statusCode = 0;
  String body;
};

struct LuaHttpProgress {
  std::weak_ptr<LuaHttpSession> session;
  std::uint64_t id;

  [[nodiscard]] std::expected<void, StarException> poll() const {
    if (auto owner = session.lock())
      return owner->poll();
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> cancel() const {
    if (auto owner = session.lock())
      return owner->cancel(id);
    return {};
  }
};

namespace {

[[nodiscard]] std::expected<String, StarException> extractDomain(String const& url) {
  auto scheme = url.find("://");
  if (!scheme)
    return std::unexpected(std::move(scheme.error()));
  if (*scheme == NPos)
    return url;
  auto start = *scheme + 3;
  auto path = url.find('/', start);
  if (!path)
    return std::unexpected(std::move(path.error()));
  auto port = url.find(':', start);
  if (!port)
    return std::unexpected(std::move(port.error()));
  std::size_t end = NPos;
  if (*path != NPos && *port != NPos)
    end = std::min(*path, *port);
  else if (*path != NPos)
    end = *path;
  else if (*port != NPos)
    end = *port;
  return end == NPos ? url.substr(start) : url.substr(start, end - start);
}

}

struct LuaHttpSession::Impl {
  struct PendingHttpRequest {
    std::uint64_t id;
    HttpRequest httpRequest;
    RpcPromiseKeeper<LuaHttpResponse> rpcKeeper;
    String domain;
    RpcPromise<LuaHttpResponse> scopedPromise;
    std::stop_token lifetime;
  };
  struct AsyncHttpRequest {
    WorkerPoolPromise<HttpResponse> workerPromise;
    RpcPromiseKeeper<LuaHttpResponse> rpcKeeper;
    RpcPromise<LuaHttpResponse> scopedPromise;
  };

  Impl(Root& root, std::unique_ptr<HttpClient> client) : root(root), httpClient(std::move(client)) {}

  [[nodiscard]] std::expected<bool, StarException> isTrustedDomain(String const& domain) const {
    if (stopped)
      return false;
    auto config = root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto sites = (*config)->getPath("safe.luaHttp.trustedSites");
    if (!sites)
      return std::unexpected(std::move(sites.error()));
    auto array = sites->optArray();
    if (!array)
      return std::unexpected(std::move(array.error()));
    if (*array) {
      for (auto const& site : **array) {
        auto value = site.toString();
        if (!value)
          return std::unexpected(std::move(value.error()));
        if (*value == domain)
          return true;
      }
    }
    return false;
  }

  // Transport and worker failures are ordinary failed Lua promises, as before.
  [[nodiscard]] std::expected<void, StarException> executeHttpRequest(PendingHttpRequest& request) {
    // Trust replies may arrive before the eager scoped callback is installed.
    // Check the registered token while still holding the HTTP state mutex.
    if (request.lifetime.stop_requested()) {
      if (auto cancelled = request.rpcKeeper.cancel("Lua session stopped"); !cancelled)
        return std::unexpected(std::move(cancelled.error()));
      return {};
    }
    auto promise = httpClient->requestAsync(request.httpRequest);
    auto added = asyncRequests.add(
      request.id,
      {.workerPromise = std::move(promise), .rpcKeeper = request.rpcKeeper, .scopedPromise = request.scopedPromise});
    if (!added) {
      if (auto failed = request.rpcKeeper.fail(Star::format("HTTP request failed: {}", added.error().what())); !failed)
        return std::unexpected(failed.error().with_related(added.error(), "Original HTTP submission failure"));
    }
    return {};
  }

  [[nodiscard]] std::expected<RpcPromise<LuaHttpResponse>, StarException>
  request(HttpRequest request, std::stop_token lifetime, std::weak_ptr<LuaHttpSession> owner) {
    auto [promise, keeper] = RpcPromise<LuaHttpResponse>::createPair();
    auto domain = extractDomain(request.url);
    if (!domain)
      return std::unexpected(std::move(domain.error()));
    std::unique_lock callbackLock(callbackMutex, std::defer_lock);
    lockWithDiagnostics(callbackLock);
    std::uint64_t id;
    {
      std::lock_guard lock(mutex);
      if (stopped || lifetime.stop_requested()) {
        if (auto cancelled =
              keeper.cancel(String(std::string_view(stopped ? "HTTP session stopped" : "Lua session stopped")));
            !cancelled)
          return std::unexpected(std::move(cancelled.error()));
        return promise;
      }
      id = nextRequestId++;
      // Register the native source before attaching the eager stop callback.
      pendingRequests.push_back({.id = id,
                                 .httpRequest = std::move(request),
                                 .rpcKeeper = keeper,
                                 .domain = *domain,
                                 .scopedPromise = {},
                                 .lifetime = lifetime});
    }
    LuaHttpProgress progress{.session = std::move(owner), .id = id};
    auto scoped = promise.withCancellation(lifetime, "Lua session stopped", progress);
    if (!scoped) {
      if (auto cancelled = progress.cancel(); !cancelled)
        return std::unexpected(cancelled.error().with_related(scoped.error(), "Original HTTP scope failure"));
      return std::unexpected(std::move(scoped.error()));
    }
    LuaBindings::HttpTrustRequestCallback callback;
    std::optional<PendingHttpRequest> retired;
    std::optional<StarException> preparationError;
    {
      std::lock_guard lock(mutex);
      // A concurrent reply may already have promoted the registered source.
      auto active = asyncRequests.find(id);
      if (active != asyncRequests.end())
        active->second.scopedPromise = *scoped;
      for (auto it = pendingRequests.begin(); it != pendingRequests.end(); ++it) {
        if (it->id != id)
          continue;
        it->scopedPromise = *scoped;
        auto trusted = isTrustedDomain(*domain);
        if (!trusted) {
          preparationError = std::move(trusted.error());
          retired.emplace(std::move(*it));
          pendingRequests.erase(it);
          break;
        }
        if (*trusted) {
          auto executed = executeHttpRequest(*it);
          retired.emplace(std::move(*it));
          pendingRequests.erase(it);
          if (!executed)
            return std::unexpected(std::move(executed.error()));
        } else {
          callback = trustRequestCallback;
          if (!callback) {
            if (auto logged = root.logger().warn(
                  "HTTP request to untrusted domain '{}' but no trust dialog handler available", *domain);
                !logged) {
              preparationError = std::move(logged.error());
              retired.emplace(std::move(*it));
              pendingRequests.erase(it);
            }
          }
        }
        break;
      }
    }
    if (preparationError) {
      if (auto failed = retired->rpcKeeper.fail(Star::format("HTTP request failed: {}", preparationError->what())); !failed)
        return std::unexpected(std::move(failed.error()));
      return std::unexpected(std::move(*preparationError));
    }
    if (callback) {
      auto notified = callback(*domain);
      if (!notified) {
        // A synchronous reply or cancellation may already have removed it.
        {
          std::lock_guard lock(mutex);
          for (auto it = pendingRequests.begin(); it != pendingRequests.end(); ++it) {
            if (it->id == id) {
              retired.emplace(std::move(*it));
              pendingRequests.erase(it);
              break;
            }
          }
        }
        if (retired) {
          if (auto failed = retired->rpcKeeper.fail(Star::format("HTTP request failed: {}", notified.error().what())); !failed)
            return std::unexpected(failed.error().with_related(notified.error(), "Original HTTP trust callback failure"));
        }
        return std::unexpected(std::move(notified.error()));
      }
    }
    return std::move(*scoped);
  }

  Root& root;
  std::unique_ptr<HttpClient> httpClient;
  std::recursive_mutex callbackMutex;
  LuaBindings::HttpTrustRequestCallback trustRequestCallback;
  std::mutex mutex;
  List<PendingHttpRequest> pendingRequests;
  HashMap<std::uint64_t, AsyncHttpRequest> asyncRequests;
  std::uint64_t nextRequestId = 1;
  bool stopped = false;
};

LuaHttpSession::LuaHttpSession(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

std::expected<std::shared_ptr<LuaHttpSession>, StarException> LuaHttpSession::create(Root& root) {
  auto client = HttpClient::create();
  if (!client)
    return std::unexpected(std::move(client.error()));
  return std::shared_ptr<LuaHttpSession>(new LuaHttpSession(std::make_unique<Impl>(root, std::move(*client))));
}

LuaHttpSession::~LuaHttpSession() {
  if (auto result = stop(); !result)
    EmergencyLog::error("Error stopping Lua HTTP session: {}", Star::output_exception(result.error(), true));
}

void LuaHttpSession::setHttpTrustRequestCallback(LuaBindings::HttpTrustRequestCallback callback) {
  std::unique_lock callbackLock(m_impl->callbackMutex, std::defer_lock);
  lockWithDiagnostics(callbackLock);
  std::lock_guard lock(m_impl->mutex);
  if (!m_impl->stopped)
    m_impl->trustRequestCallback = std::move(callback);
}

void LuaHttpSession::clearHttpTrustRequestCallback() {
  std::unique_lock callbackLock(m_impl->callbackMutex, std::defer_lock);
  lockWithDiagnostics(callbackLock);
  m_impl->trustRequestCallback = {};
}

std::expected<void, StarException> LuaHttpSession::handleHttpTrustReply(String const& domain, bool allowed) {
  // Retain callback-owning views until after releasing the state mutex.
  List<Impl::PendingHttpRequest> retired;
  std::optional<StarException> error;
  {
    std::lock_guard lock(m_impl->mutex);
    if (m_impl->stopped)
      return {};
    auto& pending = m_impl->pendingRequests;
    for (auto it = pending.begin(); it != pending.end();) {
      if (it->domain != domain) {
        ++it;
        continue;
      }
      auto result = allowed ? m_impl->executeHttpRequest(*it)
                            : it->rpcKeeper.fail(Star::format("HTTP request to domain '{}' denied by user", domain));
      if (!result) {
        if (!error)
          error = std::move(result.error());
        else
          EmergencyLog::error("Error settling HTTP trust reply: {}", Star::output_exception(result.error(), true));
      }
      retired.push_back(std::move(*it));
      it = pending.erase(it);
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> LuaHttpSession::poll() {
  List<Impl::AsyncHttpRequest> retired;
  std::optional<StarException> error;
  {
    std::lock_guard lock(m_impl->mutex);
    auto& requests = m_impl->asyncRequests;
    for (auto it = requests.begin(); it != requests.end();) {
      auto& request = it->second;
      if (!request.workerPromise.done()) {
        ++it;
        continue;
      }
      auto response = request.workerPromise.get();
      std::expected<void, StarException> settled;
      if (!response)
        settled = request.rpcKeeper.fail(Star::format("HTTP request exception: {}", response.error().what()));
      else if (!response->get().error.empty())
        settled = request.rpcKeeper.fail(Star::format("HTTP request failed: {}", response->get().error));
      else
        settled = request.rpcKeeper.fulfill(
          {.statusCode = response->get().statusCode, .body = std::move(response->get().body)});
      if (!settled) {
        if (!error)
          error = std::move(settled.error());
        else
          EmergencyLog::error("Error settling HTTP response: {}", Star::output_exception(settled.error(), true));
      }
      retired.push_back(std::move(request));
      it = requests.erase(it);
    }
  }
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

std::expected<void, StarException> LuaHttpSession::cancel(std::uint64_t id) {
  std::optional<Impl::PendingHttpRequest> pending;
  std::optional<Impl::AsyncHttpRequest> active;
  {
    std::lock_guard lock(m_impl->mutex);
    for (auto it = m_impl->pendingRequests.begin(); it != m_impl->pendingRequests.end(); ++it) {
      if (it->id == id) {
        pending.emplace(std::move(*it));
        m_impl->pendingRequests.erase(it);
        break;
      }
    }
    auto it = m_impl->asyncRequests.find(id);
    if (it != m_impl->asyncRequests.end()) {
      active.emplace(std::move(it->second));
      m_impl->asyncRequests.erase(it);
    }
  }
  if (pending) {
    if (auto cancelled = pending->rpcKeeper.cancel("HTTP request cancelled"); !cancelled)
      return std::unexpected(std::move(cancelled.error()));
  }
  if (active) {
    if (auto cancelled = active->rpcKeeper.cancel("HTTP request cancelled"); !cancelled)
      return std::unexpected(std::move(cancelled.error()));
  }
  return {};
}

std::expected<void, StarException> LuaHttpSession::stop() {
  std::unique_lock callbackLock(m_impl->callbackMutex, std::defer_lock);
  lockWithDiagnostics(callbackLock);
  std::optional<StarException> error;
  auto remember = [&](auto result) {
    if (!result) {
      if (!error)
        error = std::move(result.error());
      else
        EmergencyLog::error("Error stopping Lua HTTP request: {}", Star::output_exception(result.error(), true));
    }
  };
  List<Impl::PendingHttpRequest> pending;
  HashMap<std::uint64_t, Impl::AsyncHttpRequest> active;
  {
    std::lock_guard lock(m_impl->mutex);
    if (!m_impl->stopped) {
      m_impl->stopped = true;
      m_impl->trustRequestCallback = {};
      pending = std::move(m_impl->pendingRequests);
      active = std::move(m_impl->asyncRequests);
    }
  }
  for (auto& request : pending)
    remember(request.rpcKeeper.cancel("HTTP session stopped"));
  for (auto&& [id, request] : active)
    remember(request.rpcKeeper.cancel("HTTP session stopped"));
  remember(m_impl->httpClient->stop());
  if (error)
    return std::unexpected(std::move(*error));
  return {};
}

template <>
struct LuaConverter<LuaHttpResponse> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, LuaHttpResponse const& response) {
    auto table = stack.createTable(0, 2);
    if (!table)
      return std::unexpected(std::move(table.error()));
    if (auto result = table->set("status", response.statusCode); !result)
      return std::unexpected(std::move(result.error()));
    if (auto result = table->set("body", response.body); !result)
      return std::unexpected(std::move(result.error()));
    return {};
  }
  [[nodiscard]] static std::expected<std::optional<LuaHttpResponse>, StarException> to(LuaStackValue value) {
    if (value.is<LuaTable>()) {
      auto table = value;
      auto status = table.get<std::optional<int>>("status");
      if (!status)
        return std::unexpected(std::move(status.error()));
      LuaHttpResponse response;
      response.statusCode = status->value_or(0);
      auto hasBody = table.contains("body");
      if (!hasBody)
        return std::unexpected(std::move(hasBody.error()));
      if (*hasBody) {
        auto body = table.get<String>("body");
        if (!body)
          return std::unexpected(std::move(body.error()));
        response.body = std::move(*body);
      }
      return std::optional<LuaHttpResponse>(std::move(response));
    }
    return std::optional<LuaHttpResponse>{};
  }
};


std::expected<LuaCallbacks, StarException> LuaBindings::makeHttpCallbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled) {
  if (!session)
    return std::unexpected(LuaException("HTTP callbacks require an owning LuaHttpSession"));
  LuaCallbacks callbacks;
  std::weak_ptr<LuaHttpSession> weakSession = session;
  if (auto result = callbacks.registerCallback("available", [enabled]() { return enabled; }); !result)
    return std::unexpected(std::move(result.error()));
  auto requestForMethod = [weakSession,
                           enabled](LuaEngine& engine, String const& method, String const& url,
                                    std::optional<LuaTable> const& options) -> RpcPromise<LuaHttpResponse> {
    if (!enabled)
      return RpcPromise<LuaHttpResponse>::createFailed("luaHttp disabled by configuration");
    auto owner = weakSession.lock();
    if (!owner) {
      auto [promise, keeper] = RpcPromise<LuaHttpResponse>::createPair();
      if (auto cancelled = keeper.cancel("HTTP session stopped"); !cancelled)
        return RpcPromise<LuaHttpResponse>::createFailed(Star::format("HTTP request failed: {}", cancelled.error().what()));
      return promise;
    }
    auto prepared = [&]() -> std::expected<HttpRequest, StarException> {
      HttpRequest request;
      request.method = method;
      request.url = url;
      if (options) {
        auto headers = options->get("headers");
        if (!headers)
          return std::unexpected(std::move(headers.error()));
        if (std::holds_alternative<LuaTable>(*headers)) {
          auto const& table = std::get<LuaTable>(*headers);
          if (auto iterated = table.iterate([&](LuaValue key, LuaValue value) -> std::expected<void, StarException> {
            if (std::holds_alternative<LuaString>(key)) {
              auto const& name = std::get<LuaString>(key);
              if (std::holds_alternative<LuaString>(value))
                request.headers[name.toString()] = std::get<LuaString>(value).toString();
              else {
                auto converted = engine.luaTo<String>(value);
                if (!converted)
                  return std::unexpected(std::move(converted.error()));
                request.headers[name.toString()] = std::move(*converted);
              }
            }
            return {};
          }); !iterated)
            return std::unexpected(std::move(iterated.error()));
        }
        auto body = options->get("body");
        if (!body)
          return std::unexpected(std::move(body.error()));
        if (std::holds_alternative<LuaString>(*body)) {
          request.body = std::get<LuaString>(*body).toString();
        } else if (!std::holds_alternative<LuaNilType>(*body)) {
          auto json = engine.luaTo<Json>(*body);
          if (!json)
            return std::unexpected(std::move(json.error()));
          auto printed = json->repr();
          if (!printed)
            return std::unexpected(std::move(printed.error()));
          request.body = std::move(*printed);
        }
        auto timeout = options->get<std::optional<int>>("timeout");
        if (!timeout)
          return std::unexpected(std::move(timeout.error()));
        if (*timeout)
          request.timeout = **timeout;
      }
      return request;
    }();
    if (!prepared)
      return RpcPromise<LuaHttpResponse>::createFailed(Star::format("HTTP request failed: {}", prepared.error().what()));
    auto promise = owner->m_impl->request(std::move(*prepared), engine.promiseLifetime(), weakSession);
    if (!promise)
      return RpcPromise<LuaHttpResponse>::createFailed(Star::format("HTTP request failed: {}", promise.error().what()));
    return std::move(*promise);
  };
  if (auto result = callbacks.registerCallback("createRequest", [requestForMethod](LuaEngine& engine, String const& method, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, method, url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("get", [requestForMethod](LuaEngine& engine, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, "GET", url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("post", [requestForMethod](LuaEngine& engine, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, "POST", url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("put", [requestForMethod](LuaEngine& engine, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, "PUT", url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("delete", [requestForMethod](LuaEngine& engine, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, "DELETE", url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("patch", [requestForMethod](LuaEngine& engine, String const& url, std::optional<LuaTable> const& options) {
    return requestForMethod(engine, "PATCH", url, options);
  }); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = callbacks.registerCallback("isTrusted", [weakSession](String const& domain) -> std::expected<bool, StarException> {
    if (auto owner = weakSession.lock()) {
      std::lock_guard lock(owner->m_impl->mutex);
      return owner->m_impl->isTrustedDomain(domain);
    }
    return false;
  }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

}
