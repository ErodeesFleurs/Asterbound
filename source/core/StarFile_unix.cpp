#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>

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
int fd_from_handle(void* ptr) { return (int)((std::intptr_t)ptr - 1); }

void* handle_from_fd(int handle) { return (void*)((std::intptr_t)handle + 1); }

void close_after_failure(int fd) {
  if (::close(fd) < 0) {
    auto error = errno;
    std::fprintf(stderr, "Error closing file after failed acquisition: %s\n", std::strerror(error));
  }
}

std::expected<std::optional<struct stat>, StarException> path_status(String const& path, bool follow_links) {
  struct stat status;
  int result;
  do {
    result = follow_links ? ::stat(path.utf8_data(), &status) : ::lstat(path.utf8_data(), &status);
  } while (result < 0 && errno == EINTR);
  if (result == 0)
    return status;
  auto error = errno;
  if (error == ENOENT || error == ENOTDIR)
    return std::optional<struct stat>{};
  return std::unexpected(IOException::format("Error inspecting path '{}': {}", path, std::strerror(error)));
}

std::expected<StreamOffset, StarException> checked_size(off_t size) {
  if (!std::in_range<StreamOffset>(size) || size < 0)
    return std::unexpected(IOException("File size cannot be represented as a stream offset"));
  return static_cast<StreamOffset>(size);
}
}// namespace

String File::convert_dir_separators(String const& path) {
  auto bytes = path.utf8();
  std::replace(bytes.begin(), bytes.end(), '\\', '/');
  return String(std::move(bytes));
}

std::expected<List<std::pair<String, bool>>, StarException> File::directory_entries(String const& dirName,
                                                                                    bool skipDots) {
  std::error_code error;
  std::filesystem::directory_iterator entry(std::filesystem::path(dirName.utf8_data()), error);
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
  List<std::pair<String, bool>> files;
  // Standard directory iterators omit these entries unconditionally.
  if (!skipDots) {
    files.push_back({".", true});
    auto parent = is_directory(relative_to(dirName, ".."));
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    files.push_back({"..", *parent});
  }
  for (std::filesystem::directory_iterator end; entry != end; entry.increment(error)) {
    if (error)
      return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
    auto directory = entry->is_directory(error);
    // Native stat failures for an entry have always classified it as non-directory.
    error.clear();
    files.push_back({String(entry->path().filename().native()), directory});
  }
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dirName, error.message()));
  return files;
}

String File::base_name(String const& fileName) {
  auto bytes = fileName.utf8();
  return String(std::string_view(::basename(bytes.data())));
}

String File::dir_name(String const& fileName) {
  auto bytes = fileName.utf8();
  return String(std::string_view(::dirname(bytes.data())));
}

String File::relative_to(String const& relativeTo, String const& path) {
  if (path.utf8_size() && path.utf8_data()[0] == '/')
    return path;
  auto bytes = relativeTo.utf8();
  while (!bytes.empty() && bytes.back() == '/')
    bytes.pop_back();
  bytes.push_back('/');
  bytes.append(path.utf8());
  return String(std::move(bytes));
}

std::expected<String, StarException> File::full_path(String const& fileName) {
  std::error_code error;
  auto path = std::filesystem::canonical(std::filesystem::path(fileName.utf8_data()), error);
  if (error)
    return std::unexpected(IOException::format("could not resolve file '{}': {}", fileName, error.message()));
  return String(path.native());
}

std::expected<String, StarException> File::temporary_file_name(RandomService& random) {
  return relative_to(P_tmpdir, strf("starbound.tmpfile.{}", hexEncode(random.randBytes(16))));
}

std::expected<std::shared_ptr<File>, StarException> File::temporary_file(RandomService& random) {
  for (;;) {
    auto path = temporary_file_name(random);
    if (!path)
      return std::unexpected(std::move(path.error()));
    int fd;
    do {
      fd = ::open(path->utf8_data(), O_RDWR | O_CREAT | O_EXCL, 0666);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
      auto error = errno;
      if (error == EEXIST)
        continue;
      return std::unexpected(
        IOException::format("Could not create temporary file '{}': {}", *path, std::strerror(error)));
    }
    auto file = std::make_shared<File>(std::move(*path));
    file->m_file = handle_from_fd(fd);
    file->set_mode(IOMode::ReadWrite);
    return file;
  }
}

