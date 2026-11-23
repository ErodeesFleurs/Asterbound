#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>

import std;
import star.format;
import star.text_formatter;
import star.native_runtime;
import star.native_stack;
import star.windows_dump;


namespace Star {

// DbgHelp is process-native and not thread-safe. Its lock must remain usable
// through entry-point teardown without a dynamically initialized global owner.
struct DbgHelpLock {
  void lock() {
    AcquireSRWLockExclusive(&native_lock);
  }

  bool try_lock() noexcept {
    return TryAcquireSRWLockExclusive(&native_lock);
  }

  void unlock() {
    ReleaseSRWLockExclusive(&native_lock);
  }

  SRWLOCK native_lock = SRWLOCK_INIT;
};
static_assert(std::is_trivially_destructible_v<DbgHelpLock>);
static constinit DbgHelpLock dbg_help_lock;
// Protected by dbg_help_lock; overlapping explicit owners share one session.
static constinit std::size_t diagnostics_owners = 0;

bool try_write_minidump(DWORD thread_id, EXCEPTION_POINTERS& exception) noexcept {
  // The interrupted thread could itself hold this lock. Never wait on it.
  std::unique_lock lock(dbg_help_lock, std::try_to_lock);
  if (!lock)
    return false;
  auto file = CreateFileW(L"starbound.dmp", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  MINIDUMP_EXCEPTION_INFORMATION info{thread_id, &exception, FALSE};
  auto written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
      &info, nullptr, nullptr);
  return CloseHandle(file) && written;
}

std::expected<NativeRuntime, StarException> NativeRuntime::create(StandardStreams streams) {
  if (streams == StandardStreams::Optional) {
    auto prepared = prepare_standard_streams();
    if (!prepared)
      return std::unexpected(std::move(prepared.error()));
  }
  WSADATA wsa_data;
  auto status = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (status != 0)
    return std::unexpected(StarException(Star::format("WSAStartup failed: {}", status), false));

  std::optional<DWORD> symbol_error;
  {
    std::lock_guard lock(dbg_help_lock);
    if (diagnostics_owners == 0) {
      DWORD options = SymGetOptions();
      SymSetOptions(options | SYMOPT_LOAD_LINES);
      if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE))
        symbol_error = GetLastError();
    }
    if (!symbol_error)
      ++diagnostics_owners;
  }
  if (symbol_error) {
    WSACleanup();
    return std::unexpected(StarException(Star::format("SymInitialize failed: {}", *symbol_error), false));
  }
  return std::expected<NativeRuntime, StarException>(std::in_place, ConstructionKey{});
}

NativeRuntime::~NativeRuntime() {
  {
    std::lock_guard lock(dbg_help_lock);
    if (--diagnostics_owners == 0)
      SymCleanup(GetCurrentProcess());
  }
  WSACleanup();
}


// A temporary, balanced reference protects only the module containing this
// address while its identity is read. Diagnostics never pin a DLL for their
// lifetime and never release a borrowed module handle.
struct FreeModule {
  void operator()(HMODULE module) const noexcept { FreeLibrary(module); }
};
using ModuleReference = std::unique_ptr<std::remove_pointer_t<HMODULE>, FreeModule>;

static ModuleReference reference_module(std::uintptr_t address) {
  ModuleReference module;
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
          reinterpret_cast<LPCWSTR>(address), std::out_ptr(module)))
    return {};
  return module;
}

