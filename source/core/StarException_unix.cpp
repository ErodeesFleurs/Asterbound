#ifdef STAR_USE_CPPTRACE
#include <cpptrace/formatting.hpp>
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif

import std;
import star.native_stack;
import star.text_formatter;

namespace Star {

NativeStack NativeStack::capture() {
#ifdef STAR_USE_CPPTRACE
  return NativeStack(cpptrace::generate_object_trace(1));
#else
  std::array<void*, stack_limit> addresses;
  auto count = backtrace(addresses.data(), static_cast<int>(addresses.size()));
  NativeStack stack;
  stack.m_frames.reserve(count);
  for (int i = 0; i < count; ++i) {
    auto& frame = stack.m_frames.emplace_back(reinterpret_cast<std::uintptr_t>(addresses[i]));
    Dl_info info{};
    if (dladdr(addresses[i], &info)) {
      if (info.dli_fname)
        frame.module = info.dli_fname;
      frame.base = reinterpret_cast<std::uintptr_t>(info.dli_fbase);
    }
  }
  return stack;
#endif
}

void NativeStack::append_to(TextBuffer& output) const {
#ifdef STAR_USE_CPPTRACE
  cpptrace::formatter formatter;
  formatter.paths(cpptrace::formatter::path_mode::basename);
  // cpptrace's string API omits the newline appended by its print API.
  output.append(formatter.format(m_trace.resolve()));
  output.append("\n\nCaptured objects:");
  for (std::size_t i = 0; i < m_trace.frames.size(); ++i) {
    auto const& frame = m_trace.frames[i];
    std::format_to(std::back_inserter(output), "\n[{}] 0x{:x} {} +0x{:x}",
        i, frame.raw_address, frame.object_path, frame.object_address);
  }
#else
  for (std::size_t i = 0; i < m_frames.size(); ++i) {
    auto const& frame = m_frames[i];
    if (i)
      output.push_back('\n');
    std::format_to(std::back_inserter(output), "[{}] {:#x}", i, frame.address);
    if (!frame.module.empty())
      std::format_to(std::back_inserter(output), " {} +0x{:x}", frame.module, frame.address - frame.base);
    Dl_info current{};
    if (dladdr(reinterpret_cast<void const*>(frame.address), &current)
        && reinterpret_cast<std::uintptr_t>(current.dli_fbase) == frame.base
        && current.dli_fname && frame.module == current.dli_fname && current.dli_sname) {
      output.push_back(' ');
      output.append(current.dli_sname);
    }
  }
  if (m_frames.size() == stack_limit)
    output.append("\n[Stack Output Limit Reached]");
#endif
}

}
