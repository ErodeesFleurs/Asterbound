#include <stdio.h>
#include <windows.h>
import std;
import star.file;
import star.io_device;
import star.list;
import star.format;
import star.random;
import star.string;
import star.byte_array;
import star.encode;

namespace Star {
namespace {
OVERLAPPED make_overlapped(StreamOffset offset) {
  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset);
  overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32);
  return overlapped;
}

HANDLE native_handle(std::uintptr_t value) noexcept {
  return reinterpret_cast<HANDLE>(value);
}

DWORD transfer_size(std::size_t size) noexcept {
  return static_cast<DWORD>(std::min<std::size_t>(size, MAXDWORD));
}

StarException cursor_operation_error(std::string_view operation, DWORD error,
                                     std::expected<void, StarException> const& restored) {
  auto primary = IOException::format("{} error {}", operation, error);
  if (!restored) {
    auto logged = Star::print(*stderr, "Error restoring file cursor after failed {}: {}\n", operation, restored.error().what());
    if (!logged)
      return std::move(primary).with_context(Star::format("Could not report cursor restoration failure ({}): {}",
                                                         restored.error().what(), logged.error().what()));
  }
  return primary;
}

std::expected<String, StarException> temporary_root() {
  WCHAR path[MAX_PATH];
  auto length = GetTempPathW(MAX_PATH, path);
  if (!length)
    return std::unexpected(IOException::format("Could not call GetTempPath: {}", GetLastError()));
  if (length >= MAX_PATH)
    return std::unexpected(IOException("GetTempPath exceeds path buffer"));
  return String::from_wstring(std::wstring_view(path, length));
}

bool missing_path_error(DWORD error) {
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_DIRECTORY;
}

struct FindAttributes {
  DWORD attributes;
  DWORD reparse_tag;
};

// FindFirstFile (unlike GetFileAttributes) accepts wildcard patterns.
std::expected<std::optional<FindAttributes>, StarException> find_attributes(String const& path, std::string_view kind) {
  auto native = path.to_wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  WIN32_FIND_DATAW data;
  auto handle = FindFirstFileW(native->c_str(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return std::nullopt;
    return std::unexpected(IOException::format("Could not inspect {} '{}': {}", kind, path, error));
  }
  if (!FindClose(handle))
    return std::unexpected(IOException::format("FindClose failed: {}", GetLastError()));
  return FindAttributes{data.dwFileAttributes, data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ? data.dwReserved0 : 0};
}

std::expected<StreamOffset, StarException> checked_size(std::uint64_t size) {
  if (!std::in_range<StreamOffset>(size))
    return std::unexpected(IOException("File size cannot be represented as a stream offset"));
  return static_cast<StreamOffset>(size);
}
}// namespace

String File::convert_dir_separators(String const& path) {
  auto bytes = path.utf8();
  std::replace(bytes.begin(), bytes.end(), '/', '\\');
  return String(std::move(bytes));
}

std::expected<bool, StarException> File::exists(String const& path) {
  return find_attributes(path, "path").transform([](auto const& entry) { return entry.has_value(); });
}

std::expected<bool, StarException> File::is_regular_file(String const& path) {
  return find_attributes(path, "path").transform([](auto const& entry) {
    return entry && !(entry->attributes & FILE_ATTRIBUTE_DIRECTORY);
  });
}

