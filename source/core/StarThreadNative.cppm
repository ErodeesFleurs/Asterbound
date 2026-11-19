module;
import std;
import star.format;
import star.string;

namespace Star {

// Shared lifecycle policy; only launch_native, join_native and their C ABI
// entry trampolines depend on the platform. The handle is never observed by
// queries or read by the worker.
class NativeThread {
public:
  using Execute = void(NativeThread&);

  // For a newly constructed, unpublished one-shot state, or under m_mutex.
  [[nodiscard]] std::expected<void, StarException> launch();
  [[nodiscard]] std::expected<bool, StarException> start();
  [[nodiscard]] std::expected<bool, StarException> join();
  bool is_joined() const noexcept { return m_phase.load(std::memory_order_acquire) == Phase::Joined; }
  bool is_running() const noexcept { return m_phase.load(std::memory_order_acquire) == Phase::Running; }
  void run_worker();
  String const name;

protected:
  NativeThread(String const& name, Execute& execute) : name(name), m_execute(execute) {}
  ~NativeThread() = default;

private:
  enum class Phase : unsigned char { Joined, Running, Finished };
  static_assert(std::atomic<Phase>::is_always_lock_free);
  [[nodiscard]] std::expected<void, StarException> launch_native();
  [[nodiscard]] std::expected<void, StarException> join_native();

  Execute& m_execute;
  std::atomic<Phase> m_phase{Phase::Joined};
  std::mutex m_mutex;
#ifdef STAR_SYSTEM_WINDOWS
  std::uintptr_t m_handle{};
#else
  std::thread::native_handle_type m_handle{};
#endif
};

}

export module star.thread_native;
export namespace Star {
  using ::Star::NativeThread;
}
