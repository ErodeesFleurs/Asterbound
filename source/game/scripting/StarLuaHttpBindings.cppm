module;

import std;
import star.format;

namespace Star {
class LuaCallbacks;
class Root;
class String;
class LuaHttpSession;
struct LuaHttpProgress;

namespace LuaBindings {
  using HttpTrustRequestCallback = std::function<std::expected<void, StarException>(String const& domain)>;
  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeHttpCallbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled);
}

// One script/client session owns its trust policy and blocking HTTP executor.
// The Root must outlive the session. Stop before destroying its GUI callback.
class LuaHttpSession {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<LuaHttpSession>, StarException> create(Root& root);
  ~LuaHttpSession();

  LuaHttpSession(LuaHttpSession const&) = delete;
  LuaHttpSession& operator=(LuaHttpSession const&) = delete;
  LuaHttpSession(LuaHttpSession&&) = delete;
  LuaHttpSession& operator=(LuaHttpSession&&) = delete;

  void setHttpTrustRequestCallback(LuaBindings::HttpTrustRequestCallback callback);
  void clearHttpTrustRequestCallback();
  [[nodiscard]] std::expected<void, StarException> handleHttpTrustReply(String const& domain, bool allowed);

  // Cancel outstanding Lua promises, cancel queued work, and join active requests.
  // Late trust replies and promise polling stay bound to this stopped session.
  [[nodiscard]] std::expected<void, StarException> stop();
  [[nodiscard]] std::expected<void, StarException> poll();

private:
  struct Impl;
  explicit LuaHttpSession(std::unique_ptr<Impl> impl);
  [[nodiscard]] std::expected<void, StarException> cancel(std::uint64_t id);
  friend struct LuaHttpProgress;
  friend std::expected<LuaCallbacks, StarException> LuaBindings::makeHttpCallbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled);

  std::unique_ptr<Impl> m_impl;
};

}

export module star.lua_http_bindings;

export namespace Star {
  using ::Star::LuaHttpSession;
}

export namespace Star::LuaBindings {
  using ::Star::LuaBindings::HttpTrustRequestCallback;
  using ::Star::LuaBindings::makeHttpCallbacks;
}
