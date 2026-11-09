#include <errno.h>
#include <stdio.h>
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include "StarWindowsCrt.hpp"
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

import std;
import star.native_runtime;
import star.format;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.algorithm;
#endif

namespace Star {
namespace {

std::expected<void, StarException> prepare_optional_output(FILE* stream) {
  auto name = stream == stdout ? "stdout" : "stderr";
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto slot = stream == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
  auto inherited = GetStdHandle(slot);
  auto descriptor = ::_fileno(stream);
  if (descriptor >= 0 && inherited && inherited != INVALID_HANDLE_VALUE)
    return {};
  // A valid CRT redirection can outlive its Win32 standard-handle slot.
  // A missing descriptor is recoverable, including while freopen closes it;
  // confine that CRT policy to this thread and this recovery operation.
  auto previous = ::_set_thread_local_invalid_parameter_handler(
    [](wchar_t const*, wchar_t const*, wchar_t const*, unsigned, std::uintptr_t) {});
  auto restore = finally([previous] { ::_set_thread_local_invalid_parameter_handler(previous); });
  if (descriptor >= 0) {
    auto existing = ::_get_osfhandle(descriptor);
    if (existing != -1 && existing != -2)
      return {};
  }

  if (!std::freopen("NUL", "w", stream))
    return std::unexpected(IOException::format("Could not prepare optional {}: {}", name, std::strerror(errno)));
  auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(::_fileno(stream)));
  if (!SetStdHandle(slot, handle))
    return std::unexpected(
      IOException::format("Could not publish optional {}: Windows error {}", name, GetLastError()));
#else
  auto descriptor = ::fileno(stream);
  if (descriptor < 0)
    return std::unexpected(IOException::format("Could not inspect optional {}: {}", name, std::strerror(errno)));
  if (::fcntl(descriptor, F_GETFD) != -1)
    return {};
  if (errno != EBADF)
    return std::unexpected(IOException::format("Could not inspect optional {}: {}", name, std::strerror(errno)));

  // Reserve the original descriptor before native initialization can reuse it
  // for an unrelated file. Do not replace a valid pipe, file, or /dev/null.
  auto replacement = ::open("/dev/null", O_WRONLY);
  if (replacement < 0)
    return std::unexpected(IOException::format("Could not prepare optional {}: {}", name, std::strerror(errno)));
  if (replacement != descriptor) {
    if (::dup2(replacement, descriptor) < 0) {
      auto error = errno;
      (void)::close(replacement);
      return std::unexpected(IOException::format("Could not prepare optional {}: {}", name, std::strerror(error)));
    }
    if (::close(replacement) < 0)
      return std::unexpected(
        IOException::format("Could not release optional {} descriptor: {}", name, std::strerror(errno)));
  }
  std::clearerr(stream);
#endif
  return {};
}

}// namespace

std::expected<void, StarException> NativeRuntime::prepare_standard_streams() {
  auto output = prepare_optional_output(stdout);
  if (!output)
    return std::unexpected(std::move(output.error()));
  return prepare_optional_output(stderr);
}

}// namespace Star
