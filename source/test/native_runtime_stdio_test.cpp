#include <errno.h>
#include <stdio.h>
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

import std;
import star.algorithm;
import star.file;
import star.format;
import star.logging;
import star.native_runtime;
import star.random;
import star.string;

#include "gtest/gtest.h"

using namespace Star;

#if GTEST_HAS_DEATH_TEST
namespace {

// All descriptor mutations happen after EXPECT_EXIT enters its child. In
// threadsafe mode even NativeRuntime's initial core_tests_main startup has
// already run before these helpers explicitly exercise the selected policy.
void prepareChild() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // A closed CRT descriptor must report a checked I/O error, not invoke the
  // process-wide default invalid-parameter termination policy.
  _set_invalid_parameter_handler([](wchar_t const*, wchar_t const*, wchar_t const*, unsigned, uintptr_t) {});
#endif
}

int descriptor(FILE* stream) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return _fileno(stream);
#else
  return fileno(stream);
#endif
}

bool closeDescriptor(int fd) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return _close(fd) == 0;
#else
  return close(fd) == 0;
#endif
}

bool duplicateTo(int from, int to) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return _dup2(from, to) == 0;
#else
  return dup2(from, to) == to;
#endif
}

bool removeOutput(FILE* stream) {
  if (fflush(stream) != 0 || !closeDescriptor(descriptor(stream)))
    return false;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  if (!SetStdHandle(stream == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE, nullptr))
    return false;
#endif
  return true;
}

bool checkedFailure(std::expected<void, StarException> const& result) {
  return !result && result.error().is<IOException>();
}

int repairAbsent(unsigned missing) {
  prepareChild();
  if ((missing & 1) && !removeOutput(stdout))
    return 10;
  if ((missing & 2) && !removeOutput(stderr))
    return 11;
  // Repair must also clear the FILE error state left by an earlier attempt.
  if ((missing & 1) && !checkedFailure(Star::print(stdout, "before repair")))
    return 12;
  if ((missing & 2) && !checkedFailure(Star::print(stderr, "before repair")))
    return 13;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  _set_invalid_parameter_handler(nullptr);
#endif
  auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
  if (!runtime)
    return 14;
  if ((missing & 1) && !Star::print(stdout, "after repair\n"))
    return 15;
  if ((missing & 2) && !Star::print(stderr, "after repair\n"))
    return 16;
  Logger logger;
  if (!logger.info("normal logger after repair"))
    return 17;
  return 0;
}

int inheritAbsent() {
  prepareChild();
  if (!removeOutput(stdout) || !removeOutput(stderr))
    return 20;
  auto runtime = NativeRuntime::create();
  if (!runtime)
    return 21;
  if (!checkedFailure(Star::print(stdout, "required CLI output"))
      || !checkedFailure(Star::print(stderr, "required CLI diagnostic")))
    return 22;
  Logger logger;
  if (!checkedFailure(logger.info("required CLI log")))
    return 23;
  return 0;
}

int redirectedFile() {
  prepareChild();
  std::unique_ptr<FILE, decltype(&fclose)> output(tmpfile(), &fclose);
  std::unique_ptr<FILE, decltype(&fclose)> error(tmpfile(), &fclose);
  if (!output || !error || !duplicateTo(descriptor(output.get()), descriptor(stdout))
      || !duplicateTo(descriptor(error.get()), descriptor(stderr)))
    return 30;
  auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
  if (!runtime)
    return 31;
  constexpr std::string_view payload("redirected\0stdout\n", 18);
  if (!Star::print(stdout, "{}", payload) || !Star::print(stderr, "redirected stderr\n"))
    return 32;
  auto exactContents = [](FILE* file, std::string_view expected) {
    if (fseek(file, 0, SEEK_SET) != 0)
      return false;
    std::array<char, 128> bytes{};
    auto size = fread(bytes.data(), 1, bytes.size(), file);
    return !ferror(file) && feof(file) && std::string_view(bytes.data(), size) == expected;
  };
  if (!exactContents(output.get(), payload) || !exactContents(error.get(), "redirected stderr\n"))
    return 34;
  return 0;
}

