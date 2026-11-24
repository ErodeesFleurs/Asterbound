#include <stdio.h>

import star.file;
import star.io_device;
import star.string;
import star.format;
import std;
import star.byte_array;
import star.algorithm;

namespace Star {

namespace {
std::expected<std::filesystem::path, StarException> filesystem_path(String const& path) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    auto native = path.to_wstring();
    if (!native)
      return std::unexpected(std::move(native.error()));
    return std::filesystem::path(std::move(*native));
#else
    return std::filesystem::path(path.utf8().c_str());
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
  text.resize_and_overwrite(offset + amount, [&](std::string::pointer storage, std::size_t count_available) {
    auto tail = std::span(storage, count_available).subspan(offset, amount);
    tail.front() = first;
    count = file.read(tail.subspan(1));
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
    if (!closed) {
      if (auto written = Star::print(*stderr, "Error closing file '{}' after failed read: {}\n", filename.utf8().c_str(),
                   closed.error().what()); !written)
        return std::unexpected(std::move(contents.error()).with_context(
            Star::format("Could not report close failure ({}): {}", closed.error().what(), written.error().what())));
    }
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
  return String::from_wstring(path.native());
#else
  return String(path.native());
#endif
}

std::expected<void, StarException> File::change_directory(String const& dir_name) {
  auto path = filesystem_path(dir_name);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  std::filesystem::current_path(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not change directory to '{}': {}", dir_name, error.message()));
  return {};
}

std::expected<void, StarException> File::create_directory(String const& dir_name) {
  auto path = filesystem_path(dir_name);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  if (!std::filesystem::create_directory(*path, error)) {
    // Unlike create_directory, a single mkdir must reject an existing directory.
    if (!error)
      error = std::make_error_code(std::errc::file_exists);
    return std::unexpected(IOException::format("could not create directory '{}': {}", dir_name, error.message()));
  }
  return {};
}

std::expected<void, StarException> File::create_directories(String const& dir_name) {
  auto path = filesystem_path(dir_name);
  if (!path)
    return std::unexpected(std::move(path.error()));
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  // The native helper rejects bare, drive-relative roots after trimming separators.
  if (path->has_root_directory() && !path->has_root_name() && !path->has_relative_path())
    return std::unexpected(IOException::format("Cannot create directory ancestor '{}'", dir_name));
#endif
  std::error_code error;
  std::filesystem::create_directories(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not create directory '{}': {}", dir_name, error.message()));
  return {};
}

std::expected<void, StarException> File::remove(String const& filename) {
  auto path = filesystem_path(filename);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  if (!std::filesystem::remove(*path, error)) {
    // Missing paths are an error in the File interface, not a successful no-op.
    if (!error)
      error = std::make_error_code(std::errc::no_such_file_or_directory);
    return std::unexpected(IOException::format("remove error on '{}': {}", filename, error.message()));
  }
  return {};
}

std::expected<void, StarException> File::remove_all(String const& filename) {
  auto path = filesystem_path(filename);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  auto status = std::filesystem::symlink_status(*path, error);
  if (error)
    return std::unexpected(IOException::format("could not remove directory '{}': {}", filename, error.message()));
  // A trailing separator must still require a directory, but must not turn a
  // directory symlink into a traversal of its target.
  if (path->has_relative_path() && path->filename().empty()) {
    while (path->has_relative_path() && path->filename().empty())
      *path = path->parent_path();
    status = std::filesystem::symlink_status(*path, error);
    if (error)
      return std::unexpected(IOException::format("could not remove directory '{}': {}", filename, error.message()));
  }
  // remove_all also accepts regular files and missing paths; this API does not.
  if (!std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status))
    return std::unexpected(IOException::format("cannot recursively remove non-directory '{}'", filename));
  if (std::filesystem::remove_all(*path, error) == 0 && !error)
    error = std::make_error_code(std::errc::no_such_file_or_directory);
  if (error)
    return std::unexpected(IOException::format("could not remove directory '{}': {}", filename, error.message()));
  return {};
}

std::expected<void, StarException> File::copy(String const& source, String const& target) {
  auto source_path = filesystem_path(source);
  if (!source_path)
    return std::unexpected(std::move(source_path.error()));
  auto target_path = filesystem_path(target);
  if (!target_path)
    return std::unexpected(std::move(target_path.error()));
  std::error_code error;
  std::filesystem::copy_file(*source_path, *target_path, std::filesystem::copy_options::overwrite_existing, error);
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

std::expected<std::shared_ptr<File>, StarException> File::create_new_read_write(String filename) {
  auto file = std::make_shared<File>(std::move(filename));
  auto handle = NativeFileHandle::open(file->m_filename, IOMode::ReadWrite, true, true);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  file->m_file.reset(handle->release());
  file->set_mode(IOMode::ReadWrite);
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
  File file(filename);
  auto opened = file.open(IOMode::Write | IOMode::Truncate);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  auto written = file.write_full(data);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return file.close();
}

std::expected<void, StarException> File::write_file(ByteArray const& data, String const& filename) {
  return write_file(data.span(), filename);
}

std::expected<void, StarException> File::write_file(String const& data, String const& filename) {
  return write_file(std::span<char const>(data.utf8()), filename);
}

std::expected<void, StarException> File::overwrite_file_with_rename(std::span<char const> data, String const& filename,
                                                                    String const& new_suffix) {
  auto produce = [data](IODevice& stage) { return stage.write_full(data); };
  return write_file_with_rename(filename, std::cref(produce), new_suffix);
}

std::expected<void, StarException> File::write_file_with_rename(String const& filename,
    std::function<std::expected<void, StarException>(IODevice&)> const& produce, String const& new_suffix) {
  if (!produce)
    return std::unexpected(IOException("Missing staging file writer"));
  File staging(filename + new_suffix);
  auto const& new_file = staging.m_filename;
  auto acquired = NativeFileHandle::open(new_file, IOMode::Write, true, true);
  if (!acquired)
    return std::unexpected(std::move(acquired.error()));
  staging.m_file.reset(acquired->release());
  staging.set_mode(IOMode::Write);
  bool owns_stage = true;
  auto cleanup = make_scope_exit([&] {
    if (!owns_stage)
      return;
    auto closed = staging.close();
    if (!closed) {
      if (auto written = Star::print(*stderr, "Error closing staging file '{}': {}\n", new_file.utf8().c_str(), closed.error().what()); !written)
        std::_Exit(1);
    }
    auto removed = remove(new_file);
    if (!removed) {
      if (auto written = Star::print(*stderr, "Error removing staging file '{}': {}\n", new_file.utf8().c_str(), removed.error().what()); !written)
        std::_Exit(1);
    }
  });
  auto written = produce(staging);
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
                                                                    String const& new_suffix) {
  return overwrite_file_with_rename(data.span(), filename, new_suffix);
}

std::expected<void, StarException> File::overwrite_file_with_rename(String const& data, String const& filename,
                                                                    String const& new_suffix) {
  return overwrite_file_with_rename(std::span<char const>(data.utf8()), filename, new_suffix);
}

std::expected<void, StarException> File::backup_file_in_sequence(String const& initial_file, String const& target_file,
                                                                 unsigned maximum_backups,
                                                                 String const& backup_extension_prefix) {
  auto backup_path = [&](unsigned index) -> String {
    return Star::format("{}{}{}", target_file, backup_extension_prefix, index);
  };
  auto copy_existing = [&](String const& source, unsigned index) -> std::expected<void, StarException> {
    auto exists = is_regular_file(source);
    if (!exists)
      return std::unexpected(std::move(exists.error()));
    if (*exists)
      return copy(source, backup_path(index));
    return {};
  };
  for (unsigned index = maximum_backups; index > 1; --index) {
    auto copied = copy_existing(backup_path(index - 1), index);
    if (!copied)
      return copied;
  }
  if (maximum_backups)
    return copy_existing(initial_file, 1);
  return {};
}

std::expected<void, StarException> File::backup_file_in_sequence(String const& target_file, unsigned maximum_backups,
                                                                 String const& backup_extension_prefix) {
  return backup_file_in_sequence(target_file, target_file, maximum_backups, backup_extension_prefix);
}

File::File() : IODevice(IOMode::Closed) {}

File::File(String filename)
  : IODevice(IOMode::Closed), m_filename(std::move(filename)) {}

File::~File() noexcept {
  if (!m_file)
    return;
  auto result = m_file.close();
  if (!result) {
    if (auto written = Star::print(*stderr, "Error closing file '{}' during destruction: {}\n", m_filename.utf8().c_str(), result.error().what()); !written)
      std::_Exit(1);
  }
}

std::expected<StreamOffset, StarException> File::pos() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("pos called on closed File"));
  return m_file.pos();
}

std::expected<void, StarException> File::seek(StreamOffset offset, IOSeek origin) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("seek called on closed File"));
  return m_file.seek(offset, origin);
}

std::expected<StreamOffset, StarException> File::size() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("size called on closed File"));
  return m_file.size();
}

