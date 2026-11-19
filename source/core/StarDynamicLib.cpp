module;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
import std;
import star.format;
import star.string;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string_windows;
#endif

module star.dynamic_lib;

namespace Star {
namespace {

#ifdef STAR_SYSTEM_FAMILY_WINDOWS
struct CloseModule {
  void operator()(HMODULE handle) const noexcept {
    if (!FreeLibrary(handle)) {
      auto status = GetLastError();
      if (!Star::print(stderr, "Failed to unload dynamic library (Windows error {})\n", status))
        std::_Exit(1);
    }
  }
};
#else
struct CloseModule {
  void operator()(void* handle) const noexcept {
    if (dlclose(handle)) {
      auto detail = dlerror();
      if (!Star::print(stderr, "Failed to unload dynamic library: {}\n", detail ? detail : "no loader diagnostic"))
        std::_Exit(1);
    }
  }
};
#endif

}

std::expected<DynamicLib, StarException> DynamicLib::open(String const& path) {
  if (path.utf8().contains('\0'))
    return std::unexpected(DynamicLibException("Cannot load dynamic library: path contains an embedded NUL"));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto wide_name = stringToUtf16(path);
  if (!wide_name)
    return std::unexpected(std::move(wide_name.error()));
  auto handle = LoadLibraryW(wide_name->get());
  if (!handle) {
    auto status = GetLastError();
    return std::unexpected(DynamicLibException::format("Cannot load dynamic library '{}' (Windows error {})", path, status));
  }
#else
  auto handle = dlopen(path.utf8_data(), RTLD_NOW);
  if (!handle) {
    auto detail = dlerror();
    return std::unexpected(DynamicLibException::format("Cannot load dynamic library '{}': {}", path, detail ? detail : "no loader diagnostic"));
  }
#endif
  return DynamicLib(std::shared_ptr<void>(handle, CloseModule{}));
}

std::expected<DynamicLib, StarException> DynamicLib::open_base(String const& base_name) {
  auto const& base = base_name.utf8();
  std::string path;
  path.reserve(base.size() + extension().size());
  path.append(base).append(extension());
  return open(String(std::move(path)));
}

std::expected<DynamicLib, StarException> DynamicLib::open_current() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto handle = GetModuleHandleW(nullptr);
  if (!handle) {
    auto status = GetLastError();
    return std::unexpected(DynamicLibException::format("Cannot obtain current executable (Windows error {})", status));
  }
  // The process image is permanently resident and GetModuleHandleW does not
  // acquire a reference. An empty owner aliases it without a deleter or allocation.
  return DynamicLib(std::shared_ptr<void>(std::shared_ptr<void>{}, handle));
#else
  auto handle = dlopen(nullptr, RTLD_NOW);
  if (!handle) {
    auto detail = dlerror();
    return std::unexpected(DynamicLibException::format("Cannot obtain current executable: {}", detail ? detail : "no loader diagnostic"));
  }
  return DynamicLib(std::shared_ptr<void>(handle, CloseModule{}));
#endif
}

std::expected<std::shared_ptr<DynamicLib::ErasedFunction>, StarException> DynamicLib::lookup(String const& name) const {
  if (!m_handle)
    return std::unexpected(DynamicLibException::format("Cannot find dynamic library symbol '{}': library is not open", name));
  if (name.utf8().contains('\0'))
    return std::unexpected(DynamicLibException("Cannot find dynamic library symbol: name contains an embedded NUL"));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto symbol = GetProcAddress(static_cast<HMODULE>(m_handle.get()), name.utf8_data());
  if (!symbol) {
    auto status = GetLastError();
    return std::unexpected(DynamicLibException::format("Cannot find dynamic library symbol '{}' (Windows error {})", name, status));
  }
#else
  // A null address is legal for dlsym. Distinguish a loader failure from a
  // successful resolution which cannot yield a callable function.
  dlerror();
  auto symbol = dlsym(m_handle.get(), name.utf8_data());
  if (auto detail = dlerror())
    return std::unexpected(DynamicLibException::format("Cannot find dynamic library symbol '{}': {}", name, detail));
  if (!symbol)
    return std::unexpected(DynamicLibException::format("Dynamic library symbol '{}' resolved to a null address", name));
#endif
  return std::shared_ptr<ErasedFunction>(m_handle, reinterpret_cast<ErasedFunction*>(symbol));
}

}