std::expected<bool, StarException> File::is_directory(String const& path) {
  auto bytes = path.utf8();
  while (bytes.size() > 1 && (bytes.back() == '\\' || bytes.back() == '/') && !(bytes.size() == 3 && bytes[1] == ':'))
    bytes.pop_back();
  auto native = String(std::move(bytes)).to_wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  auto attributes = GetFileAttributesW(native->c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return false;
    return std::unexpected(IOException::format("Could not inspect directory '{}': {}", path, error));
  }
  return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::expected<bool, StarException> File::is_symlink(String const& path) {
  return find_attributes(path, "symlink").transform([](auto const& entry) {
    return entry && entry->reparse_tag == IO_REPARSE_TAG_SYMLINK;
  });
}

std::expected<StreamOffset, StarException> File::file_size(String const& path) {
  auto native = path.to_wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  auto handle = CreateFileW(native->c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return std::unexpected(IOException::format("Could not inspect file '{}': {}", path, GetLastError()));
  NativeFileHandle owner(reinterpret_cast<std::uintptr_t>(handle));
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(handle, &info))
    return std::unexpected(IOException::format("Could not inspect file '{}': {}", path, GetLastError()));
  if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || GetFileType(handle) != FILE_TYPE_DISK)
    return std::unexpected(IOException::format("Cannot query regular file size for '{}'", path));
  auto size = checked_size((static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow);
  auto closed = owner.close();
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  return size;
}

// GetFullPathName is lexical on Windows and accepts paths that do not exist.
std::expected<String, StarException> File::full_path(String const& path) {
  auto native = path.to_wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  WCHAR buffer[MAX_PATH];
  auto length = GetFullPathNameW(native->c_str(), MAX_PATH, buffer, nullptr);
  if (!length)
    return std::unexpected(IOException::format("GetFullPathName failed on path '{}': {}", path, GetLastError()));
  if (length >= MAX_PATH)
    return std::unexpected(IOException::format("GetFullPathName exceeds path buffer on path '{}'", path));
  return String::from_wstring(std::wstring_view(buffer, length));
}

std::expected<List<std::pair<String, bool>>, StarException> File::directory_entries(String const& dir_name,
                                                                                    bool skip_dots) {
  auto native = relative_to(dir_name, "*").to_wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  std::error_code error;
  std::filesystem::path path(std::move(*native));
  path.remove_filename();
  std::filesystem::directory_iterator entry(path, error);
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
  List<std::pair<String, bool>> files;
  // Windows root directories do not expose the special dot entries.
  if (!skip_dots && path != path.root_path()) {
    files.push_back({".", true});
    files.push_back({"..", true});
  }
  for (std::filesystem::directory_iterator end; entry != end; entry.increment(error)) {
    if (error)
      return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
    auto name = String::from_wstring(entry->path().filename().native());
    if (!name)
      return std::unexpected(std::move(name.error()));
    // Keep Windows' directory-reparse-point classification, even for a broken link.
    auto attributes = GetFileAttributesW(entry->path().c_str());
    files.push_back(
      {std::move(*name), attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)});
  }
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
  return files;
}

String File::base_name(String const& file_name) {
  auto const& bytes = file_name.utf8();
  auto end = bytes.find_last_not_of("\\/");
  if (end == std::string::npos)
    return {};
  auto separator = bytes.find_last_of("\\/", end);
  auto start = separator == std::string::npos ? 0 : separator + 1;
  return String(bytes.substr(start, end + 1 - start));
}

String File::dir_name(String const& file_name) {
  if (file_name == "\\" || file_name == "/")
    return "\\";
  auto const& bytes = file_name.utf8();
  auto end = bytes.find_last_not_of("\\/");
  if (end == std::string::npos)
    return ".";
  auto separator = bytes.find_last_of("\\/", end);
  if (separator == std::string::npos)
    return ".";
  auto parent_end = bytes.find_last_not_of("\\/", separator);
  return parent_end == std::string::npos ? String(".") : String(bytes.substr(0, parent_end + 1));
}

String File::relative_to(String const& base, String const& path) {
  auto const& suffix = path.utf8();
  if ((!suffix.empty() && (suffix[0] == '/' || suffix[0] == '\\'))
      || (suffix.size() >= 2 && ((suffix[0] >= 'a' && suffix[0] <= 'z') || (suffix[0] >= 'A' && suffix[0] <= 'Z')) && suffix[1] == ':'))
    return path;
  auto bytes = base.utf8();
  if (!bytes.empty() && (bytes.back() == '/' || bytes.back() == '\\'))
    bytes.pop_back();
  else if (bytes.size() >= 2 && bytes.back() == '.' && (bytes[bytes.size() - 2] == '/' || bytes[bytes.size() - 2] == '\\'))
    bytes.resize(bytes.size() - 2);
  bytes.push_back('\\');
  bytes.append(suffix, suffix.size() >= 2 && suffix[0] == '.' && (suffix[1] == '/' || suffix[1] == '\\') ? 2 : 0, std::string::npos);
  return String(std::move(bytes));
}

std::expected<String, StarException> File::temporary_file_name(RandomService& random) {
  auto root = temporary_root();
  if (!root)
    return std::unexpected(std::move(root.error()));
  return relative_to(*root, Star::format("starbound.tmpfile.{}", hex_encode(random.random_bytes(16))));
}

