#include <stdio.h>

import star.file;
import star.io_device;
import star.string;
import star.format;
import std;
import star.byte_array;
import star.algorithm;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
import star.string_windows;
#endif

namespace Star {

namespace {
std::expected<std::filesystem::path, StarException> filesystem_path(String const& path) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto native = stringToUtf16(path);
    if (!native)
      return std::unexpected(std::move(native.error()));
    return std::filesystem::path(native->get());
#else
    return std::filesystem::path(path.utf8_data());
#endif
}

std::expected<std::size_t, StarException> read_file_tail(File& file, ByteArray& bytes, std::size_t amount, char first) {
  auto offset = bytes.size();
  bytes.resize(offset + amount);
  bytes.span()[offset] = first;
  auto count = file.read(bytes.span().subspan(offset + 1, amount - 1));
  bytes.resize(offset + 1 + (count ? *count : 0));
  if (!count)
    return std::unexpected(std::move(count.error()));
  return 1 + *count;
}

std::expected<std::size_t, StarException> read_file_tail(File& file, std::string& text, std::size_t amount,
                                                         char first) {
  auto offset = text.size();
  std::expected<std::size_t, StarException> count = 0;
  text.resize_and_overwrite(offset + amount, [&](char* storage, std::size_t) {
    storage[offset] = first;
    count = file.read(std::span<char>(storage + offset + 1, amount - 1));
    return offset + 1 + (count ? *count : 0);
  });
  if (!count)
    return std::unexpected(std::move(count.error()));
  return 1 + *count;
}

template <typename Buffer>
concept whole_file_buffer = std::default_initializable<Buffer> && std::movable<Buffer>
  && requires(Buffer& buffer, Buffer const& contents, File& file, std::size_t amount) {
       { contents.size() } -> std::same_as<std::size_t>;
       { contents.capacity() } -> std::same_as<std::size_t>;
       { buffer.reserve(amount) } -> std::same_as<void>;
       { read_file_tail(file, buffer, amount, char{}) } -> std::same_as<std::expected<std::size_t, StarException>>;
     };

template <whole_file_buffer Buffer> std::expected<Buffer, StarException> read_file_contents(String const& filename) {
  File file(filename);
  auto opened = file.open(IOMode::Read);
  if (!opened)
    return std::unexpected(std::move(opened.error()));

  auto read_all = [&]() -> std::expected<Buffer, StarException> {
    Buffer buffer;
    constexpr std::size_t chunk_size = 4 * 1024;
    constexpr std::size_t maximum_chunk = 1024 * 1024;
    std::size_t amount = chunk_size;
    // Metadata is not EOF, and some readable devices have no usable size.
    // Bound even a device's misleading size before converting or allocating.
    auto hint = file.size();
    if (hint && *hint > 0)
      amount = static_cast<std::size_t>(std::clamp<StreamOffset>(*hint, 1, maximum_chunk));

    auto limit = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    if constexpr (requires {
                    { buffer.max_size() } -> std::same_as<std::size_t>;
                  })
      limit = std::min(limit, buffer.max_size());
    while (true) {
      // Probe EOF before growing the owning buffer, including after the size hint.
      char first;
      auto prefix = file.read(std::span<char>(&first, 1));
      if (!prefix)
        return std::unexpected(std::move(prefix.error()));
      if (*prefix == 0)
        return buffer;
      auto offset = buffer.size();
      // Do not outgrow a precise reservation merely to request a full last
      // chunk. Reaching the hint still requires the next real EOF probe.
      if (hint && *hint > static_cast<StreamOffset>(offset))
        amount = static_cast<std::size_t>(std::min<StreamOffset>(amount, *hint - static_cast<StreamOffset>(offset)));
      if (amount > limit - offset)
        return std::unexpected(IOException::format("File '{}' is too large to read into memory", filename));
      if (offset && hint && *hint > 0 && amount > buffer.capacity() - offset) {
        // Reduce repeated large moves, but speculate only after another real
        // byte arrives and only up to four times the payload already observed.
        // An inaccurate size cannot trigger an unbounded up-front reservation.
        auto observed_bound = offset > limit / 4 ? limit : offset * 4;
        auto reservation = static_cast<std::size_t>(std::min(*hint, static_cast<StreamOffset>(observed_bound)));
        if (reservation > offset + amount)
          buffer.reserve(reservation);
      }
      auto count = read_file_tail(file, buffer, amount, first);
      if (!count)
        return std::unexpected(std::move(count.error()));
      amount = std::clamp(buffer.size(), chunk_size, maximum_chunk);
    }
  };
  auto contents = read_all();
  auto closed = file.close();
  if (!contents) {
    if (!closed)
      std::fprintf(stderr, "Error closing file '%s' after failed read: %s\n", filename.utf8_data(),
                   closed.error().what());
    return std::unexpected(std::move(contents.error()));
  }
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  return contents;
}
}

