module;

import std;
import star.format;
import star.string;

namespace Star {
class LuaCallbacks;
class Root;
class LuaHttpSession;
struct LuaHttpProgress;

struct HttpApproval {
  std::uint64_t id;
  String domain;
};

namespace LuaBindings {
  [[nodiscard]] std::expected<LuaCallbacks, StarException> make_http_callbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled);
}

// The Root must outlive the session. The blocking executor starts on first use.
class LuaHttpSession {
public:
  explicit LuaHttpSession(Root& root);
  ~LuaHttpSession();

  LuaHttpSession(LuaHttpSession const&) = delete;
  LuaHttpSession& operator=(LuaHttpSession const&) = delete;
  LuaHttpSession(LuaHttpSession&&) = delete;
  LuaHttpSession& operator=(LuaHttpSession&&) = delete;

  [[nodiscard]] std::expected<void, StarException> set_approval_available(bool available);
  // A stable snapshot of the front group. No domain copy on unchanged GUI frames.
  [[nodiscard]] std::shared_ptr<HttpApproval const> approval() const;
  [[nodiscard]] std::expected<void, StarException> resolve_approval(std::uint64_t id, bool allow, bool remember);

  // Cancel outstanding Lua promises, cancel queued work, and join active requests.
  // Late trust replies and promise polling stay bound to this stopped session.
  [[nodiscard]] std::expected<void, StarException> stop();
  // Retire completed work even when Lua no longer observes the promise.
  [[nodiscard]] std::expected<void, StarException> poll();

private:
  struct Impl;
  [[nodiscard]] std::expected<void, StarException> poll(std::uint64_t id);
  [[nodiscard]] std::expected<void, StarException> cancel(std::uint64_t id);
  friend struct LuaHttpProgress;
  friend std::expected<LuaCallbacks, StarException> LuaBindings::make_http_callbacks(std::shared_ptr<LuaHttpSession> const& session, bool enabled);

  std::unique_ptr<Impl> m_impl;
};

}

export module star.lua_http_bindings;

export namespace Star {
  using ::Star::HttpApproval;
  using ::Star::LuaHttpSession;
}

export namespace Star::LuaBindings {
  using ::Star::LuaBindings::make_http_callbacks;
}
