module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif
import std;
import star.format;
import star.logging;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.windows_dump;
#endif

export module star.signal_handler;

export namespace Star {

// One successful registration per process, including after destruction: an
// already-dispatched native callback cannot be drained by unregistering it.
// A failed installation can be retried only after complete rollback.
class SignalHandler {
public:
  [[nodiscard]] static std::expected<SignalHandler, StarException> create();
  ~SignalHandler();

  SignalHandler(SignalHandler const&) = delete;
  SignalHandler& operator=(SignalHandler const&) = delete;
  SignalHandler(SignalHandler&&) noexcept;
  SignalHandler& operator=(SignalHandler&&) = delete;

  // Sticky for the lifetime of the registration. A moved-from owner returns false.
  [[nodiscard]] bool interrupted() const noexcept;

private:
  struct Impl;
  explicit SignalHandler(std::unique_ptr<Impl> impl) noexcept;
  [[nodiscard]] bool restore() noexcept;

  std::unique_ptr<Impl> m_impl;
};

}

namespace Star {

namespace {
enum class RegistrationState { available, owned, retired };
constinit std::atomic<RegistrationState> registration_state{RegistrationState::available};

// Callbacks never touch the registration owner or publish other data.
static_assert(std::atomic<bool>::is_always_lock_free);
constinit std::atomic<bool> interrupt_pending{false};
}

#ifdef STAR_SYSTEM_FAMILY_WINDOWS

namespace {
constinit std::atomic_flag fatal_claimed{};

std::string_view fault_name(DWORD code) noexcept {
  switch (code) {
    case EXCEPTION_STACK_OVERFLOW: return "Stack overflow";
    case EXCEPTION_ACCESS_VIOLATION: return "Access violation";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION: return "Illegal instruction";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW: return "Floating point exception";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Division by zero";
    case EXCEPTION_INT_OVERFLOW: return "Integer overflow";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_INVALID_HANDLE: return "Fatal exception";
    default: return {};
  }
}

struct CrashReport {
  EXCEPTION_POINTERS& exception;
  DWORD thread_id;
  std::string_view name;
};

DWORD WINAPI report_crash(LPVOID argument) noexcept {
  using namespace std::string_view_literals;
  auto& report = *static_cast<CrashReport*>(argument);
  auto dumped = try_write_minidump(report.thread_id, report.exception);
  auto const& record = *report.exception.ExceptionRecord;
  std::array<char, 192> text{};
  auto limit = text.end() - 1;
  auto end = std::format_to_n(text.begin(), limit - text.begin(), "{} (0x{:08X} at {})",
      report.name, record.ExceptionCode, record.ExceptionAddress).out;
  if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
    auto mode = record.ExceptionInformation[0];
    auto operation = mode == 0 ? "read"sv : mode == 1 ? "write"sv : mode == 8 ? "execute"sv : "access"sv;
    end = std::format_to_n(end, limit - end, ": {} at {:#x}", operation, record.ExceptionInformation[1]).out;
  }
  end = std::format_to_n(end, limit - end, "; {}\n", dumped ? "wrote starbound.dmp" : "minidump failed").out;
  DWORD written;
  if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE), text.data(), static_cast<DWORD>(end - text.begin()), &written, nullptr))
    OutputDebugStringA(text.data());
  return 0;
}

LONG CALLBACK handle_fatal(PEXCEPTION_POINTERS exception) noexcept {
  auto code = exception->ExceptionRecord->ExceptionCode;
  auto name = fault_name(code);
  if (name.empty() || fatal_claimed.test_and_set(std::memory_order_relaxed))
    return EXCEPTION_CONTINUE_SEARCH;

  // Never rewrite or resume a damaged stack. A fresh stack performs all dump
  // and output work; DLL initialization, DbgHelp or a full pipe may be stuck.
  // This dispatch frame and its native exception pointers live until exit.
  CrashReport report{*exception, GetCurrentThreadId(), name};
  auto thread = CreateThread(nullptr, 0, report_crash, &report, 0, nullptr);
  if (!thread || WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0)
    OutputDebugStringA("Fatal exception: minidump worker unavailable or timed out\n");
  TerminateProcess(GetCurrentProcess(), code);
  std::_Exit(EXIT_FAILURE);
}

BOOL WINAPI handle_interrupt(DWORD event) noexcept {
  if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT)
    return FALSE;
  interrupt_pending.store(true, std::memory_order_relaxed);
  return TRUE;
}
}

