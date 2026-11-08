module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#include <dbghelp.h>
#else
#include <signal.h>
#include <unistd.h>
#endif
import std;
import star.format;
import star.logging;

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string;
#endif

export module star.signal_handler;

export namespace Star {

struct SignalHandlerImpl;

// Process-owned registration for fatal signals and non-fatal interrupts.
// Only the native callbacks' minimal bridge state is process-wide.
class SignalHandler {
public:
  SignalHandler();
  ~SignalHandler();

  SignalHandler(SignalHandler const&) = delete;
  SignalHandler& operator=(SignalHandler const&) = delete;

  // If enabled, will catch segfault, fpe, and illegal instructions and output
  // error information before dying.
  [[nodiscard]] std::expected<void, StarException> setHandleFatal(bool handleFatal);
  bool handlingFatal() const;

  // If enabled, non-fatal interrupt signal will be caught and will not kill
  // the process and will instead set the interrupted flag.
  [[nodiscard]] std::expected<void, StarException> setHandleInterrupt(bool handleInterrupt);
  bool handlingInterrupt() const;

  bool interruptCaught() const;

private:
  std::unique_ptr<SignalHandlerImpl> m_impl;
};

}

namespace Star {

#ifdef STAR_SYSTEM_FAMILY_WINDOWS

DWORD WINAPI writeMiniDump(void* ExceptionInfo) {
  auto hFile = CreateFileA("starbound.dmp", GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
  if (hFile == INVALID_HANDLE_VALUE)
    return 0;
  MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo{};
  dumpExceptionInfo.ThreadId = GetCurrentThreadId();
  dumpExceptionInfo.ExceptionPointers = (PEXCEPTION_POINTERS)ExceptionInfo;
  dumpExceptionInfo.ClientPointers = FALSE;
  MiniDumpWriteDump(
    GetCurrentProcess(),
    GetCurrentProcessId(),
    hFile,
    MiniDumpNormal,
    &dumpExceptionInfo,
    NULL,
    NULL);
  CloseHandle(hFile);
  if (dumpExceptionInfo.ExceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
    MessageBoxA(NULL, "Stack overflow encountered\nA minidump has been generated", NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }
  return 0;
}

// Native callbacks and the resumed trampoline may outlive registration removal.
// Retain only the first fatal exception's native data for the process lifetime;
// no callback borrows the SignalHandler, its Impl, or application services.
struct FatalSignalBridge {
  std::atomic_flag claimed{};
  EXCEPTION_RECORD exceptionRecord{};
  char const* message = nullptr;
};
static_assert(std::is_trivially_destructible_v<FatalSignalBridge>);
static constinit FatalSignalBridge fatalSignalBridge;
static std::atomic<bool> interruptSignalPending{false};
static_assert(std::atomic<bool>::is_always_lock_free);

struct SignalHandlerImpl {
  bool handlingFatal = false;
  bool handlingInterrupt = false;
  bool interrupted = false;
  // All callback-accessed state has process lifetime, not this object's lifetime.

  PVOID handler = nullptr;

  ~SignalHandlerImpl() {
    auto fatal = setHandleFatal(false);
    if (!fatal)
      EmergencyLog::error("Signal handler teardown: {}", fatal.error().what());
    auto interrupt = setHandleInterrupt(false);
    if (!interrupt)
      EmergencyLog::error("Signal handler teardown: {}", interrupt.error().what());
  }

  [[nodiscard]] std::expected<void, StarException> setHandleFatal(bool b) {
    if (handlingFatal == b)
      return {};

    if (b) {
      handler = AddVectoredExceptionHandler(1, vectoredExceptionHandler);
      if (!handler)
        return std::unexpected(StarException("Could not register fatal exception handler"));
      handlingFatal = true;
    } else {
      if (!RemoveVectoredExceptionHandler(handler))
        return std::unexpected(StarException("Could not remove fatal exception handler"));
      handler = nullptr;
      handlingFatal = false;
    }
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> setHandleInterrupt(bool b) {
    if (handlingInterrupt == b)
      return {};
    if (b) {
      interruptSignalPending.store(false);
      if (!SetConsoleCtrlHandler(consoleCtrlHandler, true))
        return std::unexpected(StarException("Could not register console interrupt handler"));
      handlingInterrupt = true;
    } else {
      if (!SetConsoleCtrlHandler(consoleCtrlHandler, false))
        return std::unexpected(StarException("Could not remove console interrupt handler"));
      interrupted = interrupted || interruptSignalPending.load();
      handlingInterrupt = false;
    }
    return {};
  }

  static void sehTrampoline() {
    auto const& exception = fatalSignalBridge.exceptionRecord;
    String message;
    if (exception.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
      String mode;
      auto modeFlag = exception.ExceptionInformation[0];
      if (modeFlag == 0)
        mode = "Read";
      else if (modeFlag == 1)
        mode = "Write";
      else if (modeFlag == 8)
        mode = "Execute";
      else
        mode = strf("Mode({})", modeFlag);
      message = strf("Access violation detected at {} ({} of address {})",
          exception.ExceptionAddress,
          mode,
          (PVOID)exception.ExceptionInformation[1]);
    } else {
      message = strf("{} (0x{:08X} @ {})",
          fatalSignalBridge.message,
          exception.ExceptionCode,
          exception.ExceptionAddress);
      for (DWORD i = 0; i < exception.NumberParameters; i++)
        message = strf("{} [{}]", message, (PVOID)exception.ExceptionInformation[i]);
    }
    fatalError(message.utf8_data(), true);
    TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
  }

  static LONG handleFatalError(char const* message, PEXCEPTION_POINTERS ExceptionInfo) {
    // Never replace a snapshot already consumed by a resumed trampoline. A
    // concurrent or recursive fault continues through native exception dispatch.
    if (fatalSignalBridge.claimed.test_and_set(std::memory_order_relaxed))
      return EXCEPTION_CONTINUE_SEARCH;
    fatalSignalBridge.exceptionRecord = *ExceptionInfo->ExceptionRecord;
    fatalSignalBridge.exceptionRecord.ExceptionRecord = nullptr;
    fatalSignalBridge.message = message;
    writeMiniDump(ExceptionInfo);

// setup a hijack into our own trampoline as if the failure actually was a
// function call
#ifdef STAR_ARCHITECTURE_X86_64
    DWORD64 rsp = ExceptionInfo->ContextRecord->Rsp - 8;
    DWORD64 rip = ExceptionInfo->ContextRecord->Rip; // an offset avoid the issue of gdb thinking
    // the error is one statement too early, but
    // the offset is instruction dependent, and we
    // don't know its size + 1;
    *((DWORD64*)rsp) = rip;
    ExceptionInfo->ContextRecord->Rsp = rsp;
    ExceptionInfo->ContextRecord->Rip = (DWORD64)&sehTrampoline;
#else
    DWORD esp = ExceptionInfo->ContextRecord->Esp - 4;
    DWORD eip = ExceptionInfo->ContextRecord->Eip; // an offset avoid the issue of gdb thinking the
    // error is one statement too early, but the
    // offset is instruction dependent, and we don't
    // know its size + 1;
    *((DWORD*)esp) = eip;
    ExceptionInfo->ContextRecord->Esp = esp;
    ExceptionInfo->ContextRecord->Eip = (DWORD)&sehTrampoline;
#endif
    return EXCEPTION_CONTINUE_EXECUTION;
  }

  static LONG CALLBACK vectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    LONG result = EXCEPTION_CONTINUE_SEARCH;

    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
      return handleFatalError("Stack overflow detected", ExceptionInfo);
    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
      return handleFatalError("Access violation detected", ExceptionInfo);
    if ((ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION)) {
      return handleFatalError("Illegal instruction encountered", ExceptionInfo);
    }

    if ((ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_DENORMAL_OPERAND)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_DIVIDE_BY_ZERO)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_INEXACT_RESULT)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_INVALID_OPERATION)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_OVERFLOW)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_STACK_CHECK)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_FLT_UNDERFLOW)
            ) {
      return handleFatalError("Floating point exception", ExceptionInfo);
    }

    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) {
      return handleFatalError("Division by zero", ExceptionInfo);
    }

    if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_INT_OVERFLOW) {
      return handleFatalError("Integer overflow", ExceptionInfo);
    }

    if ((ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_DATATYPE_MISALIGNMENT)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_NONCONTINUABLE_EXCEPTION)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_INVALID_DISPOSITION)
        || (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_INVALID_HANDLE)) {
      return handleFatalError("Error occurred", ExceptionInfo);
    }

    return result;
  }

  static BOOL WINAPI consoleCtrlHandler(DWORD) {
    interruptSignalPending.store(true);
    return true;
  }
};

