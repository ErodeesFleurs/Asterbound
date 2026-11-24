module;
#include "SDL3/SDL.h"
#ifdef STAR_SYSTEM_WINDOWS
#include <windows.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef STAR_SYSTEM_MACOS
#include <mach-o/dyld.h>
#endif
#endif
import std;
import star.application_controller;
import star.encode;
import star.file;
import star.format;
import star.logging;
import star.secure_random;
import star.string;
import star.version;

module star.standalone_update;

namespace Star {
namespace {
namespace fs = std::filesystem;
using State = StandaloneUpdateState;
#ifdef STAR_SYSTEM_WINDOWS
constexpr auto Helper = "asterbound-updater.exe";
#else
constexpr auto Helper = "asterbound-updater";
#endif

fs::path executablePath() {
  std::error_code error;
#if defined(STAR_SYSTEM_WINDOWS) && (defined(__x86_64__) || defined(_M_X64))
  std::wstring path(256, L'\0');
  for (;;) {
    auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length) return {};
    if (length < path.size()) {
      path.resize(length);
      return fs::canonical(path, error);
    }
    path.resize(path.size() * 2);
  }
#elif defined(STAR_SYSTEM_LINUX) && (defined(__x86_64__) || defined(__aarch64__))
  return fs::canonical("/proc/self/exe", error);
#elif defined(STAR_SYSTEM_MACOS) && (defined(__x86_64__) || defined(__aarch64__))
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> path(size);
  if (!size || _NSGetExecutablePath(path.data(), &size) != 0) return {};
  return fs::canonical(path.data(), error);
#else
  return {};
#endif
}

bool isPackagedClient(fs::path const& path) {
#ifdef STAR_SYSTEM_WINDOWS
  return path.filename() == "asterbound.exe" && path.parent_path().filename() == "win";
#else
  if (path.native().starts_with("/nix/store/")) return false;
#ifdef STAR_SYSTEM_MACOS
  return path.native().ends_with("/osx/Asterbound.app/Contents/MacOS/asterbound");
#else
  return path.native().ends_with("/linux/asterbound");
#endif
#endif
}

bool regularHelper(fs::path const& helper) {
#ifdef STAR_SYSTEM_WINDOWS
  auto attributes = GetFileAttributesW(helper.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE));
#else
  std::error_code error;
  return fs::is_regular_file(fs::symlink_status(helper, error)) && access(helper.c_str(), X_OK) == 0;
#endif
}

bool createPrivateDirectory(fs::path const& path) {
#ifdef STAR_SYSTEM_WINDOWS
  // Do not inherit a permissive DACL from a shared TEMP. Owner Rights also
  // applies to the copy subsequently created inside this directory.
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr))
    return false;
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
  bool created = CreateDirectoryW(path.c_str(), &attributes) != 0;
  LocalFree(descriptor);
  return created;
#else
  return mkdir(path.c_str(), 0700) == 0;
#endif
}

#ifdef STAR_SYSTEM_WINDOWS
// SDL 3.4.8's argv encoder does not double *runs* of backslashes before
// quotes. Encode the CommandLineToArgvW/Go convention explicitly: every
// argument is quoted, including empty strings. There is no command shell.
std::string windowsCommandLine(std::vector<char const*> const& args) {
  std::string line;
  for (auto arg : args) {
    if (!arg) break;
    if (!line.empty()) line.push_back(' ');
    line.push_back('"');
    std::size_t slashes = 0;
    for (; *arg; ++arg) {
      if (*arg == '\\') {
        ++slashes;
      } else {
        line.append(*arg == '"' ? 2 * slashes + 1 : slashes, '\\');
        line.push_back(*arg);
        slashes = 0;
      }
    }
    line.append(2 * slashes, '\\');
    line.push_back('"');
  }
  return line;
}
#endif
}

void StandaloneUpdate::captureLaunch(StringList const& arguments) {
  auto client = executablePath();
  if (!isPackagedClient(client) || !regularHelper(client.parent_path() / Helper)) return;
  auto cwd = File::current_directory();
  if (!cwd) return;
  m_client = std::move(client);
  m_workingDirectory = std::move(*cwd);
  m_arguments = arguments;
  m_state = State::Ready;
}