std::expected<std::shared_ptr<File>, StarException> File::ephemeral_file(RandomService&) {
  auto file = std::make_shared<File>();
  auto filename_template = relative_to(P_tmpdir, "starbound.tmpfile.XXXXXXXX");
  auto path = ByteArray::fromCStringWithNull(filename_template.utf8_data());
  auto fd = ::mkstemp(path.ptr());
  if (fd < 0)
    return std::unexpected(IOException::format("tmpfile error: {}", std::strerror(errno)));
  if (::unlink(path.ptr()) < 0) {
    auto error = errno;
    close_after_failure(fd);
    return std::unexpected(IOException::format("Could not remove mkstemp file when creating ephemeralFile: {}", std::strerror(error)));
  }
  file->m_file = handle_from_fd(fd);
  file->set_mode(IOMode::ReadWrite);
  return file;
}

std::expected<String, StarException> File::temporary_directory(RandomService& random) {
  for (;;) {
    auto directory = relative_to(P_tmpdir, strf("starbound.tmpdir.{}", hexEncode(random.randBytes(16))));
    int result;
    do {
      result = ::mkdir(directory.utf8_data(), 0777);
    } while (result < 0 && errno == EINTR);
    if (result == 0)
      return directory;
    auto error = errno;
    if (error != EEXIST)
      return std::unexpected(
        IOException::format("Could not create temporary directory '{}': {}", directory, std::strerror(error)));
  }
}

std::expected<bool, StarException> File::exists(String const& path) {
  auto status = path_status(path, true);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return status->has_value();
}

std::expected<bool, StarException> File::is_regular_file(String const& path) {
  auto status = path_status(path, true);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return *status && S_ISREG((**status).st_mode);
}

std::expected<bool, StarException> File::is_directory(String const& path) {
  auto status = path_status(path, true);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return *status && S_ISDIR((**status).st_mode);
}

std::expected<bool, StarException> File::is_symlink(String const& path) {
  auto status = path_status(path, false);
  if (!status)
    return std::unexpected(std::move(status.error()));
  return *status && S_ISLNK((**status).st_mode);
}

std::expected<StreamOffset, StarException> File::file_size(String const& path) {
  int fd;
  do {
    fd = ::open(path.utf8_data(), O_RDONLY | O_NONBLOCK);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0)
    return std::unexpected(
      IOException::format("Could not open file '{}' for metadata: {}", path, std::strerror(errno)));
  auto handle = handle_from_fd(fd);
  auto size = fsize(handle);
  auto closed = fclose(handle);
  if (!size) {
    if (!closed)
      std::fprintf(stderr, "Error closing file '%s' after failed metadata query: %s\n", path.utf8_data(),
                   closed.error().what());
    return std::unexpected(std::move(size.error()));
  }
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  return size;
}

std::expected<void, StarException> File::rename(String const& source, String const& target) {
  if (std::rename(source.utf8_data(), target.utf8_data()) < 0)
    return std::unexpected(IOException::format("rename error: {}", std::strerror(errno)));
  return {};
}

std::expected<void, StarException> File::sync_directory(String const& directory) {
  int fd;
  do {
    fd = ::open(directory.utf8_data(), O_RDONLY | O_DIRECTORY);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0)
    return std::unexpected(
      IOException::format("Could not open directory '{}' for sync: {}", directory, std::strerror(errno)));
  int result;
  do {
    result = ::fsync(fd);
  } while (result < 0 && errno == EINTR);
  if (result < 0) {
    auto error = errno;
    close_after_failure(fd);
    return std::unexpected(IOException::format("Could not sync directory '{}': {}", directory, std::strerror(error)));
  }
  return fclose(handle_from_fd(fd));
}