int redirectedPipe(unsigned missing) {
  prepareChild();
  int pipeFds[2];
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  if (_pipe(pipeFds, 4096, _O_BINARY) != 0)
#else
  if (pipe(pipeFds) != 0)
#endif
    return 40;
  auto cleanup = finally([&] {
    if (pipeFds[0] >= 0)
      closeDescriptor(pipeFds[0]);
    if (pipeFds[1] >= 0)
      closeDescriptor(pipeFds[1]);
  });
  FILE* preserved = missing == 1 ? stderr : stdout;
  FILE* absent = missing == 1 ? stdout : stderr;
  if (!duplicateTo(pipeFds[1], descriptor(preserved)) || !removeOutput(absent))
    return 41;
  auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
  if (!runtime || !Star::print(absent, "discarded output\n"))
    return 42;
  constexpr std::string_view payload("pipe\0bytes\n", 11);
  if (!Star::print(preserved, "{}", payload))
    return 43;
  // Close every writer before reading, so a missing/truncated write fails at
  // EOF rather than hanging a read waiting for bytes that will never arrive.
  if (!closeDescriptor(descriptor(preserved)) || !closeDescriptor(pipeFds[1]))
    return 44;
  pipeFds[1] = -1;
  std::array<char, 64> bytes{};
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto count = _read(pipeFds[0], bytes.data(), static_cast<unsigned>(bytes.size()));
#else
  auto count = read(pipeFds[0], bytes.data(), bytes.size());
#endif
  if (count != static_cast<decltype(count)>(payload.size())
      || std::string_view(bytes.data(), payload.size()) != payload)
    return 45;
  return 0;
}

int fileLoggerWithoutConsole() {
  prepareChild();
  auto random = RandomService::create();
  if (!random)
    return 50;
  auto directory = File::temporary_directory(**random);
  if (!directory)
    return 51;
  auto cleanup = finally([&] { (void)File::remove_all(*directory); });
  auto path = File::relative_to(*directory, "without-console.log");
  if (!removeOutput(stdout) || !removeOutput(stderr))
    return 52;
  auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
  if (!runtime)
    return 53;
  auto sink = FileLogSink::create(path, LogLevel::Info, true);
  if (!sink)
    return 54;
  Logger logger;// Keep its default stdout sink: removing it hides the bug.
  logger.add_sink(*sink);
  if (!logger.info("first file marker") || !logger.error("second file marker"))
    return 55;
  auto contents = File::read_file_string(path);
  if (!contents)
    return 56;
  auto const& bytes = contents->utf8();
  auto first = bytes.find("first file marker\n");
  auto second = bytes.find("second file marker\n");
  if (first == std::string::npos || second == std::string::npos || first >= second)
    return 58;
  return 0;
}

int configuredErrors(char const* device, char const* mode) {
  prepareChild();
  if (!freopen(device, mode, stdout) || !freopen(device, mode, stderr))
    return 60;
  auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
  if (!runtime)
    return 61;
  if (!checkedFailure(Star::print(stdout, "must not be discarded"))
      || !checkedFailure(Star::print(stderr, "must not be discarded")))
    return 62;
  Logger logger;
  if (!checkedFailure(logger.error("normal logger must remain checked")))
    return 63;
  // A diagnostic failure must return to the caller's error handling instead
  // of terminating before that code can choose its own outcome.
  EmergencyLog::error("original emergency diagnostic after checked failure");
  if (!checkedFailure(Star::print(stderr, "still a real write error")))
    return 64;
  return 0;
}

int emergencyWithoutStderr() {
  prepareChild();
  if (!removeOutput(stderr) || !checkedFailure(Star::print(stderr, "missing stderr")))
    return 70;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  _set_invalid_parameter_handler(nullptr);
#endif
  EmergencyLog::info("startup must continue without a console");
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  prepareChild();
#endif
  bool errorHandled = false;
  auto result = Star::print(stderr, "caller handles this error");
  if (checkedFailure(result)) {
    EmergencyLog::error("caller is reporting the original checked output failure");
    errorHandled = true;
  }
  return errorHandled ? 0 : 71;
}

class NativeRuntimeStdioDeathTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_previousStyle = testing::GTEST_FLAG(death_test_style);
    testing::GTEST_FLAG(death_test_style) = "threadsafe";
  }
  void TearDown() override { testing::GTEST_FLAG(death_test_style) = m_previousStyle; }

private:
  std::string m_previousStyle;
};

TEST_F(NativeRuntimeStdioDeathTest, OptionalRepairsEitherOrBothAbsentOutputsAndTheirErrorState) {
  for (unsigned missing : {1u, 2u, 3u}) {
    SCOPED_TRACE(missing);
    EXPECT_EXIT(std::_Exit(repairAbsent(missing)), ::testing::ExitedWithCode(0), "");
  }
}

