module;
#include <cassert>
import std;

export module star.main_thread_dispatcher;

export namespace Star {

// Synchronous native calls borrow their caller's stack until completion. The
// owner must keep draining until every producer has joined, including shutdown.
class MainThreadDispatcher {
public:
  MainThreadDispatcher() : m_mainThread(std::this_thread::get_id()) {}
  MainThreadDispatcher(MainThreadDispatcher const&) = delete;
  MainThreadDispatcher& operator=(MainThreadDispatcher const&) = delete;

  bool isMainThread() const { return std::this_thread::get_id() == m_mainThread; }

  template <typename Function>
  auto invoke(Function&& function) -> std::invoke_result_t<Function&> {
    using Result = std::invoke_result_t<Function&>;
    if (isMainThread())
      return std::invoke(function);
    using Value = std::conditional_t<std::is_reference_v<Result>,
        std::reference_wrapper<std::remove_reference_t<Result>>, Result>;
    using Stored = std::conditional_t<std::is_void_v<Result>, std::monostate, Value>;
    struct Call : Task {
      Function& function;
      std::optional<Stored> result;
      explicit Call(Function& f) : function(f) {}
      void execute() override {
        if constexpr (std::is_void_v<Result>) {
          std::invoke(function);
          result.emplace();
        } else if constexpr (std::is_reference_v<Result>) {
          auto&& value = std::invoke(function);
          result.emplace(value);
        } else {
          result.emplace(std::invoke(function));
        }
      }
    } call(function);
    std::unique_lock lock(m_mutex);
    if (m_last)
      m_last->get().next = std::ref<Task>(call);
    else
      m_first = std::ref<Task>(call);
    m_last = std::ref<Task>(call);
    ++m_generation;
    m_changed.notify_all();
    m_changed.wait(lock, [&] { return call.completed; });
    if constexpr (std::is_reference_v<Result>)
      return static_cast<Result>(call.result->get());
    else if constexpr (!std::is_void_v<Result>)
      return std::move(*call.result);
  }

  // Never called during backend consumption: a queued call may flush ordered
  // renderer commands, and must not interleave them with an immutable frame.
  void drain() {
    assert(isMainThread());
    std::unique_lock lock(m_mutex);
    while (m_first) {
      auto& task = m_first->get();
      m_first = task.next;
      if (!m_first)
        m_last.reset();
      lock.unlock();
      task.execute();
      lock.lock();
      task.completed = true;
      ++m_generation;
      m_changed.notify_all();
    }
  }

  // Take the token BEFORE inspecting external work. Notifications occurring
  // between that inspection and wait are retained in the generation predicate.
  std::uint64_t generation() const {
    std::lock_guard lock(m_mutex);
    return m_generation;
  }

  void wake() {
    std::lock_guard lock(m_mutex);
    ++m_generation;
    m_changed.notify_all();
  }

  template <typename Rep, typename Period>
  void wait(std::uint64_t observed, std::chrono::duration<Rep, Period> duration) {
    std::unique_lock lock(m_mutex);
    m_changed.wait_for(lock, duration, [&] { return m_generation != observed; });
  }

private:
  struct Task {
    std::optional<std::reference_wrapper<Task>> next;
    virtual void execute() = 0;
    bool completed = false;

  protected:
    ~Task() = default;
  };
  std::thread::id const m_mainThread;
  mutable std::mutex m_mutex;
  std::condition_variable m_changed;
  std::optional<std::reference_wrapper<Task>> m_first;
  std::optional<std::reference_wrapper<Task>> m_last;
  std::uint64_t m_generation = 0;
};

}
