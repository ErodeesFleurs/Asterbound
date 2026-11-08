#include <io.h>
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
import star.string_windows;

namespace Star {
namespace {
OVERLAPPED make_overlapped(StreamOffset offset) {
  OVERLAPPED overlapped = {};
  overlapped.Offset = static_cast<DWORD>(offset);
  overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32);
  return overlapped;
}

void close_after_failure(HANDLE file) {
  if (!CloseHandle(file)) {
    if (!Star::print(stderr, "Error closing file after failed acquisition: {}\n", GetLastError()))
      std::_Exit(1);
  }
}

std::expected<String, StarException> temporary_root() {
  WCHAR path[MAX_PATH];
  auto length = GetTempPathW(MAX_PATH, path);
  if (!length)
    return std::unexpected(IOException::format("Could not call GetTempPath: {}", GetLastError()));
  if (length >= MAX_PATH)
    return std::unexpected(IOException("GetTempPath exceeds path buffer"));
  return utf16ToString(path);
}

bool missing_path_error(DWORD error) {
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_DIRECTORY;
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

// These queries intentionally retain wildcard matching and Windows reparse-point
// attributes, which are not equivalent to filesystem::status/symlink_status.
std::expected<bool, StarException> File::exists(String const& path) {
  auto native = stringToUtf16(path);
  if (!native)
    return std::unexpected(std::move(native.error()));
  WIN32_FIND_DATAW data;
  auto handle = FindFirstFileW(native->get(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return false;
    return std::unexpected(IOException::format("Could not inspect path '{}': {}", path, error));
  }
  if (!FindClose(handle))
    return std::unexpected(IOException::format("FindClose failed: {}", GetLastError()));
  return true;
}

std::expected<bool, StarException> File::is_regular_file(String const& path) {
  auto native = stringToUtf16(path);
  if (!native)
    return std::unexpected(std::move(native.error()));
  WIN32_FIND_DATAW data;
  auto handle = FindFirstFileW(native->get(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return false;
    return std::unexpected(IOException::format("Could not inspect path '{}': {}", path, error));
  }
  if (!FindClose(handle))
    return std::unexpected(IOException::format("FindClose failed: {}", GetLastError()));
  return !(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::expected<bool, StarException> File::is_directory(String const& path) {
  auto bytes = path.utf8();
  while (bytes.size() > 1 && (bytes.back() == '\\' || bytes.back() == '/') && !(bytes.size() == 3 && bytes[1] == ':'))
    bytes.pop_back();
  auto native = stringToUtf16(String(std::move(bytes)));
  if (!native)
    return std::unexpected(std::move(native.error()));
  auto attributes = GetFileAttributesW(native->get());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return false;
    return std::unexpected(IOException::format("Could not inspect directory '{}': {}", path, error));
  }
  return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::expected<bool, StarException> File::is_symlink(String const& path) {
  auto native = stringToUtf16(path);
  if (!native)
    return std::unexpected(std::move(native.error()));
  WIN32_FIND_DATAW data;
  auto handle = FindFirstFileW(native->get(), &data);
  if (handle == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    if (missing_path_error(error))
      return false;
    return std::unexpected(IOException::format("Could not inspect symlink '{}': {}", path, error));
  }
  if (!FindClose(handle))
    return std::unexpected(IOException::format("FindClose failed: {}", GetLastError()));
  return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && data.dwReserved0 == IO_REPARSE_TAG_SYMLINK;
}

std::expected<StreamOffset, StarException> File::file_size(String const& path) {
  auto native = stringToUtf16(path);
  if (!native)
    return std::unexpected(std::move(native.error()));
  auto handle = CreateFileW(native->get(), GENERIC_READ | FILE_READ_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return std::unexpected(IOException::format("Could not inspect file '{}': {}", path, GetLastError()));
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(handle, &info)) {
    auto error = GetLastError();
    close_after_failure(handle);
    return std::unexpected(IOException::format("Could not inspect file '{}': {}", path, error));
  }
  if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || GetFileType(handle) != FILE_TYPE_DISK) {
    close_after_failure(handle);
    return std::unexpected(IOException::format("Cannot query regular file size for '{}'", path));
  }
  auto size = checked_size((static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow);
  auto closed = fclose(handle);
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  return size;
}

// GetFullPathName is lexical on Windows and accepts paths that do not exist.
std::expected<String, StarException> File::full_path(String const& path) {
  auto native = stringToUtf16(path);
  if (!native)
    return std::unexpected(std::move(native.error()));
  WCHAR buffer[MAX_PATH];
  auto length = GetFullPathNameW(native->get(), MAX_PATH, buffer, nullptr);
  if (!length)
    return std::unexpected(IOException::format("GetFullPathName failed on path '{}': {}", path, GetLastError()));
  if (length >= MAX_PATH)
    return std::unexpected(IOException::format("GetFullPathName exceeds path buffer on path '{}'", path));
  return utf16ToString(buffer);
}

std::expected<List<std::pair<String, bool>>, StarException> File::directory_entries(String const& dirName,
                                                                                    bool skipDots) {
  auto native = stringToUtf16(relative_to(dirName, "*"));
  if (!native)
    return std::unexpected(std::move(native.error()));
  std::error_code error;
  std::filesystem::path path(native->get());
  path.remove_filename();
  std::filesystem::directory_iterator entry(path, error);
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
  List<std::pair<String, bool>> files;
  // Windows root directories do not expose the special dot entries.
  if (!skipDots && path != path.root_path()) {
    files.push_back({".", true});
    files.push_back({"..", true});
  }
  for (std::filesystem::directory_iterator end; entry != end; entry.increment(error)) {
    if (error)
      return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
    auto name = utf16ToString(entry->path().filename().c_str());
    if (!name)
      return std::unexpected(std::move(name.error()));
    // Keep Windows' directory-reparse-point classification, even for a broken link.
    auto attributes = GetFileAttributesW(entry->path().c_str());
    files.push_back(
      {std::move(*name), attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)});
  }
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
  return files;
}

String File::base_name(String const& fileName) {
  auto const& bytes = fileName.utf8();
  auto end = bytes.find_last_not_of("\\/");
  if (end == std::string::npos)
    return {};
  auto separator = bytes.find_last_of("\\/", end);
  auto start = separator == std::string::npos ? 0 : separator + 1;
  return String(bytes.substr(start, end + 1 - start));
}

String File::dir_name(String const& fileName) {
  if (fileName == "\\" || fileName == "/")
    return "\\";
  auto const& bytes = fileName.utf8();
  auto end = bytes.find_last_not_of("\\/");
  if (end == std::string::npos)
    return ".";
  auto separator = bytes.find_last_of("\\/", end);
  if (separator == std::string::npos)
    return ".";
  auto parentEnd = bytes.find_last_not_of("\\/", separator);
  return parentEnd == std::string::npos ? String(".") : String(bytes.substr(0, parentEnd + 1));
}

String File::relative_to(String const& relativeTo, String const& path) {
  auto const& suffix = path.utf8();
  if ((!suffix.empty() && (suffix[0] == '/' || suffix[0] == '\\'))
      || (suffix.size() >= 2 && ((suffix[0] >= 'a' && suffix[0] <= 'z') || (suffix[0] >= 'A' && suffix[0] <= 'Z')) && suffix[1] == ':'))
    return path;
  auto bytes = relativeTo.utf8();
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
  return relative_to(*root, Star::format("starbound.tmpfile.{}", hexEncode(random.randBytes(16))));
}

std::expected<std::shared_ptr<File>, StarException> File::temporary_file(RandomService& random) {
  for (;;) {
    auto name = temporary_file_name(random);
    if (!name)
      return std::unexpected(std::move(name.error()));
    auto path = stringToUtf16(*name);
    if (!path)
      return std::unexpected(std::move(path.error()));
    auto handle =
      CreateFileW(path->get(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      auto error = GetLastError();
      if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        continue;
      return std::unexpected(IOException::format("Could not create temporary file '{}': {}", *name, error));
    }
    auto file = std::make_shared<File>(std::move(*name));
    file->m_file = handle;
    file->set_mode(IOMode::ReadWrite);
    return file;
  }
}

std::expected<std::shared_ptr<File>, StarException> File::ephemeral_file(RandomService& random) {
  auto file = temporary_file(random);
  if (!file)
    return std::unexpected(std::move(file.error()));
  auto path = stringToUtf16((*file)->file_name());
  if (!path)
    return std::unexpected(std::move(path.error()));
  if (!DeleteFileW(path->get()))
    return std::unexpected(
      IOException::format("Could not delete ephemeral file '{}': {}", (*file)->file_name(), GetLastError()));
  (*file)->m_filename = "";
  return std::move(*file);
}

std::expected<String, StarException> File::temporary_directory(RandomService& random) {
  auto root = temporary_root();
  if (!root)
    return std::unexpected(std::move(root.error()));
  for (;;) {
    auto directory = relative_to(*root, Star::format("starbound.tmpdir.{}", hexEncode(random.randBytes(16))));
    auto path = stringToUtf16(directory);
    if (!path)
      return std::unexpected(std::move(path.error()));
    if (CreateDirectoryW(path->get(), nullptr))
      return directory;
    auto error = GetLastError();
    if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
      return std::unexpected(IOException::format("Could not create temporary directory '{}': {}", directory, error));
  }
}

std::expected<void, StarException> File::rename(String const& source, String const& target) {
  auto sourcePath = stringToUtf16(source);
  if (!sourcePath)
    return std::unexpected(std::move(sourcePath.error()));
  auto targetPath = stringToUtf16(target);
  if (!targetPath)
    return std::unexpected(std::move(targetPath.error()));
  // Do not move or remove the old target before the replacement succeeds.
  // Omitting COPY_ALLOWED also prevents a cross-volume copy/delete fallback.
  if (!MoveFileExW(sourcePath->get(), targetPath->get(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return std::unexpected(IOException::format("Rename error: {}", GetLastError()));
  return {};
}

std::expected<void*, StarException> File::fopen(char const* filename, IOMode mode, bool create, bool exclusive) {
  if (exclusive && !create)
    return std::unexpected(IOException("Exclusive file creation requires create mode"));
  auto path = stringToUtf16(String(std::string_view(filename)));
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
  auto file = CreateFileW(path->get(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                          disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return std::unexpected(IOException::format("could not open file '{}' {}", filename, GetLastError()));
  LARGE_INTEGER zero = {};
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    auto error = GetLastError();
    close_after_failure(file);
    return std::unexpected(IOException::format("could not set file pointer in fopen '{}' {}", filename, error));
  }
  if (mode & IOMode::Truncate && !SetEndOfFile(file)) {
    auto error = GetLastError();
    close_after_failure(file);
    return std::unexpected(IOException::format("could not set end of file in fopen '{}' {}", filename, error));
  }
  if (mode & IOMode::Append) {
    if (!SetFilePointerEx(file, zero, nullptr, FILE_END)) {
      auto error = GetLastError();
      close_after_failure(file);
      return std::unexpected(IOException::format("could not set file pointer in fopen '{}' {}", filename, error));
    }
  }
  return static_cast<void*>(file);
}

std::expected<void, StarException> File::fseek(void* file, StreamOffset offset, IOSeek seek) {
  LARGE_INTEGER distance;
  distance.QuadPart = offset;
  DWORD origin = seek == IOSeek::Relative ? FILE_CURRENT : seek == IOSeek::Absolute ? FILE_BEGIN : FILE_END;
  if (!SetFilePointerEx(file, distance, nullptr, origin))
    return std::unexpected(IOException::format("Seek error {}", GetLastError()));
  return {};
}

std::expected<StreamOffset, StarException> File::ftell(void* file) {
  LARGE_INTEGER zero = {};
  LARGE_INTEGER position;
  if (!SetFilePointerEx(file, zero, &position, FILE_CURRENT))
    return std::unexpected(IOException::format("Tell error {}", GetLastError()));
  return position.QuadPart;
}

std::expected<std::size_t, StarException> File::fread(void* file, char* data, std::size_t len) {
  if (!len)
    return 0;
  DWORD count = 0;
  if (!ReadFile(file, data, static_cast<DWORD>(std::min<std::size_t>(len, MAXDWORD)), &count, nullptr))
    return std::unexpected(IOException::format("read error {}", GetLastError()));
  return count;
}

std::expected<std::size_t, StarException> File::fwrite(void* file, char const* data, std::size_t len) {
  if (!len)
    return 0;
  DWORD count = 0;
  if (!WriteFile(file, data, static_cast<DWORD>(std::min<std::size_t>(len, MAXDWORD)), &count, nullptr))
    return std::unexpected(IOException::format("write error {}", GetLastError()));
  return count;
}

std::expected<void, StarException> File::fsync(void* file) {
  if (!FlushFileBuffers(file))
    return std::unexpected(IOException::format("fsync error {}", GetLastError()));
  return {};
}

std::expected<void, StarException> File::fclose(void* file) {
  if (!CloseHandle(file))
    return std::unexpected(IOException::format("Close error {}", GetLastError()));
  return {};
}

std::expected<StreamOffset, StarException> File::fsize(void* file) {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(file, &size))
    return std::unexpected(IOException::format("could not get file size in fsize {}", GetLastError()));
  if (size.QuadPart < 0)
    return std::unexpected(IOException("File size cannot be represented as a stream offset"));
  return checked_size(static_cast<std::uint64_t>(size.QuadPart));
}

std::expected<std::size_t, StarException> File::pread(void* file, char* data, std::size_t len, StreamOffset position) {
  if (position < 0)
    return std::unexpected(IOException("Cannot read at a negative file position"));
  if (!len)
    return 0;
  auto cursor = ftell(file);
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  DWORD count = 0;
  auto overlapped = make_overlapped(position);
  bool read = ReadFile(file, data, static_cast<DWORD>(std::min<std::size_t>(len, MAXDWORD)), &count, &overlapped);
  auto error = read ? ERROR_SUCCESS : GetLastError();
  auto restored = fseek(file, *cursor, IOSeek::Absolute);
  if (!read && error != ERROR_HANDLE_EOF) {
    if (!restored) {
      if (auto written = Star::print(stderr, "Error restoring file cursor after failed pread: {}\n", restored.error().what()); !written)
        return std::unexpected(IOException::format("pread error {}", error).with_context(
            Star::format("Could not report cursor restoration failure ({}): {}", restored.error().what(), written.error().what())));
    }
    return std::unexpected(IOException::format("pread error {}", error));
  }
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return count;
}

std::expected<std::size_t, StarException> File::pwrite(void* file, char const* data, std::size_t len, StreamOffset position) {
  if (position < 0)
    return std::unexpected(IOException("Cannot write at a negative file position"));
  if (!len)
    return 0;
  auto cursor = ftell(file);
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  DWORD count = 0;
  auto overlapped = make_overlapped(position);
  bool written = WriteFile(file, data, static_cast<DWORD>(std::min<std::size_t>(len, MAXDWORD)), &count, &overlapped);
  auto error = written ? ERROR_SUCCESS : GetLastError();
  auto restored = fseek(file, *cursor, IOSeek::Absolute);
  if (!written) {
    if (!restored) {
      if (auto written = Star::print(stderr, "Error restoring file cursor after failed pwrite: {}\n", restored.error().what()); !written)
        return std::unexpected(IOException::format("pwrite error {}", error).with_context(
            Star::format("Could not report cursor restoration failure ({}): {}", restored.error().what(), written.error().what())));
    }
    return std::unexpected(IOException::format("pwrite error {}", error));
  }
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return count;
}

std::expected<void, StarException> File::resize(void* file, StreamOffset size) {
  if (size < 0)
    return std::unexpected(IOException("Cannot resize a file to a negative size"));
  auto cursor = ftell(file);
  if (!cursor)
    return std::unexpected(std::move(cursor.error()));
  auto moved = fseek(file, size, IOSeek::Absolute);
  if (!moved)
    return std::unexpected(std::move(moved.error()));
  auto resized = SetEndOfFile(file);
  auto error = resized ? ERROR_SUCCESS : GetLastError();
  auto restored = fseek(file, *cursor, IOSeek::Absolute);
  if (!resized) {
    if (!restored) {
      if (auto written = Star::print(stderr, "Error restoring file cursor after failed resize: {}\n", restored.error().what()); !written)
        return std::unexpected(IOException::format("resize error {}", error).with_context(
            Star::format("Could not report cursor restoration failure ({}): {}", restored.error().what(), written.error().what())));
    }
    return std::unexpected(IOException::format("resize error {}", error));
  }
  return restored;
}

}