std::expected<String, StarException> File::current_directory() {
  std::error_code error;
  auto path = std::filesystem::current_path(error);
  if (error)
    return std::unexpected(IOException::format("could not get current directory: {}", error.message()));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  return utf16ToString(path.c_str());
#else
  return String(path.native());
#endif
}

std::expected<void, StarException> File::change_directory(String const& dirName) {
  auto path = filesystem_path(dirName);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  std::filesystem::current_path(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not change directory to '{}': {}", dirName, error.message()));
  return {};
}

std::expected<void, StarException> File::create_directory(String const& dirName) {
  auto path = filesystem_path(dirName);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  if (!std::filesystem::create_directory(*path, error)) {
    // Unlike create_directory, a single mkdir must reject an existing directory.
    if (!error)
      error = std::make_error_code(std::errc::file_exists);
    return std::unexpected(IOException::format("could not create directory '{}': {}", dirName, error.message()));
  }
  return {};
}

std::expected<void, StarException> File::create_directories(String const& fileName) {
  auto path = filesystem_path(fileName);
  if (!path)
    return std::unexpected(std::move(path.error()));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // The native helper rejects bare, drive-relative roots after trimming separators.
  if (path->has_root_directory() && !path->has_root_name() && !path->has_relative_path())
    return std::unexpected(IOException::format("Cannot create directory ancestor '{}'", fileName));
#endif
  std::error_code error;
  std::filesystem::create_directories(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not create directory '{}': {}", fileName, error.message()));
  return {};
}

std::expected<void, StarException> File::remove(String const& fileName) {
  auto path = filesystem_path(fileName);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  if (!std::filesystem::remove(*path, error)) {
    // Missing paths are an error in the File interface, not a successful no-op.
    if (!error)
      error = std::make_error_code(std::errc::no_such_file_or_directory);
    return std::unexpected(IOException::format("remove error on '{}': {}", fileName, error.message()));
  }
  return {};
}

std::expected<void, StarException> File::remove_all(String const& fileName) {
  auto path = filesystem_path(fileName);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  auto status = std::filesystem::symlink_status(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not remove directory '{}': {}", fileName, error.message()));
  // A trailing separator must still require a directory, but must not turn a
  // directory symlink into a traversal of its target.
  if (path->has_relative_path() && path->filename().empty()) {
    while (path->has_relative_path() && path->filename().empty())
      *path = path->parent_path();
    status = std::filesystem::symlink_status(*path, error);
    if (error)
      return std::unexpected(IOException::format("could not remove directory '{}': {}", fileName, error.message()));
  }
  // remove_all also accepts regular files and missing paths; this API does not.
  if (!std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status))
    return std::unexpected(IOException::format("cannot recursively remove non-directory '{}'", fileName));
  if (std::filesystem::remove_all(*path, error) == 0 && !error)
    error = std::make_error_code(std::errc::no_such_file_or_directory);
  if (error)
    return std::unexpected(IOException::format("could not remove directory '{}': {}", fileName, error.message()));
  return {};
}

std::expected<void, StarException> File::copy(String const& source, String const& target) {
  auto sourcePath = filesystem_path(source);
  if (!sourcePath)
    return std::unexpected(std::move(sourcePath.error()));
  auto targetPath = filesystem_path(target);
  if (!targetPath)
    return std::unexpected(std::move(targetPath.error()));
  std::error_code error;
  std::filesystem::copy_file(*sourcePath, *targetPath, std::filesystem::copy_options::overwrite_existing, error);
  if (error)
    return std::unexpected(IOException::format("could not copy '{}' to '{}': {}", source, target, error.message()));
  return {};
}

std::expected<std::shared_ptr<File>, StarException> File::open(String const& filename, IOMode mode) {
  auto file = std::make_shared<File>(filename);
  auto opened = file->open(mode);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  return file;
}

std::expected<ByteArray, StarException> File::read_file(String const& filename) {
  return read_file_contents<ByteArray>(filename);
}

std::expected<String, StarException> File::read_file_string(String const& filename) {
  auto text = read_file_contents<std::string>(filename);
  if (!text)
    return std::unexpected(std::move(text.error()));
  return String(std::move(*text));
}

std::expected<void, StarException> File::write_file(std::span<char const> data, String const& filename) {
  auto file = File::open(filename, IOMode::Write | IOMode::Truncate);
  if (!file)
    return std::unexpected(std::move(file.error()));
  auto written = (*file)->write_full(data);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return (*file)->close();
}

std::expected<void, StarException> File::write_file(ByteArray const& data, String const& filename) {
  return write_file(data.span(), filename);
}

std::expected<void, StarException> File::write_file(String const& data, String const& filename) {
  return write_file(std::span<char const>(data.utf8()), filename);
}

std::expected<void, StarException> File::overwrite_file_with_rename(std::span<char const> data, String const& filename,
                                                                    String const& new_suffix) {
  auto new_file = filename + new_suffix;
  auto acquired = fopen(new_file.utf8_data(), IOMode::Write, true, true);
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  File staging(new_file);
  staging.m_file = *acquired;
  staging.set_mode(IOMode::Write);
  bool owns_stage = true;
  auto cleanup = finally([&] {
    if (!owns_stage)
      return;
    auto closed = staging.close();
    if (!closed)
      std::fprintf(stderr, "Error closing staging file '%s': %s\n", new_file.utf8_data(), closed.error().what());
    auto removed = remove(new_file);
    if (!removed)
      std::fprintf(stderr, "Error removing staging file '%s': %s\n", new_file.utf8_data(), removed.error().what());
  });
  auto written = staging.write_full(data);
  if (!written)
    return std::unexpected(std::move(written.error()));
  auto synced = staging.sync();
  if (!synced)
    return std::unexpected(std::move(synced.error()));
  auto closed = staging.close();
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  auto replaced = rename(new_file, filename);
  if (!replaced)
    return std::unexpected(std::move(replaced.error()));
  owns_stage = false;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // Native rename requests write-through; Windows has no directory-fsync equivalent.
  return {};
#else
  return sync_directory(dir_name(filename));
#endif
}

std::expected<void, StarException> File::overwrite_file_with_rename(ByteArray const& data, String const& filename,
                                                                    String const& newSuffix) {
  return overwrite_file_with_rename(data.span(), filename, newSuffix);
}

std::expected<void, StarException> File::overwrite_file_with_rename(String const& data, String const& filename,
                                                                    String const& newSuffix) {
  return overwrite_file_with_rename(std::span<char const>(data.utf8()), filename, newSuffix);
}

std::expected<void, StarException> File::backup_file_in_sequence(String const& initialFile, String const& targetFile,
                                                                 unsigned maximumBackups,
                                                                 String const& backupExtensionPrefix) {
  for (unsigned i = maximumBackups; i > 0; --i) {
    bool initial = i == 1;
    String const& sourceFile = initial ? initialFile : targetFile;
    String curExtension = initial ? "" : strf("{}{}", backupExtensionPrefix, i - 1);
    String nextExtension = strf("{}{}", backupExtensionPrefix, i);
    auto sourceExists = File::is_regular_file(sourceFile + curExtension);
    if (!sourceExists)
      return std::unexpected(std::move(sourceExists.error()));
    if (*sourceExists) {
      auto copied = File::copy(sourceFile + curExtension, targetFile + nextExtension);
      if (!copied)
        return std::unexpected(std::move(copied.error()));
    }
  }
  return {};
}

std::expected<void, StarException> File::backup_file_in_sequence(String const& targetFile, unsigned maximumBackups,
                                                                 String const& backupExtensionPrefix) {
  return backup_file_in_sequence(targetFile, targetFile, maximumBackups, backupExtensionPrefix);
}

File::File() : IODevice(IOMode::Closed), m_file(nullptr) {}

File::File(String filename)
  : IODevice(IOMode::Closed), m_filename(std::move(filename)), m_file(nullptr) {}

File::~File() noexcept {
  auto result = close();
  if (!result)
    std::fprintf(stderr, "Error closing file '%s' during destruction: %s\n", m_filename.utf8_data(), result.error().what());
}

std::expected<StreamOffset, StarException> File::pos() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("pos called on closed File"));
  return ftell(m_file);
}