#else

// The POSIX callback may only communicate through async-signal-safe state.
static volatile sig_atomic_t interruptSignalPending = 0;

struct SignalHandlerImpl {
  bool handlingFatal = false;
  bool handlingInterrupt = false;
  bool interrupted = false;
  struct sigaction previousFatal[4]{};
  std::array<bool, 4> installedFatal{};
  struct sigaction previousInterrupt{};

  static constexpr int FatalSignals[] = {SIGSEGV, SIGILL, SIGFPE, SIGBUS};

  ~SignalHandlerImpl() {
    auto fatal = setHandleFatal(false);
    if (!fatal)
      EmergencyLog::error("Signal handler teardown: {}", fatal.error().what());
    auto interrupt = setHandleInterrupt(false);
    if (!interrupt)
      EmergencyLog::error("Signal handler teardown: {}", interrupt.error().what());
  }

  [[nodiscard]] std::expected<void, StarException> setHandleFatal(bool b) {
    if (b) {
      struct sigaction action{};
      action.sa_handler = handleFatal;
      if (sigemptyset(&action.sa_mask) != 0)
        return std::unexpected(StarException("Could not initialize fatal signal handler"));
      std::array<bool, 4> added{};
      for (unsigned i = 0; i < std::size(FatalSignals); ++i) {
        if (installedFatal[i])
          continue;
        if (sigaction(FatalSignals[i], &action, &previousFatal[i]) != 0) {
          for (unsigned j = 0; j < i; ++j) {
            if (!added[j])
              continue;
            if (sigaction(FatalSignals[j], &previousFatal[j], nullptr) == 0)
              installedFatal[j] = false;
            else
              EmergencyLog::error("Could not restore fatal signal handler during registration rollback");
          }
          handlingFatal = std::ranges::any_of(installedFatal, [](bool installed) { return installed; });
          return std::unexpected(StarException("Could not register fatal signal handler"));
        }
        installedFatal[i] = true;
        added[i] = true;
      }
    } else {
      bool failed = false;
      for (unsigned i = 0; i < std::size(FatalSignals); ++i) {
        if (!installedFatal[i])
          continue;
        if (sigaction(FatalSignals[i], &previousFatal[i], nullptr) == 0)
          installedFatal[i] = false;
        else
          failed = true;
      }
      handlingFatal = std::ranges::any_of(installedFatal, [](bool installed) { return installed; });
      if (failed)
        return std::unexpected(StarException("Could not restore fatal signal handler"));
    }
    handlingFatal = b;
    return {};
  }

