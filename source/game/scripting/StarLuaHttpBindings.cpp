import std;
import star.json;
import star.lua;
import star.lua_converters;
import star.string;
import star.map;
import star.format;
import star.logging;
import star.output_proxy;
import star.rpc_promise;
import star.configuration;
import star.worker_pool;
import star.root;
import star.http_client;
import star.lua_http_bindings;

namespace Star {

struct LuaHttpProgress {
  std::weak_ptr<LuaHttpSession> session;
  std::uint64_t id;

  [[nodiscard]] std::expected<void, StarException> poll() const {
    if (auto owner = session.lock())
      return owner->poll(id);
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> cancel() const {
    if (auto owner = session.lock())
      return owner->cancel(id);
    return {};
  }
};

namespace {

using HttpPromise = RpcPromise<HttpResponse>;
using HttpKeeper = RpcPromiseKeeper<HttpResponse>;

void collect_error(std::expected<void, StarException>& result, StarException error) {
  if (result)
    result = std::unexpected(std::move(error));
  else
    result = std::unexpected(result.error().with_related(error, "Additional HTTP session failure"));
}

String request_error(StarException const& error) {
  return Star::format("HTTP request failed: {}", error.what());
}

HttpPromise cancelled_promise(String reason) {
  auto [promise, keeper] = HttpPromise::make_pair();
  if (auto cancelled = keeper.cancel(std::move(reason)); !cancelled)
    return HttpPromise::make_failed(request_error(cancelled.error()));
  return promise;
}

}

struct LuaHttpSession::Impl {
  struct Pending {
    std::uint64_t id;
    HttpRequest request;
    HttpKeeper keeper;
    HttpPromise scoped;
    std::stop_token lifetime;
  };

  struct Group {
    std::shared_ptr<HttpApproval const> approval;
    std::vector<Pending> requests;
  };

  struct Active {
    WorkerPoolPromise<HttpResponse> worker;
    HttpKeeper keeper;
    HttpPromise scoped;

    // Only called after done(); get() cannot wait for network I/O here.
    [[nodiscard]] std::expected<void, StarException> settle() {
      auto response = worker.get();
      if (!response)
        return keeper.fail(request_error(response.error()));
      return keeper.fulfill(std::move(response->get()));
    }
  };

  explicit Impl(Root& root) : root(root) {}

  // Configuration can change while this session lives. Cache normalization
  // only for the exact immutable array snapshot read on this call.
  [[nodiscard]] std::expected<bool, StarException> trusted(String const& domain, bool remember = false) {
    static String const path = "safe.luaHttp.trustedSites";
    if (stopped)
      return false;
    auto config = root.configuration();
    if (!config)
      return std::unexpected(std::move(config.error()));
    auto sites = (*config)->get_path(path);
    if (!sites)
      return std::unexpected(std::move(sites.error()));
    std::shared_ptr<JsonArray const> snapshot;
    if (!sites->is_null()) {
      auto array = sites->array_ptr();
      if (!array)
        return std::unexpected(std::move(array.error()));
      snapshot = std::move(*array);
    }
    if (snapshot != trusted_snapshot) {
      StringSet hosts;
      if (snapshot) {
        for (auto const& entry : *snapshot) {
          auto value = entry.string_ptr();
          if (!value)
            return std::unexpected(std::move(value.error()));
          if (auto host = normalize_http_host(**value))
            hosts.insert(std::move(*host));
        }
      }
      trusted_hosts = std::move(hosts);
      trusted_snapshot = snapshot;
    }
    if (trusted_hosts.contains(domain))
      return true;
    if (!remember)
      return false;
    auto updated = snapshot ? *snapshot : JsonArray{};
    updated.push_back(domain);
    auto saved = (*config)->set_path(path, Json(std::move(updated)));
    if (!saved)
      return std::unexpected(std::move(saved.error()));
    return true;
  }

  // Requires mutex. Submission and the stopped transition are mutually
  // exclusive. Scoped registration stays alive even if Lua drops its handle.
  [[nodiscard]] std::expected<void, StarException> submit(Pending& pending) {
    if (pending.lifetime.stop_requested())
      return pending.keeper.cancel("Lua session stopped").transform([](bool) {});
    auto worker = client.request(std::move(pending.request));
    if (!worker)
      return pending.keeper.fail(request_error(worker.error()));
    active.try_emplace(pending.id, std::move(*worker), std::move(pending.keeper), std::move(pending.scoped));
    return {};
  }

