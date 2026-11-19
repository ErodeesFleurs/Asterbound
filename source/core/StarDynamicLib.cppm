module;

import std;
import star.string;
import star.format;

export module star.dynamic_lib;

export namespace Star {

struct DynamicLibExceptionTag {
  static constexpr std::string_view name() { return "DynamicLibException"; }
};
using DynamicLibException = StarError<DynamicLibExceptionTag, StarException>;

template <typename Signature>
concept dynamic_function_signature = std::is_function_v<Signature> && std::is_pointer_v<std::add_pointer_t<Signature>>;

class DynamicLib;

template <dynamic_function_signature Signature>
class DynamicFunction {
public:
  // Copying also handles rvalues: every published function remains callable.
  DynamicFunction(DynamicFunction const&) noexcept = default;
  DynamicFunction& operator=(DynamicFunction const&) noexcept = default;

  template <typename... Args>
    requires std::invocable<Signature&, Args...>
  decltype(auto) operator()(Args&&... args) const noexcept(std::is_nothrow_invocable_v<Signature&, Args...>) {
    return (*m_function)(std::forward<Args>(args)...);
  }

private:
  friend class DynamicLib;
  explicit DynamicFunction(std::shared_ptr<Signature> function) noexcept : m_function(std::move(function)) {}

  std::shared_ptr<Signature> m_function;
};

class DynamicLib {
public:
  DynamicLib(DynamicLib const&) = delete;
  DynamicLib& operator=(DynamicLib const&) = delete;
  DynamicLib(DynamicLib&&) noexcept = default;
  DynamicLib& operator=(DynamicLib&&) noexcept = default;

  [[nodiscard]] static constexpr std::string_view extension() noexcept {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    return ".dll";
#elif defined(STAR_SYSTEM_MACOS)
    return ".dylib";
#else
    return ".so";
#endif
  }
  [[nodiscard]] static std::expected<DynamicLib, StarException> open(String const& path);
  [[nodiscard]] static std::expected<DynamicLib, StarException> open_base(String const& base_name);
  [[nodiscard]] static std::expected<DynamicLib, StarException> open_current();

  // Native loaders do not report types: Signature must match the exported
  // function, including its calling convention. The result pins the module.
  template <dynamic_function_signature Signature>
  [[nodiscard]] std::expected<DynamicFunction<Signature>, StarException> find(String const& name) const {
    return lookup(name).transform([](std::shared_ptr<ErasedFunction>&& function) {
      return DynamicFunction<Signature>(std::reinterpret_pointer_cast<Signature>(std::move(function)));
    });
  }

private:
  using ErasedFunction = void();
  explicit DynamicLib(std::shared_ptr<void> handle) noexcept : m_handle(std::move(handle)) {}
  [[nodiscard]] std::expected<std::shared_ptr<ErasedFunction>, StarException> lookup(String const& name) const;

  std::shared_ptr<void> m_handle;
};

}
