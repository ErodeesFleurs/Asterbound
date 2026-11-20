module;
import std;
import star.string;
import star.format;

namespace Star {

class LockFile {
public:
  // Empty means contention; filesystem errors remain errors.
  [[nodiscard]] static std::expected<std::optional<LockFile>, StarException> try_acquire(String filename, std::int64_t timeout_ms = 1000);

  explicit LockFile(String filename);
  LockFile(LockFile&& other) noexcept;
  ~LockFile();

  LockFile(LockFile const&) = delete;
  LockFile& operator=(LockFile const&) = delete;

  LockFile& operator=(LockFile&& other) noexcept;

  // Zero makes one attempt. Negative waits indefinitely.
  [[nodiscard]] std::expected<bool, StarException> lock(std::int64_t timeout_ms = 0);
  // Consumes the handle, including when releasing it fails.
  [[nodiscard]] std::expected<void, StarException> unlock();

  [[nodiscard]] bool owns_lock() const noexcept { return m_handle != invalid_handle; }

private:
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  using Handle = std::uintptr_t;
#else
  using Handle = int;
#endif
  static constexpr Handle invalid_handle = static_cast<Handle>(-1);
  [[nodiscard]] std::expected<void, StarException> close_native(Handle handle) const;

  class RetryWindow {
  public:
    explicit RetryWindow(std::int64_t timeout_ms);
    bool wait() const;

  private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_deadline;
    std::chrono::milliseconds m_interval;
  };

  String m_filename;
  Handle m_handle = invalid_handle;
};

}

export module star.lock_file;

export namespace Star {
  using ::Star::LockFile;
}