  [[nodiscard]] std::expected<void, StarException> setHandleInterrupt(bool b) {
    if (handlingInterrupt == b)
      return {};
    if (b) {
      struct sigaction action{};
      action.sa_handler = handleInterrupt;
      if (sigemptyset(&action.sa_mask) != 0)
        return std::unexpected(StarException("Could not initialize interrupt signal handler"));
      interruptSignalPending = 0;
      if (sigaction(SIGINT, &action, &previousInterrupt) != 0)
        return std::unexpected(StarException("Could not register interrupt signal handler"));
    } else {
      if (sigaction(SIGINT, &previousInterrupt, nullptr) != 0)
        return std::unexpected(StarException("Could not restore interrupt signal handler"));
      interrupted = interrupted || interruptSignalPending != 0;
    }
    handlingInterrupt = b;
    return {};
  }

  static void handleFatal(int signum) {
    // Logging, formatting and stack unwinding are not async-signal-safe.
    char const* message;
    std::size_t length;
    if (signum == SIGSEGV) {
      message = "Segfault Encountered!\n";
      length = sizeof("Segfault Encountered!\n") - 1;
    } else if (signum == SIGILL) {
      message = "Illegal Instruction Encountered!\n";
      length = sizeof("Illegal Instruction Encountered!\n") - 1;
    } else if (signum == SIGFPE) {
      message = "Floating Point Exception Encountered!\n";
      length = sizeof("Floating Point Exception Encountered!\n") - 1;
    } else {
      message = "Bus Error Encountered!\n";
      length = sizeof("Bus Error Encountered!\n") - 1;
    }
    (void)::write(STDERR_FILENO, message, length);
    std::_Exit(128 + signum);
  }

  static void handleInterrupt(int) {
    interruptSignalPending = 1;
  }
};

#endif

SignalHandler::SignalHandler() : m_impl(std::make_unique<SignalHandlerImpl>()) {}

SignalHandler::~SignalHandler() = default;

[[nodiscard]] std::expected<void, StarException> SignalHandler::setHandleFatal(bool handleFatal) {
  return m_impl->setHandleFatal(handleFatal);
}

bool SignalHandler::handlingFatal() const {
  return m_impl->handlingFatal;
}

[[nodiscard]] std::expected<void, StarException> SignalHandler::setHandleInterrupt(bool handleInterrupt) {
  return m_impl->setHandleInterrupt(handleInterrupt);
}

bool SignalHandler::handlingInterrupt() const {
  return m_impl->handlingInterrupt;
}

bool SignalHandler::interruptCaught() const {
  return m_impl->interrupted || (m_impl->handlingInterrupt && interruptSignalPending);
}

}