struct SignalHandler::Impl {
  PVOID fatal_handler = nullptr;
  bool interrupt_installed = false;

  [[nodiscard]] std::expected<void, StarException> install() {
    fatal_handler = AddVectoredExceptionHandler(1, handle_fatal);
    if (!fatal_handler)
      return std::unexpected(StarException("Could not register fatal exception handler"));
    if (!SetConsoleCtrlHandler(handle_interrupt, TRUE))
      return std::unexpected(StarException("Could not register console interrupt handler"));
    interrupt_installed = true;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> restore() {
    if (interrupt_installed && SetConsoleCtrlHandler(handle_interrupt, FALSE))
      interrupt_installed = false;
    if (fatal_handler && RemoveVectoredExceptionHandler(fatal_handler))
      fatal_handler = nullptr;
    if (interrupt_installed)
      return std::unexpected(StarException("Could not remove console interrupt handler"));
    if (fatal_handler)
      return std::unexpected(StarException("Could not remove fatal exception handler"));
    return {};
  }
};

#else

struct SignalHandler::Impl {
  struct Registration {
    int signal_number;
    struct sigaction previous{};
    bool installed = false;
  };

  std::array<Registration, 5> registrations{{{SIGSEGV}, {SIGILL}, {SIGFPE}, {SIGBUS}, {SIGINT}}};

  [[nodiscard]] std::expected<void, StarException> install() {
    struct sigaction action{};
    if (::sigemptyset(&action.sa_mask) != 0)
      return std::unexpected(StarException("Could not initialize signal handler"));
    for (auto& registration : registrations) {
      action.sa_handler = registration.signal_number == SIGINT ? handle_interrupt : handle_fatal;
      if (::sigaction(registration.signal_number, &action, &registration.previous) != 0)
        return std::unexpected(StarException("Could not register signal handler"));
      registration.installed = true;
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> restore() {
    bool failed = false;
    for (auto& registration : registrations | std::views::reverse) {
      if (!registration.installed)
        continue;
      if (::sigaction(registration.signal_number, &registration.previous, nullptr) == 0)
        registration.installed = false;
      else
        failed = true;
    }
    if (failed)
      return std::unexpected(StarException("Could not restore signal handler"));
    return {};
  }

  [[noreturn]] static void handle_fatal(int signal_number) noexcept {
    using namespace std::string_view_literals;
    auto message = signal_number == SIGSEGV ? "Segfault Encountered!\n"sv
        : signal_number == SIGILL ? "Illegal Instruction Encountered!\n"sv
        : signal_number == SIGFPE ? "Floating Point Exception Encountered!\n"sv
        : "Bus Error Encountered!\n"sv;
    (void)::write(STDERR_FILENO, message.data(), message.size());
    std::_Exit(128 + signal_number);
  }

  static void handle_interrupt(int) noexcept {
    interrupt_pending.store(true, std::memory_order_relaxed);
  }
};

#endif

SignalHandler::SignalHandler(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
SignalHandler::SignalHandler(SignalHandler&&) noexcept = default;

std::expected<SignalHandler, StarException> SignalHandler::create() {
  auto state = RegistrationState::available;
  if (!registration_state.compare_exchange_strong(state, RegistrationState::owned, std::memory_order_acquire,
                                                  std::memory_order_relaxed))
    return std::unexpected(StarException(state == RegistrationState::retired
        ? "Process signal registration has already ended"
        : "Process signal registration is already owned"));

  SignalHandler owner(std::make_unique<Impl>());
  if (auto installed = owner.m_impl->install(); !installed) {
    // The interrupt callback is installed last: a failed install never published it.
    if (owner.restore())
      registration_state.store(RegistrationState::available, std::memory_order_release);
    return std::unexpected(std::move(installed.error()));
  }
  return owner;
}

bool SignalHandler::restore() noexcept {
  if (auto restored = m_impl->restore(); !restored) {
    // Keep both the claim and the saved native dispositions if removal failed.
    static auto const unrestored = std::move(m_impl);
    EmergencyLog::error("Signal handler teardown: {}", restored.error().what());
    return false;
  }
  m_impl.reset();
  return true;
}

SignalHandler::~SignalHandler() {
  if (m_impl && restore())
    registration_state.store(RegistrationState::retired, std::memory_order_release);
}

bool SignalHandler::interrupted() const noexcept {
  return m_impl && interrupt_pending.load(std::memory_order_relaxed);
}

}
