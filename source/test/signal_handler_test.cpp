#include <pthread.h>
#include <signal.h>

import std;
import star.signal_handler;

#include "gtest/gtest.h"

namespace {
using Star::SignalHandler;

static_assert(std::is_nothrow_move_constructible_v<SignalHandler>);
static_assert(!std::is_copy_constructible_v<SignalHandler>);
static_assert(!std::is_move_assignable_v<SignalHandler>);

constexpr std::array handled_signals{SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGINT};
volatile sig_atomic_t restored_signal = 0;

void previous_handler(int signal_number) noexcept {
  restored_signal = signal_number;
}

bool unblock_signals() {
  sigset_t mask;
  if (sigemptyset(&mask) != 0)
    return false;
  for (int signal_number : handled_signals)
    if (sigaddset(&mask, signal_number) != 0)
      return false;
  return ::pthread_sigmask(SIG_UNBLOCK, &mask, nullptr) == 0;
}

int exercise_owner() {
  struct sigaction original{};
  original.sa_handler = previous_handler;
  original.sa_flags = SA_RESTART;
  if (!unblock_signals() || sigemptyset(&original.sa_mask) != 0 || sigaddset(&original.sa_mask, SIGUSR1) != 0)
    return 1;
  for (int signal_number : handled_signals)
    if (::sigaction(signal_number, &original, nullptr) != 0)
      return 2;
  {
    std::optional<SignalHandler> owner;
    {
      auto result = SignalHandler::create();
      if (!result)
        return 3;
      owner.emplace(std::move(*result));
      if (owner->interrupted())
        return 4;
      int delivered = -1;
      std::thread worker([&] { delivered = ::raise(SIGINT); });
      worker.join();
      if (delivered != 0 || !owner->interrupted() || result->interrupted())
        return 5;
    }
    if (SignalHandler::create() || !owner->interrupted() || restored_signal != 0)
      return 6;
  }
  if (SignalHandler::create())
    return 9;
  for (int signal_number : handled_signals) {
    struct sigaction restored{};
    if (::sigaction(signal_number, nullptr, &restored) != 0 || restored.sa_handler != previous_handler
        || !(restored.sa_flags & SA_RESTART) || sigismember(&restored.sa_mask, SIGUSR1) != 1)
      return 7;
    if (::raise(signal_number) != 0 || restored_signal != signal_number)
      return 8;
  }
  return 0;
}

TEST(SignalHandlerDeathTest, ExclusiveMovedOwnerCapturesStickyInterruptAndRestoresDispositions) {
  EXPECT_EXIT(std::_Exit(exercise_owner()), ::testing::ExitedWithCode(0), "");
}

int raise_fatal(int signal_number) {
  auto owner = SignalHandler::create();
  if (!owner || !unblock_signals())
    return 1;
  ::raise(signal_number);
  return 2;
}

TEST(SignalHandlerDeathTest, FatalSignalsWriteDiagnosticAndExitWithSignalStatus) {
  struct FatalCase {
    int signal_number;
    std::string diagnostic;
  };
  for (auto const& fatal : {FatalCase{SIGSEGV, "Segfault Encountered!\n"}, {SIGILL, "Illegal Instruction Encountered!\n"},
           {SIGFPE, "Floating Point Exception Encountered!\n"}, {SIGBUS, "Bus Error Encountered!\n"}}) {
    SCOPED_TRACE(fatal.signal_number);
    EXPECT_EXIT(std::_Exit(raise_fatal(fatal.signal_number)), ::testing::ExitedWithCode(128 + fatal.signal_number), fatal.diagnostic);
  }
}
}