void StandaloneUpdate::start(String const& targetVersion) {
  if (m_state != State::Ready && m_state != State::Failed) return;
  m_state = State::Failed;
  auto helper = m_client.parent_path() / Helper;
  if (!regularHelper(helper)) return;
  std::error_code error;
#ifdef STAR_SYSTEM_WINDOWS
  auto temp = fs::temp_directory_path(error);
#else
  // Match os.TempDir() in the helper (libc++ also considers TMP and TEMP).
  auto tmpdir = std::getenv("TMPDIR");
  fs::path temp = tmpdir && *tmpdir ? tmpdir : "/tmp";
#endif
  std::array<char, 16> nonce;
  if (error || !temp.is_absolute() || !secure_random_fill(nonce)) return;
  auto dir = temp / ("asterbound-update-" + hex_encode(nonce).utf8());
  if (!createPrivateDirectory(dir)) return;
  auto copy = dir / Helper;
  if (fs::copy_file(helper, copy, error)) {
    auto executable = copy.u8string();
    auto client = m_client.u8string();
    std::vector<char const*> args;
    args.reserve(6 + m_arguments.size());
    args.insert(args.end(), {reinterpret_cast<char const*>(executable.c_str()), "--install", product_version.data(),
        targetVersion.utf8().c_str(), reinterpret_cast<char const*>(client.c_str())});
    for (auto const& arg : m_arguments) args.push_back(arg.utf8().c_str());
    args.push_back(nullptr);
    auto props = SDL_CreateProperties();
    if (props && SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args.data()) &&
#ifdef STAR_SYSTEM_WINDOWS
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_CMDLINE_STRING, windowsCommandLine(args).c_str()) &&
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true) && // CREATE_NO_WINDOW, still a direct child on Windows
#endif
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, m_workingDirectory.utf8().c_str()) &&
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP) &&
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP))
      m_process.reset(SDL_CreateProcessWithProperties(props));
    if (props) SDL_DestroyProperties(props);
  }
  if (!m_process) {
    EmergencyLog::warn("Could not launch standalone updater: {}", error ? error.message() : std::string(SDL_GetError()));
    fs::remove(copy, error);
    fs::remove(dir, error);
    return;
  }
  // The helper retires its copy even after a committed restart. A Windows
  // helper that rejects its installation before finding a trusted cleanup
  // sibling cannot unlink its running executable: reap that case here.
#ifdef STAR_SYSTEM_WINDOWS
  m_tempDirectory = std::move(dir);
#endif
  m_readyBytes = 0;
  m_afterExit = State::Failed;
  m_state = State::Preparing;
}

void StandaloneUpdate::closeInput(State afterExit) {
  if (auto input = SDL_GetProcessInput(m_process.get())) SDL_CloseIO(input);
  m_afterExit = afterExit;
  m_state = State::Cancelling;
}

void StandaloneUpdate::cancel() {
  if (m_process && m_state == State::Preparing) closeInput(State::Ready);
}

bool StandaloneUpdate::poll() {
  if (!m_process) return false;
  if (SDL_WaitProcess(m_process.get(), false, nullptr)) {
    m_process.reset();
#ifdef STAR_SYSTEM_WINDOWS
    // Pin the directory against rename; never follow a replacement junction.
    auto dir = CreateFileW(m_tempDirectory.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (dir != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION info{};
      bool safe = GetFileInformationByHandle(dir, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
      if (safe) DeleteFileW((m_tempDirectory / Helper).c_str());
      CloseHandle(dir);
      if (safe) RemoveDirectoryW(m_tempDirectory.c_str());
    }
    m_tempDirectory.clear();
#endif
    m_state = m_afterExit;
    return false;
  }
  if (m_state != State::Preparing) return false;
  constexpr std::string_view ready = "READY\n", commit = "COMMIT\n";
  auto output = SDL_GetProcessOutput(m_process.get());
  std::array<char, ready.size() + 1> buffer;
  auto count = output ? SDL_ReadIO(output, buffer.data(), buffer.size()) : 0;
  if (!count && output && SDL_GetIOStatus(output) == SDL_IO_STATUS_NOT_READY) return false;
  if (!count || !ready.substr(m_readyBytes).starts_with(std::string_view(buffer.data(), count))) {
    closeInput(State::Failed);
    return false;
  }
  m_readyBytes += count;
  if (m_readyBytes != ready.size()) return false;
  auto input = SDL_GetProcessInput(m_process.get());
  if (!input || SDL_WriteIO(input, commit.data(), commit.size()) != commit.size()) {
    closeInput(State::Failed);
    return false;
  }
  m_state = State::Restarting;
  m_process.reset(); // closes the pipes; never waits for the installer or the restarted game
  return true;
}

}
