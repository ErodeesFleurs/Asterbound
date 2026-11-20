import std;
import star.listener;
import star.format;
import star.unique_function;

namespace Star {

struct ListenerRegistration {
  explicit ListenerRegistration(std::weak_ptr<Listener> listener) : target(std::move(listener)) {}

  std::weak_ptr<Listener> const target;
  std::mutex mutex;
  std::condition_variable idle;
  std::atomic<bool> active{true};
  std::size_t running = 0;
  std::thread::id invoking_thread;

  bool prunable() {
    if (active.load(std::memory_order_relaxed) && !target.expired())
      return false;
    // Neither expiration nor cancellation alone guarantees callback teardown is over.
    std::lock_guard lock(mutex);
    return running == 0;
  }

  void deactivate() {
    std::lock_guard lock(mutex);
    active.store(false, std::memory_order_relaxed);
  }

  void disconnect() {
    std::unique_lock lock(mutex);
    active.store(false, std::memory_order_relaxed);
    // Dispatch is serial per group. Nested invocations run on the same thread;
    // self-disconnection cannot wait for its own stack frames to return.
    if (running && invoking_thread == std::this_thread::get_id())
      return;
    idle.wait(lock, [&] { return running == 0; });
  }

  [[nodiscard]] std::expected<void, StarException> invoke() {
    std::shared_ptr<Listener> listener;
    {
      std::lock_guard lock(mutex);
      if (!active.load(std::memory_order_relaxed) || !(listener = target.lock()))
        return {};
      ++running;
      invoking_thread = std::this_thread::get_id();
    }
    auto result = listener->trigger();
    listener.reset();
    complete();
    return result;
  }

  void complete() {
    std::lock_guard lock(mutex);
    if (--running == 0 && !active.load(std::memory_order_relaxed))
      idle.notify_all();
  }
};

Listener::~Listener() = default;

CallbackListener::CallbackListener(std::function<std::expected<void, StarException>()> callback)
  : m_callback(callback ? Callback(std::move(callback)) : Callback{}) {}

[[nodiscard]] std::expected<void, StarException> CallbackListener::trigger() {
  if (m_callback)
    return m_callback();
  return std::unexpected(StarException("Cannot invoke an empty listener callback"));
}

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
  clear();
}

ListenerSubscription ListenerGroup::subscribe(std::weak_ptr<Listener> listener) {
  std::lock_guard lock(m_mutex);
  auto const [first, end] = m_listeners.equal_range(listener);
  auto slot = end;
  if (first != end) {
    // The newest registration is last among equivalent owners.
    auto previous = std::prev(end);
    if (previous->second->active.load(std::memory_order_relaxed))
      return ListenerSubscription(previous->second);
    if (previous->second->prunable())
      slot = previous;
  }
  auto registration = std::make_shared<ListenerRegistration>(listener);
  if (slot != end)
    slot->second = registration;
  else
    m_listeners.emplace_hint(end, std::move(listener), registration);
  return ListenerSubscription(std::move(registration));
}

void ListenerGroup::prune() {
  std::lock_guard lock(m_mutex);
  std::erase_if(m_listeners, [](decltype(m_listeners)::value_type const& entry) { return entry.second->prunable(); });
}

void ListenerGroup::clear() {
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
  std::lock_guard dispatch_lock(m_dispatch_mutex);
  std::vector<std::shared_ptr<ListenerRegistration>> snapshot;
  {
    std::lock_guard registry_lock(m_mutex);
    snapshot.reserve(m_listeners.size());
    for (auto it = m_listeners.begin(); it != m_listeners.end();) {
      if (it->second->prunable()) {
        it = m_listeners.erase(it);
      } else {
        snapshot.push_back(it->second);
        ++it;
      }
    }
  }
  for (auto const& registration : snapshot) {
    if (auto result = registration->invoke(); !result)
      return result;
  }
  return {};
}

}
