#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>

import std;
import star.format;
import star.output_proxy;
import star.text_formatter;
import star.logging;
import star.version;
import star.native_runtime;


namespace Star {

// DbgHelp is process-native and not thread-safe. Its lock must remain usable
// through entry-point teardown without a dynamically initialized global owner.
struct DbgHelpLock {
  void lock() {
    AcquireSRWLockExclusive(&nativeLock);
  }

  void unlock() {
    ReleaseSRWLockExclusive(&nativeLock);
  }

  SRWLOCK nativeLock = SRWLOCK_INIT;
};
static_assert(std::is_trivially_destructible_v<DbgHelpLock>);
static constinit DbgHelpLock g_dbgHelpLock;
// Protected by g_dbgHelpLock; overlapping explicit owners share one session.
static constinit std::size_t g_diagnosticsOwners = 0;

std::expected<std::unique_ptr<NativeRuntime>, StarException> NativeRuntime::create() {
  auto runtime = std::unique_ptr<NativeRuntime>(new NativeRuntime());
  WSADATA wsaData;
  auto status = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (status != 0)
    return std::unexpected(StarException(Star::format("WSAStartup failed: {}", status), false));
  runtime->m_winsockInitialized = true;

  DWORD symbolError = ERROR_SUCCESS;
  bool initialized = true;
  {
    std::lock_guard lock(g_dbgHelpLock);
    if (g_diagnosticsOwners == 0) {
      DWORD options = SymGetOptions();
      SymSetOptions(options | SYMOPT_LOAD_LINES);
      if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
        initialized = false;
        symbolError = GetLastError();
      }
    }
    if (initialized) {
      ++g_diagnosticsOwners;
      runtime->m_diagnosticsInitialized = true;
    }
  }
  if (!initialized)
    return std::unexpected(StarException(Star::format("SymInitialize failed: {}", symbolError), false));
  return runtime;
}

NativeRuntime::~NativeRuntime() {
  {
    std::lock_guard lock(g_dbgHelpLock);
    if (m_diagnosticsInitialized && --g_diagnosticsOwners == 0)
      SymCleanup(GetCurrentProcess());
  }
  if (m_winsockInitialized)
    WSACleanup();
}

static constexpr std::size_t StackLimit = 256;

struct CapturedModule {
  HMODULE base;
  DWORD imageSize;
  DWORD timestamp;
  std::wstring path;
};

// A temporary, balanced reference protects only the module containing this
// address while its identity is read. Diagnostics never pin a DLL for their
// lifetime and never release a borrowed module handle.
class ModuleReference {
public:
  explicit ModuleReference(void const* address) {
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(address), &m_module);
  }
  ~ModuleReference() {
    if (m_module)
      FreeLibrary(m_module);
  }
  ModuleReference(ModuleReference const&) = delete;
  ModuleReference& operator=(ModuleReference const&) = delete;
  ModuleReference(ModuleReference&& other) noexcept : m_module(std::exchange(other.m_module, nullptr)) {}
  HMODULE get() const { return m_module; }

private:
  HMODULE m_module = nullptr;
};

static CapturedModule describe_module(HMODULE module) {
  auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(module);
  auto const* headers = reinterpret_cast<IMAGE_NT_HEADERS const*>(
      reinterpret_cast<std::byte const*>(module) + dos->e_lfanew);
  std::array<wchar_t, 32768> path;
  auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
  return {module, headers->OptionalHeader.SizeOfImage, headers->FileHeader.TimeDateStamp,
      length && length < path.size() ? std::wstring(path.data(), length) : std::wstring()};
}

static bool same_module(CapturedModule const& left, CapturedModule const& right) {
  return !left.path.empty() && left.base == right.base && left.imageSize == right.imageSize
      && left.timestamp == right.timestamp && left.path == right.path;
}

static std::string module_path_utf8(std::wstring const& path) {
  if (path.empty())
    return {};
  auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
      nullptr, 0, nullptr, nullptr);
  if (!length) {
    // Windows paths may contain unpaired UTF-16 units. Retain those units
    // explicitly instead of replacing or dropping the captured module name.
    TextBuffer escaped;
    for (wchar_t unit : path)
      std::format_to(std::back_inserter(escaped), "\\u{:04x}", static_cast<unsigned int>(unit));
    return std::move(escaped).str();
  }
  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
      result.data(), length, nullptr, nullptr);
  return result;
}

