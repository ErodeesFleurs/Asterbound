import std;
import star.listener;
import star.format;

namespace Star {

struct ListenerRegistration {
  std::mutex mutex;
  std::condition_variable idle;
  bool active = true;
  std::size_t running = 0;
  std::thread::id invokingThread;

  bool connected() {
    std::lock_guard lock(mutex);
    return active;
  }

  void deactivate() {
    std::lock_guard lock(mutex);
    active = false;
  }

  void disconnect() {
    std::unique_lock lock(mutex);
    active = false;
    // Dispatch is serial per group. Nested invocations run on the same thread;
    // self-disconnection cannot wait for its own stack frames to return.
    if (running && invokingThread == std::this_thread::get_id())
      return;
    idle.wait(lock, [&] { return running == 0; });
  }

  [[nodiscard]] std::expected<void, StarException> invoke(std::weak_ptr<Listener> const& weakListener) {
    std::shared_ptr<Listener> listener;
    {
      std::lock_guard lock(mutex);
      if (!active || !(listener = weakListener.lock()))
        return {};
      ++running;
      invokingThread = std::this_thread::get_id();
    }
    auto result = listener->trigger();
    listener.reset();
    complete();
    return result;
  }

  void complete() {
    std::lock_guard lock(mutex);
    if (--running == 0)
      idle.notify_all();
  }
};

Listener::~Listener() = default;

CallbackListener::CallbackListener(std::function<std::expected<void, StarException>()> callback)
  : callback(std::move(callback)) {}

[[nodiscard]] std::expected<void, StarException> CallbackListener::trigger() {
  if (callback)
    return callback();
  return std::unexpected(StarException("Cannot invoke an empty listener callback"));
}

TrackerListener::TrackerListener() : triggered(false) {}

ListenerSubscription::ListenerSubscription(std::shared_ptr<ListenerRegistration> registration)
  : m_registration(std::move(registration)) {}

ListenerSubscription::ListenerSubscription(ListenerSubscription&& other) noexcept = default;

ListenerSubscription& ListenerSubscription::operator=(ListenerSubscription&& other) noexcept {
  if (this != &other) {
    disconnect();
    m_registration = std::move(other.m_registration);
  }
  return *this;
}

ListenerSubscription::~ListenerSubscription() {
  disconnect();
}

void ListenerSubscription::disconnect() {
  // Keep the handle itself intact: concurrent disconnect calls on a subscription
  // synchronize through the registration, without racing a shared_ptr reset.
  if (m_registration)
    m_registration->disconnect();
}

ListenerGroup::~ListenerGroup() {
  clearAllListeners();
}

ListenerSubscription ListenerGroup::addListener(std::weak_ptr<Listener> listener) {
  std::lock_guard lock(m_mutex);
  auto existing = m_listeners.find(listener);
  if (existing != m_listeners.end() && existing->second->connected())
    return ListenerSubscription(existing->second);
  auto registration = std::make_shared<ListenerRegistration>();
  m_listeners.insert_or_assign(std::move(listener), registration);
  return ListenerSubscription(std::move(registration));
}

void ListenerGroup::clearExpiredListeners() {
  std::lock_guard lock(m_mutex);
  std::erase_if(m_listeners, [](auto const& entry) { return entry.first.expired(); });
}

void ListenerGroup::clearAllListeners() {
  decltype(m_listeners) listeners;
  {
    std::lock_guard lock(m_mutex);
    for (auto const& entry : m_listeners)
      entry.second->deactivate();
    listeners.swap(m_listeners);
  }
  for (auto const& entry : listeners)
    entry.second->disconnect();
}

[[nodiscard]] std::expected<void, StarException> ListenerGroup::trigger() {
  std::lock_guard dispatchLock(m_dispatchMutex);
  std::vector<std::pair<std::weak_ptr<Listener>, std::shared_ptr<ListenerRegistration>>> listeners;
  {
    std::lock_guard registryLock(m_mutex);
    std::erase_if(m_listeners, [](auto const& entry) { return entry.first.expired(); });
    listeners.assign(m_listeners.begin(), m_listeners.end());
  }
  for (auto const& entry : listeners) {
    auto result = entry.second->invoke(entry.first);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

}