std::expected<std::shared_ptr<File>, StarException> File::temporary_file(RandomService& random) {
  for (;;) {
    auto name = temporary_file_name(random);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto path = name->to_wstring();
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto handle =
      CreateFileW(path->c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      auto error = GetLastError();
      if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        continue;
      return std::unexpected(IOException::format("Could not create temporary file '{}': {}", *name, error));
    }
    NativeFileHandle owner(reinterpret_cast<std::uintptr_t>(handle));
    auto file = std::make_shared<File>(std::move(*name));
    file->m_file.reset(owner.release());
    file->set_mode(IOMode::ReadWrite);
    return file;
  }
}

std::expected<std::shared_ptr<File>, StarException> File::ephemeral_file(RandomService& random) {
  auto file = temporary_file(random);
  if (!file)
    return std::unexpected(std::move(file.error()));
  auto& name = (*file)->m_filename;
  auto path = name.to_wstring();
  if (!path)
    return std::unexpected(std::move(path.error()));
  if (!DeleteFileW(path->c_str()))
    return std::unexpected(
      IOException::format("Could not delete ephemeral file '{}': {}", name, GetLastError()));
  name = "";
  return std::move(*file);
}

std::expected<String, StarException> File::temporary_directory(RandomService& random) {
  auto root = temporary_root();
  if (!root)
    return std::unexpected(std::move(root.error()));
  for (;;) {
    auto directory = relative_to(*root, Star::format("starbound.tmpdir.{}", hex_encode(random.random_bytes(16))));
    auto path = directory.to_wstring();
    if (!path)
      return std::unexpected(std::move(path.error()));
    if (CreateDirectoryW(path->c_str(), nullptr))
      return directory;
    auto error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
      return std::unexpected(IOException::format("Could not create temporary directory '{}': {}", directory, error));
  }
}