std::expected<bool, StarException> File::at_end() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
  if (!m_file)
    return std::unexpected(IOException("pos called on closed File"));
  auto position = m_file.pos();
  if (!position)
    return std::unexpected(std::move(position.error()));
  auto length = m_file.size();
  if (!length)
    return std::unexpected(std::move(length.error()));
  return *position >= *length;
#else
  return IODevice::at_end();
#endif
}

std::expected<std::size_t, StarException> File::read(std::span<char> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("read called on closed File"));
  if (!is_readable())
    return std::unexpected(IOException("read called on non-readable File"));
  return m_file.read(data);
}

std::expected<std::size_t, StarException> File::write(std::span<char const> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("write called on closed File"));
  if (!is_writable())
    return std::unexpected(IOException("write called on non-writable File"));
  return m_file.write(data);
}

std::expected<std::size_t, StarException> File::read_absolute(StreamOffset position, std::span<char> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file || !is_readable())
    return std::unexpected(IOException("read_absolute called on closed or non-readable File"));
  return m_file.read_at(position, data);
}

std::expected<std::size_t, StarException> File::write_absolute(StreamOffset position, std::span<char const> data) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file || !is_writable())
    return std::unexpected(IOException("write_absolute called on closed or non-writable File"));
  return m_file.write_at(position, data);
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
  return m_file.resize(length);
}

