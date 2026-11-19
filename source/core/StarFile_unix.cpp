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
int open_descriptor(String const& path, int flags) {
  int fd;
  do {
    fd = ::open(path.utf8_data(), flags, 0666);
  } while (fd < 0 && errno == EINTR);
  return fd;
}

std::size_t transfer_size(std::size_t size) {
  return std::min(size, static_cast<std::size_t>(std::numeric_limits<ssize_t>::max()));
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

std::expected<List<std::pair<String, bool>>, StarException> File::directory_entries(String const& dir_name,
                                                                                    bool skip_dots) {
  std::error_code error;
  std::filesystem::directory_iterator entry(std::filesystem::path(dir_name.utf8_data()), error);
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
  List<std::pair<String, bool>> files;
  // Standard directory iterators omit these entries unconditionally.
  if (!skip_dots) {
    files.push_back({".", true});
    auto parent = is_directory(relative_to(dir_name, ".."));
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    files.push_back({"..", *parent});
  }
  for (std::filesystem::directory_iterator end; entry != end; entry.increment(error)) {
    if (error)
      return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
    auto directory = entry->is_directory(error);
    // Native stat failures for an entry have always classified it as non-directory.
    error.clear();
    files.push_back({String(entry->path().filename().native()), directory});
  }
  if (error)
    return std::unexpected(IOException::format("directory_entries failed on dir '{}': {}", dir_name, error.message()));
  return files;
}

String File::base_name(String const& filename) {
  auto bytes = filename.utf8();
  return String(std::string_view(::basename(bytes.data())));
}

String File::dir_name(String const& filename) {
  auto bytes = filename.utf8();
  return String(std::string_view(::dirname(bytes.data())));
}

String File::relative_to(String const& relative_to_path, String const& path) {
  if (path.utf8_size() && path.utf8_data()[0] == '/')
    return path;
  auto bytes = relative_to_path.utf8();
  while (!bytes.empty() && bytes.back() == '/')
    bytes.pop_back();
  bytes.push_back('/');
  bytes.append(path.utf8());
  return String(std::move(bytes));
}

std::expected<String, StarException> File::full_path(String const& filename) {
  std::error_code error;
  auto path = std::filesystem::canonical(std::filesystem::path(filename.utf8_data()), error);
  if (error)
    return std::unexpected(IOException::format("could not resolve file '{}': {}", filename, error.message()));
  return String(path.native());
}

std::expected<String, StarException> File::temporary_file_name(RandomService& random) {
  return relative_to(P_tmpdir, Star::format("starbound.tmpfile.{}", hex_encode(random.random_bytes(16))));
}

std::expected<std::shared_ptr<File>, StarException> File::temporary_file(RandomService& random) {
  for (;;) {
    auto path = temporary_file_name(random);
    if (!path)
      return std::unexpected(std::move(path.error()));
    NativeFileHandle handle(open_descriptor(*path, O_RDWR | O_CREAT | O_EXCL));
    if (!handle) {
      auto error = errno;
      if (error == EEXIST)
        continue;
      return std::unexpected(
        IOException::format("Could not create temporary file '{}': {}", *path, std::strerror(error)));
    }
    auto file = std::make_shared<File>(std::move(*path));
    file->m_file.reset(handle.release());
    file->set_mode(IOMode::ReadWrite);
    return file;
  }
}

std::expected<std::shared_ptr<File>, StarException> File::ephemeral_file(RandomService&) {
  auto file = std::make_shared<File>();
  auto filename_template = relative_to(P_tmpdir, "starbound.tmpfile.XXXXXXXX");
  auto path = ByteArray::from_string_with_null(filename_template.utf8_data());
  NativeFileHandle handle(::mkstemp(path.data()));
  if (!handle)
    return std::unexpected(IOException::format("tmpfile error: {}", std::strerror(errno)));
  if (::unlink(path.data()) < 0)
    return std::unexpected(IOException::format("Could not remove mkstemp file when creating ephemeralFile: {}",
                                               std::strerror(errno)));
  file->m_file.reset(handle.release());
  file->set_mode(IOMode::ReadWrite);
  return file;
}

std::expected<String, StarException> File::temporary_directory(RandomService& random) {
  for (;;) {
    auto directory = relative_to(P_tmpdir, Star::format("starbound.tmpdir.{}", hex_encode(random.random_bytes(16))));
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
  NativeFileHandle handle(open_descriptor(path, O_RDONLY | O_NONBLOCK));
  if (!handle)
    return std::unexpected(
      IOException::format("Could not open file '{}' for metadata: {}", path, std::strerror(errno)));
  auto size = handle.size();
  auto closed = handle.close();
  if (!size) {
    if (!closed) {
      if (auto written = Star::print(stderr, "Error closing file '{}' after failed metadata query: {}\n", path.utf8_data(),
                   closed.error().what()); !written)
        return std::unexpected(std::move(size.error()).with_context(
            Star::format("Could not report close failure ({}): {}", closed.error().what(), written.error().what())));
    }
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
  NativeFileHandle handle(open_descriptor(directory, O_RDONLY | O_DIRECTORY));
  if (!handle)
    return std::unexpected(
      IOException::format("Could not open directory '{}' for sync: {}", directory, std::strerror(errno)));
  int result;
  do {
    result = ::fsync(handle.get());
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Could not sync directory '{}': {}", directory, std::strerror(errno)));
  return handle.close();
}

File::NativeFileHandle::~NativeFileHandle() noexcept {
  if (*this && ::close(release()) < 0) {
    auto error = errno;
    if (!Star::print(stderr, "Error closing file after failed acquisition: {}\n", std::strerror(error)))
      std::_Exit(1);
  }
}

std::expected<File::NativeFileHandle, StarException>
File::NativeFileHandle::open(String const& filename, IOMode mode, bool create, bool exclusive) {
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
  NativeFileHandle handle(open_descriptor(filename, flags));
  if (!handle)
    return std::unexpected(IOException::format("Error opening file '{}', error: {}", filename, std::strerror(errno)));
  struct stat status;
  int inspected;
  do {
    inspected = ::fstat(handle.get(), &status);
  } while (inspected < 0 && errno == EINTR);
  if (inspected < 0)
    return std::unexpected(IOException::format("Error inspecting file '{}', error: {}", filename, std::strerror(errno)));
  if (S_ISDIR(status.st_mode))
    return std::unexpected(IOException::format("Cannot open directory '{}' as a file", filename));
  if (mode & IOMode::Append && ::lseek(handle.get(), 0, SEEK_END) < 0)
    return std::unexpected(IOException::format("Error opening file '{}', cannot seek: {}", filename, std::strerror(errno)));
  return handle;
}

std::expected<void, StarException> File::NativeFileHandle::seek(StreamOffset offset, IOSeek seek_mode) {
  int origin = seek_mode == IOSeek::Relative ? SEEK_CUR : seek_mode == IOSeek::Absolute ? SEEK_SET : SEEK_END;
  if (::lseek(get(), offset, origin) < 0)
    return std::unexpected(IOException::format("Seek error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::NativeFileHandle::pos() {
  auto position = ::lseek(get(), 0, SEEK_CUR);
  if (position < 0)
    return std::unexpected(IOException::format("Tell error: {}", std::strerror(errno)));
  return position;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::read(std::span<char> data) {
  if (data.empty())
    return 0;
  ssize_t result;
  do {
    result = ::read(get(), data.data(), transfer_size(data.size()));
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  return result;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::write(std::span<char const> data) {
  if (data.empty())
    return 0;
  ssize_t result;
  do {
    result = ::write(get(), data.data(), transfer_size(data.size()));
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  return result;
}

std::expected<void, StarException> File::NativeFileHandle::sync() {
  int result;
  do {
#ifdef STAR_SYSTEM_LINUX
    result = ::fdatasync(get());
#else
    result = ::fsync(get());
#endif
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Sync error: {}", std::strerror(errno)));
  return {};
}

std::expected<void, StarException> File::NativeFileHandle::close() {
  // Never retry close, even on EINTR: the descriptor may already be reused.
  if (*this && ::close(release()) < 0)
    return std::unexpected(IOException::format("Close error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::NativeFileHandle::size() {
  struct stat status;
  int result;
  do {
    result = ::fstat(get(), &status);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("File size error: {}", std::strerror(errno)));
  // Pipes, sockets, and native devices do not expose a portable byte length.
  // In particular, their st_size can be zero without denoting EOF.
  if (!S_ISREG(status.st_mode))
    return std::unexpected(IOException("Cannot query size of a nonregular file device"));
  return checked_size(status.st_size);
}

std::expected<std::size_t, StarException> File::NativeFileHandle::read_at(StreamOffset position, std::span<char> data) {
  ssize_t result;
  do {
    result = ::pread(get(), data.data(), transfer_size(data.size()), position);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  return result;
}

std::expected<std::size_t, StarException> File::NativeFileHandle::write_at(StreamOffset position, std::span<char const> data) {
  ssize_t result;
  do {
    result = ::pwrite(get(), data.data(), transfer_size(data.size()), position);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  return result;
}

std::expected<void, StarException> File::NativeFileHandle::resize(StreamOffset size) {
  int result;
  do {
    result = ::ftruncate(get(), size);
  } while (result < 0 && errno == EINTR);
  if (result < 0)
    return std::unexpected(IOException::format("resize error: {}", std::strerror(errno)));
  return {};
}

}