std::expected<void, StarException> File::rename(String const& source, String const& target) {
  auto source_path = source.to_wstring();
  if (!source_path)
    return std::unexpected(std::move(source_path.error()));
  auto target_path = target.to_wstring();
  if (!target_path)
    return std::unexpected(std::move(target_path.error()));
  // Do not move or remove the old target before the replacement succeeds.
  // Omitting COPY_ALLOWED also prevents a cross-volume copy/delete fallback.
  if (!MoveFileExW(source_path->c_str(), target_path->c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return std::unexpected(IOException::format("Rename error: {}", GetLastError()));
  return {};
}

File::NativeFileHandle::~NativeFileHandle() noexcept {
  if (*this && !CloseHandle(native_handle(release()))) {
    if (!Star::print(*stderr, "Error closing file after failed acquisition: {}\n", GetLastError()))
      std::_Exit(1);
  }
}

std::expected<File::NativeFileHandle, StarException>
File::NativeFileHandle::open(String const& filename, IOMode mode, bool create, bool exclusive) {
  if (exclusive && !create)
    return std::unexpected(IOException("Exclusive file creation requires create mode"));
  auto path = filename.to_wstring();
  if (!path)
    return std::unexpected(std::move(path.error()));
  DWORD access = 0;
  if (mode & IOMode::Read)
    access |= GENERIC_READ;
  if (mode & IOMode::Write)
    access |= GENERIC_WRITE;
  if (!access)
    return std::unexpected(IOException("Cannot open file with closed IO mode"));
  auto disposition = exclusive ? CREATE_NEW : create && mode & IOMode::Write ? OPEN_ALWAYS : OPEN_EXISTING;
  auto file = CreateFileW(path->c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                          disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return std::unexpected(IOException::format("could not open file '{}' {}", filename, GetLastError()));
  NativeFileHandle owner(reinterpret_cast<std::uintptr_t>(file));
  LARGE_INTEGER zero = {};
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
    return std::unexpected(IOException::format("could not set file pointer in fopen '{}' {}", filename, GetLastError()));
  if (mode & IOMode::Truncate && !SetEndOfFile(file))
    return std::unexpected(IOException::format("could not set end of file in fopen '{}' {}", filename, GetLastError()));
  if (mode & IOMode::Append && !SetFilePointerEx(file, zero, nullptr, FILE_END))
    return std::unexpected(IOException::format("could not set file pointer in fopen '{}' {}", filename, GetLastError()));
  return owner;
}

std::expected<void, StarException> File::NativeFileHandle::seek(StreamOffset offset, IOSeek seek_mode) {
  LARGE_INTEGER distance;
  distance.QuadPart = offset;
  DWORD origin = seek_mode == IOSeek::Relative ? FILE_CURRENT : seek_mode == IOSeek::Absolute ? FILE_BEGIN : FILE_END;
  if (!SetFilePointerEx(native_handle(get()), distance, nullptr, origin))
    return std::unexpected(IOException::format("Seek error {}", GetLastError()));
  return {};
}

std::expected<StreamOffset, StarException> File::NativeFileHandle::pos() {
  LARGE_INTEGER zero = {};
  LARGE_INTEGER position;
  if (!SetFilePointerEx(native_handle(get()), zero, &position, FILE_CURRENT))
    return std::unexpected(IOException::format("Tell error {}", GetLastError()));
  return position.QuadPart;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::read(std::span<char> data) {
  if (data.empty())
    return 0;
  DWORD count = 0;
  if (!ReadFile(native_handle(get()), data.data(), transfer_size(data.size()), &count, nullptr))
    return std::unexpected(IOException::format("read error {}", GetLastError()));
  return count;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::write(std::span<char const> data) {
  if (data.empty())
    return 0;
  DWORD count = 0;
  if (!WriteFile(native_handle(get()), data.data(), transfer_size(data.size()), &count, nullptr))
    return std::unexpected(IOException::format("write error {}", GetLastError()));
  return count;
}

std::expected<void, StarException> File::NativeFileHandle::sync() {
  if (!FlushFileBuffers(native_handle(get())))
    return std::unexpected(IOException::format("fsync error {}", GetLastError()));
  return {};
}

std::expected<void, StarException> File::NativeFileHandle::close() {
  if (*this && !CloseHandle(native_handle(release())))
    return std::unexpected(IOException::format("Close error {}", GetLastError()));
  return {};
}

std::expected<StreamOffset, StarException> File::NativeFileHandle::size() {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(native_handle(get()), &size))
    return std::unexpected(IOException::format("could not get file size in fsize {}", GetLastError()));
  if (size.QuadPart < 0)
    return std::unexpected(IOException("File size cannot be represented as a stream offset"));
  return checked_size(static_cast<std::uint64_t>(size.QuadPart));
}

std::expected<std::size_t, StarException> File::NativeFileHandle::read_at(StreamOffset position, std::span<char> data) {
  if (position < 0)
    return std::unexpected(IOException("Cannot read at a negative file position"));
  if (data.empty())
    return 0;
  auto cursor = pos();
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  DWORD count = 0;
  auto overlapped = make_overlapped(position);
  auto succeeded = ReadFile(native_handle(get()), data.data(), transfer_size(data.size()), &count, &overlapped);
  auto error = succeeded ? ERROR_SUCCESS : GetLastError();
  auto restored = seek(*cursor, IOSeek::Absolute);
  if (!succeeded && error != ERROR_HANDLE_EOF)
    return std::unexpected(cursor_operation_error("pread", error, restored));
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return count;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::write_at(StreamOffset position, std::span<char const> data) {
  if (position < 0)
    return std::unexpected(IOException("Cannot write at a negative file position"));
  if (data.empty())
    return 0;
  auto cursor = pos();
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  DWORD count = 0;
  auto overlapped = make_overlapped(position);
  auto succeeded = WriteFile(native_handle(get()), data.data(), transfer_size(data.size()), &count, &overlapped);
  auto error = succeeded ? ERROR_SUCCESS : GetLastError();
  auto restored = seek(*cursor, IOSeek::Absolute);
  if (!succeeded)
    return std::unexpected(cursor_operation_error("pwrite", error, restored));
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return count;
}

std::expected<void, StarException> File::NativeFileHandle::resize(StreamOffset length) {
  if (length < 0)
    return std::unexpected(IOException("Cannot resize a file to a negative size"));
  auto cursor = pos();
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  auto moved = seek(length, IOSeek::Absolute);
  if (!moved)
    return std::unexpected(std::move(moved.error()));
  auto resized = SetEndOfFile(native_handle(get()));
  auto error = resized ? ERROR_SUCCESS : GetLastError();
  auto restored = seek(*cursor, IOSeek::Absolute);
  if (!resized)
    return std::unexpected(cursor_operation_error("resize", error, restored));
  return restored;
}

}
