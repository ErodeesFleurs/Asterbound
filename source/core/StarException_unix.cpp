#include <stdlib.h>
#ifdef STAR_USE_CPPTRACE
#include "cpptrace/basic.hpp"
#include "cpptrace/formatting.hpp"
#else
#include <execinfo.h>
#include <dlfcn.h>
#endif
import star.format;
import star.logging;

import std;

import star.output_proxy;
namespace Star {

// Capture object identity before a module can be unloaded, but leave symbol
// resolution to the origin payload's independently cached full rendering.
static std::function<void(std::ostream&, bool)> captureStack() {
#ifdef STAR_USE_CPPTRACE
  return [stack = cpptrace::generate_object_trace(1)](std::ostream& os, bool) {
        auto formatter = cpptrace::formatter{}
          .paths(cpptrace::formatter::path_mode::basename);
        formatter.print(os, stack.resolve());
        // The resolver may have lost access to an unloaded or removed object.
        // Keep every captured address and module-relative address regardless of
        // how many frames (including inlined frames) it can resolve later.
        os << "\nCaptured objects:";
        for (std::size_t i = 0; i < stack.frames.size(); ++i) {
          auto const& frame = stack.frames[i];
          os << strf("\n[{}] 0x{:x} {} +0x{:x}", i, frame.raw_address, frame.object_path, frame.object_address);
        }
      };
#else
  static constexpr std::size_t StackLimit = 256;
  struct Frame {
    void* address;
    std::string module;
    void* base = nullptr;
  };
  std::array<void*, StackLimit> addresses;
  auto count = backtrace(addresses.data(), addresses.size());
  std::vector<Frame> frames;
  frames.reserve(count);
  for (int i = 0; i < count; ++i) {
    Dl_info info{};
    auto& frame = frames.emplace_back(addresses[i], std::string());
    if (dladdr(addresses[i], &info)) {
      if (info.dli_fname)
        frame.module = info.dli_fname;
      frame.base = info.dli_fbase;
    }
  }
  return [frames = std::move(frames)](std::ostream& os, bool) {
        for (std::size_t i = 0; i < frames.size(); ++i) {
          auto const& frame = frames[i];
          if (i)
            os << '\n';
          os << strf("[{}] {}", i, frame.address);
          if (!frame.module.empty())
            os << strf(" {} +0x{:x}", frame.module,
                reinterpret_cast<std::uintptr_t>(frame.address) - reinterpret_cast<std::uintptr_t>(frame.base));
          Dl_info current{};
          if (dladdr(frame.address, &current) && current.dli_fbase == frame.base
              && current.dli_fname && frame.module == current.dli_fname && current.dli_sname)
            os << " " << current.dli_sname;
        }
        if (frames.size() == StackLimit)
          os << "\n[Stack Output Limit Reached]";
      };
#endif
}

static std::string captureBacktrace() {
  std::ostringstream os;
  captureStack()(os, true);
  return os.str();
}

std::function<void(std::ostream&, bool)> StarException::captureOriginStack() {
  return captureStack();
}

OutputProxy outputException(ExceptionDiagnostic const& e, bool fullStacktrace) {
  return OutputProxy([payload = e.m_payload, fullStacktrace](std::ostream& os) {
    os << payload->rendered(fullStacktrace);
  });
}

void printStack(std::string_view message) {
  EmergencyLog::info("Stack Trace ({})...\n{}", message, captureBacktrace());
}

void fatalError(std::string_view message, bool showStackTrace) {
  if (showStackTrace)
    EmergencyLog::error("Fatal Error: {}\n{}", message, captureBacktrace());
  else
    EmergencyLog::error("Fatal Error: {}", message);

  std::abort();
}

}