  [[nodiscard]] std::expected<HttpPromise, StarException>
  request(HttpRequest request, String domain, std::stop_token lifetime, std::weak_ptr<LuaHttpSession> owner) {
    std::uint64_t id;
    {
      std::lock_guard lock(mutex);
      if (stopped)
        return cancelled_promise("HTTP session stopped");
      if (lifetime.stop_requested())
        return cancelled_promise("Lua session stopped");
      if (next_id == 0)
        return std::unexpected(StarException("HTTP request IDs exhausted"));
      id = next_id++;
    }
    auto [promise, keeper] = HttpPromise::make_pair();
    // Installing the hook may synchronously invoke cancel(id). Nothing is
    // visible yet; the token is checked under the publication lock below.
    auto scoped = promise.with_cancellation(lifetime, "Lua session stopped", LuaHttpProgress{std::move(owner), id});
    if (!scoped)
      return std::unexpected(std::move(scoped.error()));
    Pending pending{id, std::move(request), std::move(keeper), *scoped, lifetime};
    std::lock_guard lock(mutex);
    if (stopped || lifetime.stop_requested()) {
      if (auto cancelled = pending.keeper.cancel(stopped ? String("HTTP session stopped") : String("Lua session stopped"));
          !cancelled)
        return std::unexpected(std::move(cancelled.error()));
    } else {
      auto allowed = trusted(domain);
      if (!allowed)
        return std::unexpected(std::move(allowed.error()));
      if (*allowed) {
        if (auto submitted = submit(pending); !submitted)
          return std::unexpected(std::move(submitted.error()));
      } else if (!approval_available) {
        if (auto failed = pending.keeper.fail(Star::format("HTTP approval unavailable for domain '{}'", domain)); !failed)
          return std::unexpected(std::move(failed.error()));
      } else {
        auto group = std::ranges::find_if(groups, [&](Group const& g) { return g.approval->domain == domain; });
        if (group == groups.end()) {
          auto approval = std::make_shared<HttpApproval const>(id, std::move(domain));
          group = groups.emplace(group, std::move(approval), std::vector<Pending>{});
        }
        group->requests.push_back(std::move(pending));
      }
    }
    return std::move(*scoped);
  }

