module;
import std;
import star.format;


namespace Star {

class Listener {
public:
  virtual ~Listener();
  [[nodiscard]] virtual std::expected<void, StarException> trigger() = 0;
};

class CallbackListener : public Listener {
public:
  CallbackListener(std::function<std::expected<void, StarException>()> callback);

protected:
  [[nodiscard]] std::expected<void, StarException> trigger() override;

private:
  std::function<std::expected<void, StarException>()> callback;
};

class TrackerListener : public Listener {
public:
  TrackerListener();

  bool pullTriggered();

protected:
  [[nodiscard]] std::expected<void, StarException> trigger() override;

private:
  std::atomic<bool> triggered;
};

struct ListenerRegistration;

// Scoped cancellation. Disconnect prevents subsequent invocations and waits for
// a callback already in progress, except when called by that callback itself.
// Duplicate registrations share cancellation, identified by shared owner identity.
class ListenerSubscription {
public:
  ListenerSubscription() = default;
  ListenerSubscription(ListenerSubscription const&) = delete;
  ListenerSubscription& operator=(ListenerSubscription const&) = delete;
  ListenerSubscription(ListenerSubscription&& other) noexcept;
  ListenerSubscription& operator=(ListenerSubscription&& other) noexcept;
  ~ListenerSubscription();

  void disconnect();

private:
  friend class ListenerGroup;
  explicit ListenerSubscription(std::shared_ptr<ListenerRegistration> registration);
  std::shared_ptr<ListenerRegistration> m_registration;
};

class ListenerGroup {
public:
  ~ListenerGroup();
  [[nodiscard]] ListenerSubscription addListener(std::weak_ptr<Listener> listener);
  void clearExpiredListeners();
  void clearAllListeners();

  [[nodiscard]] std::expected<void, StarException> trigger();

private:
  // Registry access never invokes callbacks. Dispatch is serial within a group,
  // but permits a callback to trigger that same group recursively.
  std::mutex m_mutex;
  std::recursive_mutex m_dispatchMutex;
  std::map<std::weak_ptr<Listener>, std::shared_ptr<ListenerRegistration>,
      std::owner_less<std::weak_ptr<Listener>>> m_listeners;
};

inline bool TrackerListener::pullTriggered() {
  return triggered.exchange(false);
}

[[nodiscard]] inline std::expected<void, StarException> TrackerListener::trigger() {
  triggered = true;
  return {};
}

}

export module star.listener;

export namespace Star {
  using ::Star::Listener;
  using ::Star::CallbackListener;
  using ::Star::TrackerListener;
  using ::Star::ListenerSubscription;
  using ::Star::ListenerGroup;
}
