#include <windows.h>
#include <dbghelp.h>

import std;
import star.algorithm;
import star.signal_handler;

#include "gtest/gtest.h"

namespace {

std::atomic<bool> previous_interrupt{false};

BOOL WINAPI previous_handler(DWORD event) {
  if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT)
    return FALSE;
  previous_interrupt.store(true);
  return TRUE;
}

int console_case(DWORD event) {
  // Do not send a broadcast to the runner's console or process group.
  FreeConsole();
  if (!AllocConsole() || !SetConsoleCtrlHandler(nullptr, FALSE) || !SetConsoleCtrlHandler(previous_handler, TRUE))
    return 10;
  {
    auto owner = Star::SignalHandler::create();
    if (!owner || owner->interrupted() || !GenerateConsoleCtrlEvent(event, 0))
      return 11;
    for (auto until = GetTickCount64() + 3000; !owner->interrupted() && GetTickCount64() < until;)
      Sleep(1);
    if (!owner->interrupted() || !owner->interrupted() || previous_interrupt.load())
      return 12;
  }
  if (!GenerateConsoleCtrlEvent(event, 0))
    return 13;
  for (auto until = GetTickCount64() + 3000; !previous_interrupt.load() && GetTickCount64() < until;)
    Sleep(1);
  return previous_interrupt.load() ? 0 : 14;
}

TEST(WindowsSignalDeathTest, CatchesControlCAndBreakAndRestoresPredecessor) {
  for (auto event : {CTRL_C_EVENT, CTRL_BREAK_EVENT}) {
    SCOPED_TRACE(event);
    EXPECT_EXIT(std::_Exit(console_case(event)), ::testing::ExitedWithCode(0), "");
  }
}

// The condition is runtime data, and the post-call volatile access prevents a
// tail call. This is actual guard-page exhaustion, not RaiseException with an
// overflow status on an otherwise healthy stack.
[[clang::noinline]] unsigned exhaust_stack(unsigned depth, volatile unsigned const& stop) {
  volatile unsigned slot = depth;
  if (depth == stop)
    return slot;
  return exhaust_stack(depth + 1, stop) + slot;
}

constexpr wchar_t directory_key[] = L"ASTERBOUND_TEST_SIGNAL_CHILD_CWD";

void fault_child(bool overflow) {
  std::array<wchar_t, 32768> dir;
  auto size = GetEnvironmentVariableW(directory_key, dir.data(), dir.size());
  if (!size || size >= dir.size() || !SetCurrentDirectoryW(dir.data()))
    std::_Exit(20);
  auto owner = Star::SignalHandler::create();
  if (!owner)
    std::_Exit(21);
  {
    std::ofstream thread_file("fault-thread.txt");
    thread_file << GetCurrentThreadId();
    thread_file.close();
    if (!thread_file)
      std::_Exit(23);
  }
  if (overflow) {
    volatile unsigned stop = UINT_MAX;
    std::_Exit(static_cast<int>(exhaust_stack(1, stop)));
  }
  RaiseException(EXCEPTION_INT_DIVIDE_BY_ZERO, EXCEPTION_NONCONTINUABLE, 0, nullptr);
  std::_Exit(22);
}

void check_fault(bool overflow, bool block_dump) {
  // GTest re-executes the setup in its Windows child. Propagate only our
  // freshly created directory, never write into a caller-supplied env path.
  std::array<wchar_t, 32768> previous{};
  auto size = GetEnvironmentVariableW(directory_key, previous.data(), previous.size());
  ASSERT_LT(size, previous.size());
  auto dir = std::filesystem::temp_directory_path()
      / std::format("asterbound-signal-{}-{}", GetCurrentProcessId(), GetTickCount64());
  ASSERT_TRUE(std::filesystem::create_directory(dir));
  auto release = [&] {
    SetEnvironmentVariableW(directory_key, size ? previous.data() : nullptr);
    std::error_code error;
    std::filesystem::remove_all(dir, error);
    EXPECT_FALSE(error) << error.message();
  };
  auto cleanup = Star::make_scope_exit(release);
  ASSERT_TRUE(SetEnvironmentVariableW(directory_key, dir.c_str()));
  if (block_dump)
    ASSERT_TRUE(std::filesystem::create_directory(dir / L"starbound.dmp"));
  auto code = overflow ? EXCEPTION_STACK_OVERFLOW : EXCEPTION_INT_DIVIDE_BY_ZERO;
  EXPECT_EXIT({ release(); fault_child(overflow); }, ::testing::ExitedWithCode(static_cast<int>(code)),
      block_dump ? "minidump failed" : "wrote starbound.dmp");
  if (!block_dump) {
    std::ifstream stream(dir / L"starbound.dmp", std::ios::binary);
    MINIDUMP_HEADER header{};
    ASSERT_TRUE(stream.read(reinterpret_cast<char*>(&header), sizeof(header)));
    ASSERT_EQ(header.Signature, static_cast<ULONG32>(MINIDUMP_SIGNATURE));
    stream.seekg(header.StreamDirectoryRva);
    for (ULONG32 i = 0; i != header.NumberOfStreams; ++i) {
      MINIDUMP_DIRECTORY entry{};
      ASSERT_TRUE(stream.read(reinterpret_cast<char*>(&entry), sizeof(entry)));
      if (entry.StreamType == ExceptionStream) {
        MINIDUMP_EXCEPTION_STREAM exception{};
        stream.seekg(entry.Location.Rva);
        ASSERT_TRUE(stream.read(reinterpret_cast<char*>(&exception), sizeof(exception)));
        DWORD fault_thread = 0;
        std::ifstream thread_file(dir / L"fault-thread.txt");
        ASSERT_TRUE(thread_file >> fault_thread);
        EXPECT_EQ(exception.ThreadId, fault_thread);
        EXPECT_EQ(exception.ExceptionRecord.ExceptionCode, code);
        return;
      }
    }
    FAIL() << "Missing exception stream";
  }
}

TEST(WindowsSignalDeathTest, StackExhaustionWritesAnUnattendedDump) { check_fault(true, false); }
TEST(WindowsSignalDeathTest, NativeFaultWritesAnUnattendedDump) { check_fault(false, false); }
TEST(WindowsSignalDeathTest, CannotOpenDumpStillTerminatesWithoutClaimingSuccess) { check_fault(false, true); }

}