  Root& root;
  HttpClient client;
  std::mutex mutex;
  std::list<Group> groups;
  HashMap<std::uint64_t, Active> active;
  std::shared_ptr<JsonArray const> trusted_snapshot;
  StringSet trusted_hosts;
  std::uint64_t next_id = 1;
  bool approval_available = false;
  bool stopped = false;
};

LuaHttpSession::LuaHttpSession(Root& root) : m_impl(std::make_unique<Impl>(root)) {}

LuaHttpSession::~LuaHttpSession() {
  if (auto result = stop(); !result)
    EmergencyLog::error("Error stopping Lua HTTP session: {}", Star::output_exception(result.error(), true));
}

std::expected<void, StarException> LuaHttpSession::set_approval_available(bool available) {
  std::list<Impl::Group> retired;
  std::expected<void, StarException> result;
  std::lock_guard lock(m_impl->mutex);
  if (m_impl->stopped)
    return {};
  m_impl->approval_available = available;
  if (!available) {
    retired.swap(m_impl->groups);
    for (auto& group : retired) {
      auto reason = Star::format("HTTP approval unavailable for domain '{}'", group.approval->domain);
      for (auto& request : group.requests) {
        if (auto failed = request.keeper.fail(reason); !failed)
          collect_error(result, std::move(failed.error()));
      }
    }
  }
  return result;
}

std::shared_ptr<HttpApproval const> LuaHttpSession::approval() const {
  std::lock_guard lock(m_impl->mutex);
  return m_impl->groups.empty() ? std::shared_ptr<HttpApproval const>{} : m_impl->groups.front().approval;
}

std::expected<void, StarException> LuaHttpSession::resolve_approval(std::uint64_t id, bool allow, bool remember) {
  std::list<Impl::Group> retired;
  std::expected<void, StarException> result;
  std::lock_guard lock(m_impl->mutex);
  auto& groups = m_impl->groups;
  if (groups.empty() || groups.front().approval->id != id)
    return {};
  retired.splice(retired.end(), groups, groups.begin());
  auto& group = retired.front();
  auto const& domain = group.approval->domain;
  std::optional<String> failure;
  // A stop callback may still be waiting for this lock. Do not let its
  // obsolete dialog persist a decision if no live request remains.
  if (!allow)
    failure = Star::format("HTTP request to domain '{}' denied by user", domain);
  else if (remember && std::ranges::any_of(group.requests, [](Impl::Pending const& request) {
        return !request.lifetime.stop_requested();
      })) {
    if (auto saved = m_impl->trusted(domain, true); !saved) {
      failure = request_error(saved.error());
      result = std::unexpected(std::move(saved.error()));
    }
  }
  for (auto& request : group.requests) {
    std::expected<void, StarException> settled;
    if (!failure)
      settled = m_impl->submit(request);
    else if (request.lifetime.stop_requested())
      settled = request.keeper.cancel("Lua session stopped").transform([](bool) {});
    else
      settled = request.keeper.fail(*failure);
    if (!settled)
      collect_error(result, std::move(settled.error()));
  }
  return result;
}

std::expected<void, StarException> LuaHttpSession::poll(std::uint64_t id) {
  std::optional<Impl::Active> retired;
  std::lock_guard lock(m_impl->mutex);
  auto& active = m_impl->active;
  auto it = active.find(id);
  if (it == active.end() || !it->second.worker.done())
    return {};
  retired.emplace(std::move(it->second));
  active.erase(it);
  return retired->settle();
}

std::expected<void, StarException> LuaHttpSession::poll() {
  // Retire callback owners after unlocking: unregistering can wait for a
  // concurrent stop callback that needs the session mutex.
  std::vector<Impl::Active> retired;
  std::expected<void, StarException> result;
  std::lock_guard lock(m_impl->mutex);
  auto& active = m_impl->active;
  if (active.empty())
    return {};
  for (auto it = active.begin(); it != active.end();) {
    if (!it->second.worker.done()) {
      ++it;
      continue;
    }
    auto& ready = retired.emplace_back(std::move(it->second));
    it = active.erase(it);
    if (auto settled = ready.settle(); !settled)
      collect_error(result, std::move(settled.error()));
  }
  return result;
}

std::expected<void, StarException> LuaHttpSession::cancel(std::uint64_t id) {
  std::optional<Impl::Active> active;
  std::optional<Impl::Pending> pending;
  std::lock_guard lock(m_impl->mutex);
  auto it = m_impl->active.find(id);
  if (it != m_impl->active.end()) {
    active.emplace(std::move(it->second));
    m_impl->active.erase(it);
    return active->keeper.cancel("HTTP request cancelled").transform([](bool) {});
  }
  for (auto group = m_impl->groups.begin(); group != m_impl->groups.end(); ++group) {
    auto& requests = group->requests;
    auto item = std::ranges::find(requests, id, &Impl::Pending::id);
    if (item != requests.end()) {
      pending.emplace(std::move(*item));
      requests.erase(item);
      if (requests.empty())
        m_impl->groups.erase(group);
      return pending->keeper.cancel("HTTP request cancelled").transform([](bool) {});
    }
  }
  return {};
}

std::expected<void, StarException> LuaHttpSession::stop() {
  std::list<Impl::Group> pending;
  HashMap<std::uint64_t, Impl::Active> active;
  std::expected<void, StarException> result;
  {
    std::lock_guard lock(m_impl->mutex);
    m_impl->stopped = true;
    pending.swap(m_impl->groups);
    active = std::move(m_impl->active);
    for (auto& group : pending) {
      for (auto& request : group.requests) {
        if (auto cancelled = request.keeper.cancel("HTTP session stopped"); !cancelled)
          collect_error(result, std::move(cancelled.error()));
      }
    }
    for (auto&& [id, request] : active) {
      if (auto cancelled = request.keeper.cancel("HTTP session stopped"); !cancelled)
        collect_error(result, std::move(cancelled.error()));
    }
  }
  if (auto stopped = m_impl->client.stop(); !stopped)
    collect_error(result, std::move(stopped.error()));
  return result;
}

template <>
struct LuaConverter<HttpResponse> {
  [[nodiscard]] static std::expected<void, StarException> from(LuaStack stack, HttpResponse const& response) {
    auto table = stack.create_table(0, 2);
    if (!table)
      return std::unexpected(std::move(table.error()));
    if (auto result = table->set("status", response.status_code); !result)
      return std::unexpected(std::move(result.error()));
    return table->set("body", response.body);
  }
};

namespace {

[[nodiscard]] std::expected<void, StarException> apply_options(HttpRequest& request, LuaEngine& engine, LuaTable const& options) {
  // The general Lua integer converter can narrow an integer by wrapping.
  // Check the original width before validating the transport's milliseconds.
  auto timeout = options.get<std::optional<std::int64_t>>("timeout");
  if (!timeout)
    return std::unexpected(std::move(timeout.error()));
  if (*timeout) {
    if (!std::in_range<int>(**timeout))
      return std::unexpected(StarException("HTTP request timeout is outside the supported range"));
    request.timeout = static_cast<int>(**timeout);
    if (auto valid = validate_http_timeout(request.timeout); !valid)
      return std::unexpected(std::move(valid.error()));
  }
  auto headers = options.get("headers");
  if (!headers)
    return std::unexpected(std::move(headers.error()));
  if (std::holds_alternative<LuaTable>(*headers)) {
    auto parsed = std::get<LuaTable>(*headers).iterate([&](LuaValue key, LuaValue value) -> std::expected<void, StarException> {
      if (std::holds_alternative<LuaString>(key)) {
        auto name = std::get<LuaString>(key).to_string();
        if (std::holds_alternative<LuaString>(value))
          request.headers[std::move(name)] = std::get<LuaString>(value).to_string();
        else {
          auto text = engine.lua_to<String>(value);
          if (!text)
            return std::unexpected(std::move(text.error()));
          request.headers[std::move(name)] = std::move(*text);
        }
      }
      return {};
    });
    if (!parsed)
      return parsed;
    if (auto valid = validate_http_headers(request.headers); !valid)
      return valid;
  }
  if (request.method == HttpMethod::Get || request.method == HttpMethod::Delete)
    return {}; // These methods never send a body.
  auto body = options.get("body");
  if (!body)
    return std::unexpected(std::move(body.error()));
  if (std::holds_alternative<LuaString>(*body))
    request.body = std::get<LuaString>(*body).to_string();
  else if (!std::holds_alternative<LuaNilType>(*body)) {
    auto json = engine.lua_to<Json>(*body);
    if (!json)
      return std::unexpected(std::move(json.error()));
    auto printed = json->repr();
    if (!printed)
      return std::unexpected(std::move(printed.error()));
    request.body = std::move(*printed);
  }
  return {};
}

}

std::expected<LuaCallbacks, StarException> LuaBindings::make_http_callbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled) {
  if (!session)
    return std::unexpected(LuaException("HTTP callbacks require an owning LuaHttpSession"));
  LuaCallbacks callbacks;
  std::weak_ptr<LuaHttpSession> weak_session = session;
  if (auto result = callbacks.register_callback("available", [enabled]() { return enabled; }); !result)
    return std::unexpected(std::move(result.error()));
  auto request = [weak_session, enabled](LuaEngine& engine, std::expected<HttpMethod, StarException> method,
                                         String const& url, std::optional<LuaTable> const& options) -> HttpPromise {
    if (!enabled)
      return HttpPromise::make_failed("luaHttp disabled by configuration");
    auto owner = weak_session.lock();
    if (!owner)
      return cancelled_promise("HTTP session stopped");
    if (!method)
      return HttpPromise::make_failed(request_error(method.error()));
    auto target = parse_http_url(url);
    if (!target)
      return HttpPromise::make_failed(request_error(target.error()));
    HttpRequest prepared;
    prepared.method = *method;
    prepared.url = std::move(target->url);
    if (options) {
      if (auto parsed = apply_options(prepared, engine, *options); !parsed)
        return HttpPromise::make_failed(request_error(parsed.error()));
    }
    auto promise = owner->m_impl->request(std::move(prepared), std::move(target->host), engine.promise_lifetime(), weak_session);
    if (!promise)
      return HttpPromise::make_failed(request_error(promise.error()));
    return std::move(*promise);
  };
  if (auto result = callbacks.register_callback("createRequest", [request](LuaEngine& engine, String const& method,
      String const& url, std::optional<LuaTable> const& options) {
        return request(engine, parse_http_method(method), url, options);
      }); !result)
    return std::unexpected(std::move(result.error()));
  constexpr std::pair<std::string_view, HttpMethod> methods[]{
    {"get", HttpMethod::Get}, {"post", HttpMethod::Post}, {"put", HttpMethod::Put},
    {"delete", HttpMethod::Delete}, {"patch", HttpMethod::Patch}
  };
  for (auto [name, method] : methods) {
    if (auto result = callbacks.register_callback(name, [request, method](LuaEngine& engine, String const& url,
        std::optional<LuaTable> const& options) { return request(engine, method, url, options); }); !result)
      return std::unexpected(std::move(result.error()));
  }
  if (auto result = callbacks.register_callback("isTrusted", [weak_session](String const& domain) -> std::expected<bool, StarException> {
        auto owner = weak_session.lock();
        if (!owner)
          return false;
        auto host = normalize_http_host(domain);
        if (!host)
          return false;
        std::lock_guard lock(owner->m_impl->mutex);
        return owner->m_impl->trusted(*host);
      }); !result)
    return std::unexpected(std::move(result.error()));
  return callbacks;
}

}
