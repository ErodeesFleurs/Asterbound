module;
import std;
import star.map;
import star.format;
import star.string;
import star.either;
import star.host_address;
import star.thread;

namespace Star {

class Logger;

// Returned when a socket is closed or shutdown; other failures use NetworkException.
struct SocketClosedExceptionTag {
  static constexpr std::string_view name() { return "SocketClosedException"; }
};
using SocketClosedException = StarError<SocketClosedExceptionTag, NetworkException>;

class Socket;
struct SocketImpl;

enum class SocketMode {
  Closed,
  Shutdown,
  Bound,
  Connected
};

struct SocketPollQueryEntry {
  // Query whether the tcp socket is readable
  bool readable;
  // Query whether the tcp socket is writable
  bool writable;
};

struct SocketPollResultEntry {
  // The tcp socket can be read without blocking
  bool readable;
  // The tcp socket can be written without blocking
  bool writable;
  // The tcp socket has had an error condition, or it has been closed.
  bool exception;
};

typedef Map<std::shared_ptr<Socket>, SocketPollQueryEntry> SocketPollQuery;
typedef Map<std::shared_ptr<Socket>, SocketPollResultEntry> SocketPollResult;

class Socket {
public:
  // Waits for sockets that are readable, writiable, or have pending error
  // conditions within the given timeout.  Returns result if any sockets are
  // ready for I/O or have had error events occur on them within the timeout,
  // nothing otherwise.  If socket hangup occurs during this call, this will
  // automatically shut down the socket.
  [[nodiscard]] static std::expected<std::optional<SocketPollResult>, StarException> poll(SocketPollQuery const& query, unsigned timeout);

  // Wait for native read/write readiness or shutdown until a steady deadline.
  // Retains the native handle, not m_mutex, so close can shut down and wake it.
  [[nodiscard]] std::expected<void, StarException> waitForData(bool writable, std::chrono::steady_clock::time_point deadline) const;

  ~Socket();

  [[nodiscard]] std::expected<void, StarException> bind(Logger& logger, HostAddressWithPort const& address);
  [[nodiscard]] std::expected<void, StarException> listen(int backlog);

  // Sockets default to blocking mode
  [[nodiscard]] std::expected<void, StarException> setNonBlocking(bool nonBlocking);
  // Sockets default to 60 second timeout
  [[nodiscard]] std::expected<void, StarException> setTimeout(unsigned millis);

  NetworkMode networkMode() const;
  SocketMode socketMode() const;

  // Is the socketMode either Bound or Connected?
  bool isActive() const;

  // Is the socketMode not closed?
  bool isOpen() const;

  // Shuts down the underlying socket only.
  void shutdown();

  // Interrupts blocking I/O before waiting to close the underlying socket.
  void close();

protected:
  friend struct SocketTestAccess;

  enum class SocketType {
    Tcp,
    Udp
  };

  explicit Socket(NetworkMode networkMode);
  [[nodiscard]] std::expected<void, StarException> initialize(SocketType type);
  [[nodiscard]] std::expected<void, StarException> configure();
  Socket(NetworkMode networkMode, std::shared_ptr<SocketImpl> impl, SocketMode socketMode);

  [[nodiscard]] std::expected<void, StarException> checkOpen(std::string_view methodName) const;
  void doShutdown();
  void doClose();

  mutable ReadersWriterMutex m_mutex;
  NetworkMode m_networkMode;
  std::shared_ptr<SocketImpl> m_impl;
  std::atomic<SocketMode> m_socketMode;
  HostAddressWithPort m_localAddress;
};

}

export module star.socket;

export namespace Star {
  using ::Star::SocketClosedExceptionTag;
  using ::Star::SocketClosedException;
  using ::Star::SocketMode;
  using ::Star::SocketPollQueryEntry;
  using ::Star::SocketPollResultEntry;
  using ::Star::SocketPollQuery;
  using ::Star::SocketPollResult;
  using ::Star::SocketImpl;
  using ::Star::Socket;
}
