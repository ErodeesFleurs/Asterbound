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
  int fdFromHandle(void* ptr) {
    return (int)((std::intptr_t)ptr - 1);
  }

  void* handleFromFd(int handle) {
    return (void*)((std::intptr_t)handle + 1);
  }

  void closeAfterFailure(int fd) {
    if (::close(fd) < 0) {
      auto error = errno;
      std::fprintf(stderr, "Error closing file after failed acquisition: %s\n", std::strerror(error));
    }
  }
}

String File::convertDirSeparators(String const& path) {
  auto bytes = path.utf8();
  std::replace(bytes.begin(), bytes.end(), '\\', '/');
  return String(std::move(bytes));
}

std::expected<List<std::pair<String, bool>>, StarException> File::dirList(String const& dirName, bool skipDots) {
  std::error_code error;
  std::filesystem::directory_iterator entry(std::filesystem::path(dirName.utf8Ptr()), error);
  if (error)
    return std::unexpected(IOException::format("dirList failed on dir '{}': {}", dirName, error.message()));
  List<std::pair<String, bool>> files;
  // Standard directory iterators omit these entries unconditionally.
  if (!skipDots) {
    files.append({".", true});
    auto parent = isDirectory(relativeTo(dirName, ".."));
    if (!parent)
      return std::unexpected(std::move(parent.error()));
    files.append({"..", *parent});
  }
  for (std::filesystem::directory_iterator end; entry != end; entry.increment(error)) {
    if (error)
      return std::unexpected(IOException::format("dirList failed on dir '{}': {}", dirName, error.message()));
    auto directory = entry->is_directory(error);
    // Native stat failures for an entry have always classified it as non-directory.
    error.clear();
    files.append({String(entry->path().filename().native()), directory});
  }
  if (error)
    return std::unexpected(IOException::format("dirList failed on dir '{}': {}", dirName, error.message()));
  return files;
}

String File::baseName(String const& fileName) {
  auto bytes = fileName.utf8();
  return String(std::string_view(::basename(bytes.data())));
}

String File::dirName(String const& fileName) {
  auto bytes = fileName.utf8();
  return String(std::string_view(::dirname(bytes.data())));
}

String File::relativeTo(String const& relativeTo, String const& path) {
  if (path.utf8Size() && path.utf8Ptr()[0] == '/')
    return path;
  auto bytes = relativeTo.utf8();
  while (!bytes.empty() && bytes.back() == '/')
    bytes.pop_back();
  bytes.push_back('/');
  bytes.append(path.utf8());
  return String(std::move(bytes));
}

std::expected<String, StarException> File::fullPath(String const& fileName) {
  std::error_code error;
  auto path = std::filesystem::canonical(std::filesystem::path(fileName.utf8Ptr()), error);
  if (error)
    return std::unexpected(IOException::format("could not resolve file '{}': {}", fileName, error.message()));
  return String(path.native());
}

std::expected<String, StarException> File::temporaryFileName(RandomService& random) {
  return relativeTo(P_tmpdir, strf("starbound.tmpfile.{}", hexEncode(random.randBytes(16))));
}

std::expected<std::shared_ptr<File>, StarException> File::temporaryFile(RandomService& random) {
  auto path = temporaryFileName(random);
  if (!path)
    return std::unexpected(std::move(path.error()));
  return open(*path, IOMode::ReadWrite);
}

std::expected<std::shared_ptr<File>, StarException> File::ephemeralFile(RandomService&) {
  auto file = std::make_shared<File>();
  auto path = ByteArray::fromCStringWithNull(relativeTo(P_tmpdir, "starbound.tmpfile.XXXXXXXX").utf8Ptr());
  auto fd = ::mkstemp(path.ptr());
  if (fd < 0)
    return std::unexpected(IOException::format("tmpfile error: {}", std::strerror(errno)));
  if (::unlink(path.ptr()) < 0) {
    auto error = errno;
    closeAfterFailure(fd);
    return std::unexpected(IOException::format("Could not remove mkstemp file when creating ephemeralFile: {}", std::strerror(error)));
  }
  file->m_file = handleFromFd(fd);
  file->setMode(IOMode::ReadWrite);
  return file;
}

std::expected<String, StarException> File::temporaryDirectory(RandomService& random) {
  String directory = relativeTo(P_tmpdir, strf("starbound.tmpdir.{}", hexEncode(random.randBytes(16))));
  auto made = makeDirectory(directory);
  if (!made)
    return std::unexpected(std::move(made.error()));
  return directory;
}

std::expected<bool, StarException> File::exists(String const& path) {
  std::error_code error;
  return std::filesystem::exists(std::filesystem::path(path.utf8Ptr()), error);
}

std::expected<bool, StarException> File::isFile(String const& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(std::filesystem::path(path.utf8Ptr()), error);
}

std::expected<bool, StarException> File::isDirectory(String const& path) {
  std::error_code error;
  return std::filesystem::is_directory(std::filesystem::path(path.utf8Ptr()), error);
}

std::expected<bool, StarException> File::isSymlink(String const& path) {
  std::error_code error;
  return std::filesystem::is_symlink(std::filesystem::path(path.utf8Ptr()), error);
}

std::expected<void, StarException> File::rename(String const& source, String const& target) {
  if (std::rename(source.utf8Ptr(), target.utf8Ptr()) < 0)
    return std::unexpected(IOException::format("rename error: {}", std::strerror(errno)));
  return {};
}

