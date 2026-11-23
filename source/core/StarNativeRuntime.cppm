module;
import std;
import star.format;

namespace Star {

// Entry-owned native facilities; construct before application objects and keep
// alive through their destruction and the entry point's error diagnostics.
class NativeRuntime {
  struct ConstructionKey { explicit ConstructionKey() = default; };
public:
  // GUI clients may have no inherited console. Repair only absent outputs;
  // ordinary CLI owners keep their required-output error semantics.
  enum class StandardStreams { Inherit, Optional };

  explicit NativeRuntime(ConstructionKey) noexcept {}
  [[nodiscard]] static std::expected<NativeRuntime, StarException>
  create(StandardStreams streams = StandardStreams::Inherit);
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  ~NativeRuntime();
#else
  ~NativeRuntime() = default;
#endif

  NativeRuntime(NativeRuntime const&) = delete;
  NativeRuntime& operator=(NativeRuntime const&) = delete;
  NativeRuntime(NativeRuntime&&) = delete;
  NativeRuntime& operator=(NativeRuntime&&) = delete;

private:
  [[nodiscard]] static std::expected<void, StarException> prepare_standard_streams();
};

}

#ifndef STAR_SYSTEM_FAMILY_WINDOWS
[[nodiscard]] inline std::expected<Star::NativeRuntime, Star::StarException>
Star::NativeRuntime::create(StandardStreams streams) {
  if (streams == StandardStreams::Optional) {
    auto prepared = prepare_standard_streams();
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
  }
  return std::expected<NativeRuntime, StarException>(std::in_place, ConstructionKey{});
}
#endif

export module star.native_runtime;

export namespace Star {
  using ::Star::NativeRuntime;
}