std::expected<void*, StarException> File::fopen(char const* filename, IOMode mode, bool create, bool exclusive) {
  if (exclusive && !create)
    return std::unexpected(IOException("Exclusive file creation requires create mode"));
  int flags = 0;
  if (mode & IOMode::Read && mode & IOMode::Write)
    flags |= O_RDWR;
  else if (mode & IOMode::Read)
    flags |= O_RDONLY;
  else if (mode & IOMode::Write)
    flags |= O_WRONLY;
  else
    return std::unexpected(IOException("Cannot open file with closed IO mode"));
  if (create && mode & IOMode::Write)
    flags |= O_CREAT;
  if (exclusive)
    flags |= O_CREAT | O_EXCL;
  if (mode & IOMode::Truncate)
    flags |= O_TRUNC;
  int fd;
  do {
    fd = ::open(filename, flags, 0666);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0)
    return std::unexpected(IOException::format("Error opening file '{}', error: {}", filename, std::strerror(errno)));
  struct stat status;
  int inspected;
  do {
    inspected = ::fstat(fd, &status);
  } while (inspected < 0 && errno == EINTR);
  if (inspected < 0) {
    auto error = errno;
    close_after_failure(fd);
    return std::unexpected(IOException::format("Error inspecting file '{}', error: {}", filename, std::strerror(error)));
  }
  if (S_ISDIR(status.st_mode)) {
    close_after_failure(fd);
    return std::unexpected(IOException::format("Cannot open directory '{}' as a file", filename));
  }
  if (mode & IOMode::Append && ::lseek(fd, 0, SEEK_END) < 0) {
    auto error = errno;
    close_after_failure(fd);
    return std::unexpected(IOException::format("Error opening file '{}', cannot seek: {}", filename, std::strerror(error)));
  }
  return handle_from_fd(fd);
}

std::expected<void, StarException> File::fseek(void* file, StreamOffset offset, IOSeek seek) {
  int origin = seek == IOSeek::Relative ? SEEK_CUR : seek == IOSeek::Absolute ? SEEK_SET : SEEK_END;
  if (::lseek(fd_from_handle(file), offset, origin) < 0)
    return std::unexpected(IOException::format("Seek error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::ftell(void* file) {
  auto position = ::lseek(fd_from_handle(file), 0, SEEK_CUR);
  if (position < 0)
    return std::unexpected(IOException::format("Tell error: {}", std::strerror(errno)));
  return position;
}

std::expected<std::size_t, StarException> File::fread(void* file, char* data, std::size_t len) {
  if (len == 0)
    return 0;
  ssize_t result;
  do {
    result =
      ::read(fd_from_handle(file), data, std::min(len, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())));
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  return result;
}

std::expected<std::size_t, StarException> File::fwrite(void* file, char const* data, std::size_t len) {
  if (len == 0)
    return 0;
  ssize_t result;
  do {
    result =
      ::write(fd_from_handle(file), data, std::min(len, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())));
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  return result;
}

std::expected<void, StarException> File::fsync(void* file) {
  int result;
  do {
#ifdef STAR_SYSTEM_LINUX
    result = ::fdatasync(fd_from_handle(file));
#else
    result = ::fsync(fd_from_handle(file));
#endif
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Sync error: {}", std::strerror(errno)));
  return {};
}

std::expected<void, StarException> File::fclose(void* file) {
  if (::close(fd_from_handle(file)) < 0)
    return std::unexpected(IOException::format("Close error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::fsize(void* file) {
  struct stat status;
  int result;
  do {
    result = ::fstat(fd_from_handle(file), &status);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("File size error: {}", std::strerror(errno)));
  // Pipes, sockets, and native devices do not expose a portable byte length.
  // In particular, their st_size can be zero without denoting EOF.
  if (!S_ISREG(status.st_mode))
    return std::unexpected(IOException("Cannot query size of a nonregular file device"));
  return checked_size(status.st_size);
}

std::expected<std::size_t, StarException> File::pread(void* file, char* data, std::size_t len, StreamOffset position) {
  ssize_t result;
  do {
    result = ::pread(fd_from_handle(file), data,
                     std::min(len, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())), position);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  return result;
}

std::expected<std::size_t, StarException> File::pwrite(void* file, char const* data, std::size_t len, StreamOffset position) {
  ssize_t result;
  do {
    result = ::pwrite(fd_from_handle(file), data,
                      std::min(len, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())), position);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  return result;
}

std::expected<void, StarException> File::resize(void* file, StreamOffset size) {
  int result;
  do {
    result = ::ftruncate(fd_from_handle(file), size);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("resize error: {}", std::strerror(errno)));
  return {};
}

}