static std::function<void(TextBuffer&, bool)> capture_stack() {
  struct Frame {
    void* address;
    std::size_t module;
  };
  std::array<void*, StackLimit> addresses;
  auto count = CaptureStackBackTrace(1, static_cast<DWORD>(addresses.size()), addresses.data(), nullptr);
  std::vector<Frame> frames;
  std::vector<CapturedModule> modules;
  frames.reserve(count);
  for (USHORT i = 0; i < count; ++i) {
    auto moduleIndex = std::numeric_limits<std::size_t>::max();
    ModuleReference module(addresses[i]);
    if (module.get()) {
      auto found = std::find_if(modules.begin(), modules.end(),
          [&](auto const& captured) { return captured.base == module.get(); });
      if (found == modules.end()) {
        moduleIndex = modules.size();
        modules.push_back(describe_module(module.get()));
      } else {
        moduleIndex = static_cast<std::size_t>(found - modules.begin());
      }
    }
    frames.push_back({addresses[i], moduleIndex});
  }
  return [frames = std::move(frames), modules = std::move(modules)](TextBuffer& output, bool) {
    struct CurrentModule {
      ModuleReference reference;
      std::string path;
      bool matches;
    };
    std::vector<CurrentModule> current;
    current.reserve(modules.size());
    for (auto const& captured : modules) {
      auto& module = current.emplace_back(ModuleReference(captured.base), module_path_utf8(captured.path), false);
      module.matches = module.reference.get() && same_module(captured, describe_module(module.reference.get()));
    }
    HANDLE process = GetCurrentProcess();
    // Release the symbol lock before dropping module references: a last
    // reference can run DLL teardown code, which must not inherit this lock.
    std::lock_guard lock(g_dbgHelpLock);
    for (std::size_t i = 0; i < modules.size(); ++i) {
      if (!current[i].matches || g_diagnosticsOwners == 0) {
        current[i].matches = false;
        continue;
      }
      auto const& captured = modules[i];
      IMAGEHLP_MODULE64 symbolModule{};
      symbolModule.SizeOfStruct = sizeof(symbolModule);
      current[i].matches = SymGetModuleInfo64(process, reinterpret_cast<DWORD64>(captured.base), &symbolModule)
          && symbolModule.BaseOfImage == reinterpret_cast<DWORD64>(captured.base)
          && symbolModule.ImageSize == captured.imageSize && symbolModule.TimeDateStamp == captured.timestamp;
    }
    for (std::size_t i = 0; i < frames.size(); ++i) {
      auto const& frame = frames[i];
      if (i)
        output.push_back('\n');
      std::format_to(std::back_inserter(output), "[{}] {}", i, frame.address);
      if (frame.module == std::numeric_limits<std::size_t>::max())
        continue;
      auto const& captured = modules[frame.module];
      std::format_to(std::back_inserter(output), " {} +0x{:x} (image size 0x{:x}, timestamp 0x{:x})", current[frame.module].path,
          reinterpret_cast<std::uintptr_t>(frame.address) - reinterpret_cast<std::uintptr_t>(captured.base),
          captured.imageSize, captured.timestamp);
      if (!current[frame.module].matches)
        continue;
      auto address = reinterpret_cast<DWORD64>(frame.address);
      IMAGEHLP_LINE64 line{};
      line.SizeOfStruct = sizeof(line);
      DWORD displacement = 0;
      if (SymGetLineFromAddr64(process, address, &displacement, &line) && line.FileName && *line.FileName) {
        std::string_view file(line.FileName);
        auto slash = file.find_last_of("\\/");
        if (slash != std::string_view::npos)
          file.remove_prefix(slash + 1);
        std::format_to(std::back_inserter(output), " ({}:{})", file, line.LineNumber);
      }
      alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> buffer{};
      auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = MAX_SYM_NAME;
      if (SymFromAddr(process, address, nullptr, symbol))
        std::format_to(std::back_inserter(output), " {}", std::string_view(symbol->Name, symbol->NameLen));
    }
    if (frames.size() == StackLimit)
      output.append("\n[Stack Output Limit Reached]");
  };
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
  TextBuffer buffer;
  std::format_to(std::back_inserter(buffer), "v{} ({})\nFatal Error: {}\n",
      OpenStarVersionString, StarSourceIdentifierString, message);
  if (showStackTrace)
    buffer.append(capture_backtrace());

  auto diagnostic = std::move(buffer).str();
  EmergencyLog::log(LogLevel::Error, diagnostic);
  // Fatal paths must terminate without waiting for interactive acknowledgement.
  std::abort();
}


}
