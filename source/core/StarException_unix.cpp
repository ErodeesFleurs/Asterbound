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
import star.text_formatter;

import std;

import star.output_proxy;
namespace Star {

// Capture object identity before a module can be unloaded, but leave symbol
// resolution to the origin payload's independently cached full rendering.
static std::function<void(TextBuffer&, bool)> capture_stack() {
#ifdef STAR_USE_CPPTRACE
  return [stack = cpptrace::generate_object_trace(1)](TextBuffer& buffer, bool) {
        auto formatter = cpptrace::formatter{}
          .paths(cpptrace::formatter::path_mode::basename);
        // cpptrace's string API omits the newline appended by its print API.
        buffer.append(formatter.format(stack.resolve()));
        buffer.push_back('\n');
        // The resolver may have lost access to an unloaded or removed object.
        // Keep every captured address and module-relative address regardless of
        // how many frames (including inlined frames) it can resolve later.
        buffer.append("\nCaptured objects:");
        for (std::size_t i = 0; i < stack.frames.size(); ++i) {
          auto const& frame = stack.frames[i];
          std::format_to(std::back_inserter(buffer), "\n[{}] 0x{:x} {} +0x{:x}",
              i, frame.raw_address, frame.object_path, frame.object_address);
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
  return [frames = std::move(frames)](TextBuffer& buffer, bool) {
        for (std::size_t i = 0; i < frames.size(); ++i) {
          auto const& frame = frames[i];
          if (i)
            buffer.push_back('\n');
          std::format_to(std::back_inserter(buffer), "[{}] {}", i, frame.address);
          if (!frame.module.empty())
            std::format_to(std::back_inserter(buffer), " {} +0x{:x}", frame.module,
                reinterpret_cast<std::uintptr_t>(frame.address) - reinterpret_cast<std::uintptr_t>(frame.base));
          Dl_info current{};
          if (dladdr(frame.address, &current) && current.dli_fbase == frame.base
              && current.dli_fname && frame.module == current.dli_fname && current.dli_sname) {
            buffer.push_back(' ');
            buffer.append(current.dli_sname);
          }
        }
        if (frames.size() == StackLimit)
          buffer.append("\n[Stack Output Limit Reached]");
      };
#endif
}

static std::string capture_backtrace() {
  TextBuffer buffer;
  capture_stack()(buffer, true);
  return std::move(buffer).str();
}

std::function<void(TextBuffer&, bool)> StarException::capture_origin_stack() {
  return capture_stack();
}

OutputProxy output_exception(ExceptionDiagnostic const& e, bool fullStacktrace) {
  return OutputProxy([payload = e.m_payload, fullStacktrace](TextBuffer& buffer) {
    buffer.append(payload->rendered(fullStacktrace));
  });
}

void print_stack(std::string_view message) {
  EmergencyLog::info("Stack Trace ({})...\n{}", message, capture_backtrace());
}

void fatal_error(std::string_view message, bool showStackTrace) {
  if (showStackTrace)
    EmergencyLog::error("Fatal Error: {}\n{}", message, capture_backtrace());
  else
    EmergencyLog::error("Fatal Error: {}", message);

  std::abort();
}

}
