module;

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
import std;
import star.string;
import star.format;

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string_windows;
#endif

export module star.dynamic_lib;

export namespace Star {

struct DynamicLibExceptionTag {
  static constexpr std::string_view name() { return "DynamicLibException"; }
};
using DynamicLibException = StarError<DynamicLibExceptionTag, StarException>;

class DynamicLib {
public:
  // Includes '.', e.g. '.dll', '.so', '.dylib'.
  static String libraryExtension();
  // Loading and symbol lookup preserve the native loader diagnostic.
  [[nodiscard]] static std::expected<std::unique_ptr<DynamicLib>, StarException> loadLibrary(String const& fileName);
  [[nodiscard]] static std::expected<std::unique_ptr<DynamicLib>, StarException> loadLibraryBase(String const& baseName);
  [[nodiscard]] static std::expected<std::unique_ptr<DynamicLib>, StarException> currentExecutable();
  virtual ~DynamicLib() = default;
  [[nodiscard]] virtual std::expected<void*, StarException> funcPtr(String const& name) = 0;
};

[[nodiscard]] inline std::expected<std::unique_ptr<DynamicLib>, StarException> DynamicLib::loadLibraryBase(String const& baseName) {
  return loadLibrary(baseName + libraryExtension());
}

}

namespace Star {

#ifdef STAR_SYSTEM_FAMILY_WINDOWS

class PrivateDynLib : public DynamicLib {
public:
  PrivateDynLib(HMODULE handle, bool owned)
    : m_handle(handle), m_owned(owned) {}

  ~PrivateDynLib() override {
    // GetModuleHandleW returns a borrowed executable handle, not a reference
    // acquired by LoadLibraryW. Only release references we actually own.
    if (m_owned && !FreeLibrary(m_handle)) {
      auto status = GetLastError();
      fprintf(stderr, "Failed to unload dynamic library (Windows error %lu)\n", static_cast<unsigned long>(status));
    }
  }

  [[nodiscard]] std::expected<void*, StarException> funcPtr(String const& name) override {
    auto symbol = GetProcAddress(m_handle, name.utf8_data());
    if (!symbol) {
      auto status = GetLastError();
      return std::unexpected(DynamicLibException::format("Cannot find dynamic library symbol '{}' (Windows error {})", name, status));
    }
    return reinterpret_cast<void*>(symbol);
  }

private:
  HMODULE m_handle;
  bool m_owned;
};

String DynamicLib::libraryExtension() {
  return ".dll";
}

std::expected<std::unique_ptr<DynamicLib>, StarException> DynamicLib::loadLibrary(String const& libraryName) {
  auto wideName = stringToUtf16(libraryName);
  if (!wideName)
    return std::unexpected(std::move(wideName.error()));
  auto handle = LoadLibraryW(wideName->get());
  if (!handle) {
    auto status = GetLastError();
    return std::unexpected(DynamicLibException::format("Cannot load dynamic library '{}' (Windows error {})", libraryName, status));
  }
  return std::make_unique<PrivateDynLib>(handle, true);
}

std::expected<std::unique_ptr<DynamicLib>, StarException> DynamicLib::currentExecutable() {
  auto handle = GetModuleHandleW(nullptr);
  if (!handle) {
    auto status = GetLastError();
    return std::unexpected(DynamicLibException::format("Cannot obtain current executable (Windows error {})", status));
  }
  return std::make_unique<PrivateDynLib>(handle, false);
}

#else

class PrivateDynLib : public DynamicLib {
public:
  explicit PrivateDynLib(void* handle)
    : m_handle(handle) {}

  ~PrivateDynLib() override {
    if (dlclose(m_handle)) {
      auto detail = dlerror();
      fprintf(stderr, "Failed to unload dynamic library: %s\n", detail ? detail : "no loader diagnostic");
    }
  }

  [[nodiscard]] std::expected<void*, StarException> funcPtr(String const& name) override {
    // A symbol may legally have a null address. Only dlerror distinguishes it
    // from a failed lookup, and stale loader diagnostics must be cleared first.
    dlerror();
    auto symbol = dlsym(m_handle, name.utf8_data());
    if (auto detail = dlerror())
      return std::unexpected(DynamicLibException::format("Cannot find dynamic library symbol '{}': {}", name, detail));
    return symbol;
  }

private:
  void* m_handle;
};

String DynamicLib::libraryExtension() {
#ifdef STAR_SYSTEM_MACOS
  return ".dylib";
#else
  return ".so";
#endif
}

std::expected<std::unique_ptr<DynamicLib>, StarException> DynamicLib::loadLibrary(String const& libraryName) {
  auto handle = dlopen(libraryName.utf8_data(), RTLD_NOW);
  if (!handle) {
    auto detail = dlerror();
    return std::unexpected(DynamicLibException::format("Cannot load dynamic library '{}': {}", libraryName, detail ? detail : "no loader diagnostic"));
  }
  return std::make_unique<PrivateDynLib>(handle);
}

std::expected<std::unique_ptr<DynamicLib>, StarException> DynamicLib::currentExecutable() {
  auto handle = dlopen(nullptr, RTLD_NOW);
  if (!handle) {
    auto detail = dlerror();
    return std::unexpected(DynamicLibException::format("Cannot obtain current executable: {}", detail ? detail : "no loader diagnostic"));
  }
  return std::make_unique<PrivateDynLib>(handle);
}

#endif

}