std::expected<void, StarException> File::overwriteFileWithRename(std::span<char const> data, String const& filename, String const& newSuffix) {
  auto newFile = filename + newSuffix;
  auto written = writeFile(data, newFile);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return rename(newFile, filename);
}

std::expected<void*, StarException> File::fopen(char const* filename, IOMode mode) {
  int flags = 0;
  if (mode & IOMode::Read && mode & IOMode::Write)
    flags |= O_RDWR | O_CREAT;
  else if (mode & IOMode::Read)
    flags |= O_RDONLY;
  else if (mode & IOMode::Write)
    flags |= O_WRONLY | O_CREAT;
  else
    return std::unexpected(IOException("Cannot open file with closed IO mode"));
  if (mode & IOMode::Truncate)
    flags |= O_TRUNC;
  int fd = ::open(filename, flags, 0666);
  if (fd < 0)
    return std::unexpected(IOException::format("Error opening file '{}', error: {}", filename, std::strerror(errno)));
  struct stat status;
  if (::fstat(fd, &status) < 0) {
    auto error = errno;
    closeAfterFailure(fd);
    return std::unexpected(IOException::format("Error inspecting file '{}', error: {}", filename, std::strerror(error)));
  }
  if (S_ISDIR(status.st_mode)) {
    closeAfterFailure(fd);
    return std::unexpected(IOException::format("Cannot open directory '{}' as a file", filename));
  }
  if (mode & IOMode::Append && ::lseek(fd, 0, SEEK_END) < 0) {
    auto error = errno;
    closeAfterFailure(fd);
    return std::unexpected(IOException::format("Error opening file '{}', cannot seek: {}", filename, std::strerror(error)));
  }
  return handleFromFd(fd);
}

std::expected<void, StarException> File::fseek(void* file, StreamOffset offset, IOSeek seek) {
  int origin = seek == IOSeek::Relative ? SEEK_CUR : seek == IOSeek::Absolute ? SEEK_SET : SEEK_END;
  if (::lseek(fdFromHandle(file), offset, origin) < 0)
    return std::unexpected(IOException::format("Seek error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::ftell(void* file) {
  auto position = ::lseek(fdFromHandle(file), 0, SEEK_CUR);
  if (position < 0)
    return std::unexpected(IOException::format("Tell error: {}", std::strerror(errno)));
  return position;
}

std::expected<std::size_t, StarException> File::fread(void* file, char* data, std::size_t len) {
  if (len == 0)
    return 0;
  auto result = ::read(fdFromHandle(file), data, len);
  if (result < 0) {
    if (errno == EAGAIN || errno == EINTR)
      return 0;
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  }
  return result;
}

std::expected<std::size_t, StarException> File::fwrite(void* file, char const* data, std::size_t len) {
  if (len == 0)
    return 0;
  auto result = ::write(fdFromHandle(file), data, len);
  if (result < 0) {
    if (errno == EAGAIN || errno == EINTR)
      return 0;
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  }
  return result;
}

std::expected<void, StarException> File::fsync(void* file) {
#ifdef STAR_SYSTEM_LINUX
  auto result = ::fdatasync(fdFromHandle(file));
#else
  auto result = ::fsync(fdFromHandle(file));
#endif
  if (result < 0)
    return std::unexpected(IOException::format("Sync error: {}", std::strerror(errno)));
  return {};
}

std::expected<void, StarException> File::fclose(void* file) {
  if (::close(fdFromHandle(file)) < 0)
    return std::unexpected(IOException::format("Close error: {}", std::strerror(errno)));
  return {};
}

std::expected<StreamOffset, StarException> File::fsize(void* file) {
  auto position = ftell(file);
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto size = ::lseek(fdFromHandle(file), 0, SEEK_END);
  auto error = size < 0 ? errno : 0;
  auto restored = fseek(file, *position, IOSeek::Absolute);
  if (error) {
    if (!restored)
      std::fprintf(stderr, "Error restoring file cursor after failed size query: %s\n", restored.error().what());
    return std::unexpected(IOException::format("File size error: {}", std::strerror(error)));
  }
  if (!restored)
    return std::unexpected(std::move(restored.error()));
  return size;
}

std::expected<std::size_t, StarException> File::pread(void* file, char* data, std::size_t len, StreamOffset position) {
  auto result = ::pread(fdFromHandle(file), data, len, position);
  if (result < 0) {
    if (errno == EAGAIN || errno == EINTR)
      return 0;
    return std::unexpected(IOException::format("Read error: {}", std::strerror(errno)));
  }
  return result;
}

std::expected<std::size_t, StarException> File::pwrite(void* file, char const* data, std::size_t len, StreamOffset position) {
  auto result = ::pwrite(fdFromHandle(file), data, len, position);
  if (result < 0) {
    if (errno == EAGAIN || errno == EINTR)
      return 0;
    return std::unexpected(IOException::format("Write error: {}", std::strerror(errno)));
  }
  return result;
}

std::expected<void, StarException> File::resize(void* file, StreamOffset size) {
  if (::ftruncate(fdFromHandle(file), size) < 0)
    return std::unexpected(IOException::format("resize error: {}", std::strerror(errno)));
  return {};
}

}
