module;
#ifdef STAR_USE_CPPTRACE
#include <cpptrace/basic.hpp>
#endif

import std;
import star.text_formatter;

namespace Star {

// Addresses and module identities belong to the capture site. Only symbol
// resolution is deferred; this value has no diagnostic or rendering cache.
class NativeStack {
public:
  static NativeStack capture();
  void append_to(TextBuffer& output) const;

private:
#ifdef STAR_USE_CPPTRACE
  explicit NativeStack(cpptrace::object_trace&& trace) : m_trace(std::move(trace)) {}
  cpptrace::object_trace m_trace;
#else
  static constexpr std::size_t stack_limit = 256;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  struct Module {
    std::uintptr_t base;
    std::uint32_t image_size;
    std::uint32_t timestamp;
    std::wstring path;

    bool operator==(Module const&) const = default;
  };
  static Module describe_module(std::uintptr_t base);
  std::vector<Module> m_modules;
  struct Frame {
    std::uintptr_t address;
    std::size_t module;
  };
#else
  struct Frame {
    std::uintptr_t address;
    std::string module;
    std::uintptr_t base = 0;
  };
#endif
  std::vector<Frame> m_frames;
#endif
};

}

export module star.native_stack;

export namespace Star {
using ::Star::NativeStack;
}