std::expected<void, StarException> File::seek(StreamOffset offset, IOSeek seekMode) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("seek called on closed File"));
  return fseek(m_file, offset, seekMode);
}

std::expected<StreamOffset, StarException> File::size() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("size called on closed File"));
  return fsize(m_file);
}

std::expected<bool, StarException> File::at_end() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  auto position = pos();
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto length = size();
  if (!length)
    return std::unexpected(std::move(length.error()));
  return *position >= *length;
}

std::expected<std::size_t, StarException> File::read(std::span<char> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("read called on closed File"));
  if (!is_readable())
    return std::unexpected(IOException("read called on non-readable File"));
  return fread(m_file, data.data(), data.size());
}

std::expected<std::size_t, StarException> File::write(std::span<char const> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("write called on closed File"));
  if (!is_writable())
    return std::unexpected(IOException("write called on non-writable File"));
  return fwrite(m_file, data.data(), data.size());
}

std::expected<std::size_t, StarException> File::read_absolute(StreamOffset position, std::span<char> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file || !is_readable())
    return std::unexpected(IOException("read_absolute called on closed or non-readable File"));
  return pread(m_file, data.data(), data.size(), position);
}

std::expected<std::size_t, StarException> File::write_absolute(StreamOffset position, std::span<char const> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file || !is_writable())
    return std::unexpected(IOException("write_absolute called on closed or non-writable File"));
  return pwrite(m_file, data.data(), data.size(), position);
}