TEST_F(NativeRuntimeStdioDeathTest, InheritKeepsRequiredCliWritesChecked) {
  EXPECT_EXIT(std::_Exit(inheritAbsent()), ::testing::ExitedWithCode(0), "");
}

TEST_F(NativeRuntimeStdioDeathTest, OptionalPreservesExactRedirectedFileBytes) {
  EXPECT_EXIT(std::_Exit(redirectedFile()), ::testing::ExitedWithCode(0), "");
}

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
TEST_F(NativeRuntimeStdioDeathTest, OptionalPreservesCrtOutputWhenNativeSlotIsUnset) {
  EXPECT_EXIT(([] {
                prepareChild();
                std::unique_ptr<FILE, decltype(&fclose)> output(tmpfile(), &fclose);
                if (!output || !duplicateTo(descriptor(output.get()), descriptor(stdout))
                    || !SetStdHandle(STD_OUTPUT_HANDLE, nullptr))
                  std::_Exit(95);
                auto runtime = NativeRuntime::create(NativeRuntime::StandardStreams::Optional);
                constexpr std::string_view marker = "CRT-only output\n";
                if (!runtime || !Star::print(stdout, "{}", marker) || fseek(output.get(), 0, SEEK_SET) != 0)
                  std::_Exit(96);
                std::array<char, 32> bytes{};
                auto count = fread(bytes.data(), 1, bytes.size(), output.get());
                std::_Exit(
                  !ferror(output.get()) && feof(output.get()) && std::string_view(bytes.data(), count) == marker ? 0
                                                                                                                 : 97);
              }()),
              ::testing::ExitedWithCode(0), "");
}
#endif

TEST_F(NativeRuntimeStdioDeathTest, RepairingOneOutputPreservesOtherRedirectedPipe) {
  for (unsigned missing : {1u, 2u}) {
    SCOPED_TRACE(missing);
    EXPECT_EXIT(std::_Exit(redirectedPipe(missing)), ::testing::ExitedWithCode(0), "");
  }
}

TEST_F(NativeRuntimeStdioDeathTest, OptionalAllowsNormalLoggerToWriteRealFileWithoutConsole) {
  EXPECT_EXIT(std::_Exit(fileLoggerWithoutConsole()), ::testing::ExitedWithCode(0), "");
}

TEST_F(NativeRuntimeStdioDeathTest, OptionalDoesNotRepairConfiguredReadOnlyStreams) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  EXPECT_EXIT(std::_Exit(configuredErrors("NUL", "rb")), ::testing::ExitedWithCode(0), "");
#else
  EXPECT_EXIT(std::_Exit(configuredErrors("/dev/null", "r")), ::testing::ExitedWithCode(0), "");
#endif
}

TEST_F(NativeRuntimeStdioDeathTest, EmergencyFailureReturnsToCallerWithoutRepairingStderr) {
  EXPECT_EXIT(std::_Exit(emergencyWithoutStderr()), ::testing::ExitedWithCode(0), "");
}

#ifndef STAR_SYSTEM_FAMILY_WINDOWS
TEST_F(NativeRuntimeStdioDeathTest, OptionalDoesNotReuseClosedInputAsRepairedOutput) {
  EXPECT_EXIT(
    {
      if (close(STDIN_FILENO) != 0)
        std::_Exit(80);
      auto result = repairAbsent(3);
      if (result != 0)
        std::_Exit(result);
      errno = 0;
      std::_Exit(fcntl(STDIN_FILENO, F_GETFD) == -1 && errno == EBADF ? 0 : 81);
    },
    ::testing::ExitedWithCode(0), "");
}
#endif

#ifdef STAR_SYSTEM_LINUX
TEST_F(NativeRuntimeStdioDeathTest, OptionalPreservesFullDeviceErrorsAndEmergencyContinuation) {
  EXPECT_EXIT(std::_Exit(configuredErrors("/dev/full", "w")), ::testing::ExitedWithCode(0), "");
}

TEST_F(NativeRuntimeStdioDeathTest, RealFileLogSinkStillReportsFullDeviceWriteFailure) {
  EXPECT_EXIT(
    {
      auto sink = FileLogSink::create("/dev/full", LogLevel::Info, false);
      if (!sink)
        std::_Exit(90);
      std::_Exit(checkedFailure((*sink)->log("must report ENOSPC", LogLevel::Info)) ? 0 : 91);
    },
    ::testing::ExitedWithCode(0), "");
}
#endif

}// namespace
#endif