NativeStack::Module NativeStack::describe_module(std::uintptr_t base) {
  auto const& dos = *reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
  auto const& headers = *reinterpret_cast<IMAGE_NT_HEADERS const*>(base + dos.e_lfanew);
  std::array<wchar_t, 32768> path;
  auto length = GetModuleFileNameW(reinterpret_cast<HMODULE>(base), path.data(), static_cast<DWORD>(path.size()));
  return {base, headers.OptionalHeader.SizeOfImage, headers.FileHeader.TimeDateStamp,
      length && length < path.size() ? std::wstring(path.data(), length) : std::wstring()};
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

NativeStack NativeStack::capture() {
  std::array<void*, stack_limit> addresses;
  auto count = CaptureStackBackTrace(1, static_cast<DWORD>(addresses.size()), addresses.data(), nullptr);
  NativeStack stack;
  stack.m_frames.reserve(count);
  for (USHORT i = 0; i < count; ++i) {
    auto address = reinterpret_cast<std::uintptr_t>(addresses[i]);
    auto index = std::numeric_limits<std::size_t>::max();
    if (auto reference = reference_module(address)) {
      auto base = reinterpret_cast<std::uintptr_t>(reference.get());
      auto found = std::ranges::find(stack.m_modules, base, &Module::base);
      if (found == stack.m_modules.end()) {
        index = stack.m_modules.size();
        stack.m_modules.push_back(describe_module(base));
      } else {
        index = static_cast<std::size_t>(found - stack.m_modules.begin());
      }
    }
    stack.m_frames.push_back({address, index});
  }
  return stack;
}

void NativeStack::append_to(TextBuffer& output) const {
  struct CurrentModule {
    ModuleReference reference;
    std::string path;
    bool matches;
  };
  std::vector<CurrentModule> current;
  current.reserve(m_modules.size());
  for (auto const& captured : m_modules) {
    auto reference = reference_module(captured.base);
    auto matches = reference && !captured.path.empty()
        && reinterpret_cast<std::uintptr_t>(reference.get()) == captured.base
        && captured == describe_module(captured.base);
    current.emplace_back(std::move(reference), module_path_utf8(captured.path), matches);
  }
  HANDLE process = GetCurrentProcess();
  // A last module reference can run DLL teardown. Declare the DbgHelp guard
  // after current so the guard is gone when those references are released.
  std::lock_guard lock(dbg_help_lock);
  for (std::size_t i = 0; i < m_modules.size(); ++i) {
    if (!current[i].matches || diagnostics_owners == 0) {
      current[i].matches = false;
      continue;
    }
    auto const& captured = m_modules[i];
    IMAGEHLP_MODULE64 symbol_module{};
    symbol_module.SizeOfStruct = sizeof(symbol_module);
    current[i].matches = SymGetModuleInfo64(process, captured.base, &symbol_module)
        && symbol_module.BaseOfImage == captured.base
        && symbol_module.ImageSize == captured.image_size && symbol_module.TimeDateStamp == captured.timestamp;
  }
  for (std::size_t i = 0; i < m_frames.size(); ++i) {
    auto const& frame = m_frames[i];
    if (i)
      output.push_back('\n');
    std::format_to(std::back_inserter(output), "[{}] {:#x}", i, frame.address);
    if (frame.module == std::numeric_limits<std::size_t>::max())
      continue;
    auto const& captured = m_modules[frame.module];
    std::format_to(std::back_inserter(output), " {} +0x{:x} (image size 0x{:x}, timestamp 0x{:x})",
        current[frame.module].path, frame.address - captured.base, captured.image_size, captured.timestamp);
    if (!current[frame.module].matches)
      continue;
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD displacement = 0;
    if (SymGetLineFromAddr64(process, frame.address, &displacement, &line) && line.FileName && *line.FileName) {
      std::string_view file(line.FileName);
      auto slash = file.find_last_of("\\/");
      if (slash != std::string_view::npos)
        file.remove_prefix(slash + 1);
      std::format_to(std::back_inserter(output), " ({}:{})", file, line.LineNumber);
    }
    SYMBOL_INFO_PACKAGE symbol{};
    symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.si.MaxNameLen = MAX_SYM_NAME;
    if (SymFromAddr(process, frame.address, nullptr, &symbol.si))
      std::format_to(std::back_inserter(output), " {}", std::string_view(symbol.si.Name, symbol.si.NameLen));
  }
  if (m_frames.size() == stack_limit)
    output.append("\n[Stack Output Limit Reached]");
}

}