std::expected<void, StarException> File::sync() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  if (!m_file)
    return std::unexpected(IOException("sync called on closed File"));
  return m_file.sync();
}

std::expected<void, StarException> File::open(IOMode mode) {
  auto closed = close();
  if (!closed)
    return std::unexpected(std::move(closed.error()));
  if (m_filename.empty())
    return std::unexpected(IOException("Cannot open file, no filename set"));
  auto opened = NativeFileHandle::open(m_filename, mode);
  if (!opened)
    return std::unexpected(std::move(opened.error()));
  m_file.reset(opened->release());
  set_mode(mode);
  return {};
}

std::expected<void, StarException> File::close() {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::lock_guard lock(m_cursor_mutex);
#endif
  set_mode(IOMode::Closed);
  if (m_file)
    return m_file.close();
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
    auto opened = NativeFileHandle::open(m_filename, clone_mode, false);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    cloned->m_file.reset(opened->release());
    cloned->set_mode(clone_mode);
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
    if (!m_file)
      return std::unexpected(IOException("pos called on closed File"));
    auto position = m_file.pos();
#else
    auto position = pos();
#endif
    if (!position)
      return std::unexpected(std::move(position.error()));
    auto moved = cloned->m_file.seek(*position, IOSeek::Absolute);
    if (!moved)
      return std::unexpected(std::move(moved.error()));
  }
  return std::shared_ptr<IODevice>(std::move(cloned));
}

}