String File::file_name() const { return m_filename; }

std::expected<void, StarException> File::set_filename(String filename) {
  if (is_open())
    return std::unexpected(IOException("Cannot call set_filename while File is open"));
  m_filename = std::move(filename);
  return {};
}

std::expected<void, StarException> File::remove() {
  auto closed = close();
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  if (m_filename.empty())
    return std::unexpected(IOException("Cannot remove file, no filename set"));
  return remove(m_filename);
}

std::expected<void, StarException> File::resize(StreamOffset length) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("resize called on closed File"));
  if (!is_writable())
    return std::unexpected(IOException("resize called on non-writable File"));
  return File::resize(m_file, length);
}

std::expected<void, StarException> File::sync() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("sync called on closed File"));
  return fsync(m_file);
}

std::expected<void, StarException> File::open(IOMode mode) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  auto closed = close();
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  if (m_filename.empty())
    return std::unexpected(IOException("Cannot open file, no filename set"));
  auto opened = fopen(m_filename.utf8_data(), mode);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  m_file = *opened;
  set_mode(mode);
  return {};
}

std::expected<void, StarException> File::close() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  // Consume first: a failed close must never later close a reused descriptor.
  auto file = std::exchange(m_file, nullptr);
  set_mode(IOMode::Closed);
  if (file)
    return fclose(file);
  return {};
}

std::expected<String, StarException> File::device_name() const {
  return m_filename.empty() ? String("<unnamed temp file>") : m_filename;
}

std::expected<std::shared_ptr<IODevice>, StarException> File::clone() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (is_open() && m_filename.empty())
    return std::unexpected(IOException("Cannot clone open File without a filename"));
  auto cloned = std::make_shared<File>(m_filename);
  if (is_open()) {
    auto clone_mode = static_cast<IOMode>(static_cast<std::uint8_t>(mode())
                                          & ~static_cast<std::uint8_t>(IOMode::Truncate | IOMode::Append));
    auto opened = fopen(m_filename.utf8_data(), clone_mode, false);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    cloned->m_file = *opened;
    cloned->set_mode(clone_mode);
    auto position = pos();
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto moved = cloned->seek(*position, IOSeek::Absolute);
    if (!moved)
      return std::unexpected(std::move(moved.error()));
  }
  return std::shared_ptr<IODevice>(std::move(cloned));
}

}
