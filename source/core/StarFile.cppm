module;
import std;
import star.format;

import star.io_device;
import star.string;
import star.list;

import star.byte_array;
import star.random;
namespace Star {

// A File exclusively owns its native handle; share the File itself when needed.
// Callers must exclude lifecycle changes (open, close, remove, set_filename and
// destruction) from every other operation on the same File. Sequential cursor
// operations share one cursor; a seek followed by a read is not atomic.
// Positioned IO leaves that cursor unchanged and can run concurrently with IO.
// Windows serializes native cursor access; Unix positioned IO uses pread/pwrite
// without a per-File lock. Concurrent writes still require caller coordination
// when their byte ranges overlap.
class File : public IODevice {
public:
  // Converts the passed in path to use the platform specific directory
  // separators only (Windows supports '/' just fine, this is mostly for
  // uniform appearance).  Does *nothing else* (no validity checks, etc).
  static String convert_dir_separators(String const& path);

  // Fallible file operations return their IO diagnostics.
  // get the current working directory
  [[nodiscard]] static std::expected<String, StarException> current_directory();
  // set the current working directory.
  [[nodiscard]] static std::expected<void, StarException> change_directory(String const& dirName);
  [[nodiscard]] static std::expected<void, StarException> create_directory(String const& dirName);
  [[nodiscard]] static std::expected<void, StarException> create_directories(String const& dirName);

  // List all files or directories under given directory.  skipDots skips the
  // special '.' and '..' entries.  Bool value is true for directories.
  [[nodiscard]] static std::expected<List<std::pair<String, bool>>, StarException>
  directory_entries(String const& dirName, bool skipDots = true);

  // Returns the final component of the given path with no directory separators
  static String base_name(String const& fileName);
  // All components of the given path minus the final component, separated by
  // the directory separator
  static String dir_name(String const& fileName);

  // Resolve a path relative to another path.  If the given path is absolute,
  // then the given path is returned unmodified.
  static String relative_to(String const& relativeTo, String const& path);

  // Resolve the given possibly relative path into an absolute path.
  [[nodiscard]] static std::expected<String, StarException> full_path(String const& path);

  [[nodiscard]] static std::expected<String, StarException> temporary_file_name(RandomService& random);

  // Creates and opens a new ReadWrite temporary file with a real path that can
  // be closed and re-opened.  Will not be removed automatically.
  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> temporary_file(RandomService& random);

  // Creates and opens new ReadWrite temporary file and opens it.  This file
  // has no filename and will be removed on close.
  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> ephemeral_file(RandomService& random);

  // Creates a new temporary directory and reutrns the path.  Will not be
  // removed automatically.
  [[nodiscard]] static std::expected<String, StarException> temporary_directory(RandomService& random);

  [[nodiscard]] static std::expected<bool, StarException> exists(String const& path);

  // Does the file exist and is it a regular file (not a directory or special
  // file)?
  [[nodiscard]] static std::expected<bool, StarException> is_regular_file(String const& path);
  // Is the file a directory?
  [[nodiscard]] static std::expected<bool, StarException> is_directory(String const& path);

  // Is the path a symbolic link?  Directory walks must not follow those: a link
  // can point outside of the tree being walked, or at one of its own parents.
  [[nodiscard]] static std::expected<bool, StarException> is_symlink(String const& path);

  [[nodiscard]] static std::expected<void, StarException> remove(String const& filename);
  [[nodiscard]] static std::expected<void, StarException> remove_all(String const& filename);

  // Moves the source file to the target path, overwriting the target path if
  // it already exists.
  [[nodiscard]] static std::expected<void, StarException> rename(String const& source, String const& target);

  // Copies a regular file, overwriting an existing target. Source and target
  // must identify different files; symlinks are followed as by filesystem::copy_file.
  [[nodiscard]] static std::expected<void, StarException> copy(String const& source, String const& target);

  // Reads until a read reports EOF, not merely until an initial size is reached.
  // Size metadata is only a bounded allocation hint: growth before EOF is read,
  // shrinkage is accepted, and changing files do not provide a snapshot.
  [[nodiscard]] static std::expected<ByteArray, StarException> read_file(String const& filename);
  [[nodiscard]] static std::expected<String, StarException> read_file_string(String const& filename);
  [[nodiscard]] static std::expected<StreamOffset, StarException> file_size(String const& path);

  [[nodiscard]] static std::expected<void, StarException> write_file(std::span<char const> data,
                                                                     String const& filename);
  [[nodiscard]] static std::expected<void, StarException> write_file(ByteArray const& data, String const& filename);
  [[nodiscard]] static std::expected<void, StarException> write_file(String const& data, String const& filename);

  // Exclusively create filename + newSuffix in the target directory, write and
  // flush it, then replace the target. An existing staging path is never
  // clobbered. Failures before replacement preserve the old target; there is
  // no fallback that truncates it. On Unix, file sync precedes rename and
  // parent-directory sync follows it: a directory-sync failure is reported
  // after the replacement is already visible. Windows uses a flushed staging
  // file and write-through replacement, not an equivalent parent-directory sync.
  [[nodiscard]] static std::expected<void, StarException>
  overwrite_file_with_rename(std::span<char const> data, String const& filename, String const& newSuffix = ".new");
  [[nodiscard]] static std::expected<void, StarException>
  overwrite_file_with_rename(ByteArray const& data, String const& filename, String const& newSuffix = ".new");
  [[nodiscard]] static std::expected<void, StarException>
  overwrite_file_with_rename(String const& data, String const& filename, String const& newSuffix = ".new");

  [[nodiscard]] static std::expected<void, StarException>
  backup_file_in_sequence(String const& initialFile, String const& targetFile, unsigned maximumBackups,
                          String const& backupExtensionPrefix = ".");
  [[nodiscard]] static std::expected<void, StarException>
  backup_file_in_sequence(String const& targetFile, unsigned maximumBackups, String const& backupExtensionPrefix = ".");

  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> open(String const& filename, IOMode mode);

  File();
  File(String filename);
  ~File() noexcept override;
  File(File const&) = delete;
  File& operator=(File const&) = delete;
  File(File&&) = delete;
  File& operator=(File&&) = delete;

  String file_name() const;
  [[nodiscard]] std::expected<void, StarException> set_filename(String filename);

  // File is closed before removal.
  [[nodiscard]] std::expected<void, StarException> remove();

  [[nodiscard]] std::expected<StreamOffset, StarException> pos() override;
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset pos, IOSeek seek = IOSeek::Absolute) override;
  [[nodiscard]] std::expected<void, StarException> resize(StreamOffset size) override;
  [[nodiscard]] std::expected<StreamOffset, StarException> size() override;
  [[nodiscard]] std::expected<bool, StarException> at_end() override;
  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override;

  // Read / write an absolute location without modifying the shared cursor.
  // Safe alongside other IO, but not lifecycle changes; see the class contract.
  [[nodiscard]] std::expected<std::size_t, StarException> read_absolute(StreamOffset readPosition,
                                                                        std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write_absolute(StreamOffset writePosition,
                                                                         std::span<char const> data) override;

  [[nodiscard]] std::expected<void, StarException> open(IOMode mode) override;
  // Consumes the handle even on error.  Call explicitly after writing so that
  // delayed close errors can be reported to the caller; destruction only logs.
  [[nodiscard]] std::expected<void, StarException> close() override;

  [[nodiscard]] std::expected<void, StarException> sync() override;

  [[nodiscard]] std::expected<String, StarException> device_name() const override;

  // Opens the existing named file with an independent cursor at this position.
  // Acquisition-only flags are not replayed. Unnamed open files cannot clone.
  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override;

private:
  static std::expected<void*, StarException> fopen(char const* filename, IOMode mode, bool create = true,
                                                   bool exclusive = false);
#ifndef STAR_SYSTEM_FAMILY_WINDOWS
  static std::expected<void, StarException> sync_directory(String const& directory);
#endif
  static std::expected<void, StarException> fseek(void* file, StreamOffset offset, IOSeek seek);
  static std::expected<StreamOffset, StarException> ftell(void* file);
  static std::expected<std::size_t, StarException> fread(void* file, char* data, std::size_t len);
  static std::expected<std::size_t, StarException> fwrite(void* file, char const* data, std::size_t len);
  static std::expected<void, StarException> fsync(void* file);
  static std::expected<void, StarException> fclose(void* file);
  static std::expected<StreamOffset, StarException> fsize(void* file);
  static std::expected<std::size_t, StarException> pread(void* file, char* data, std::size_t len, StreamOffset absPosition);
  static std::expected<std::size_t, StarException> pwrite(void* file, char const* data, std::size_t len, StreamOffset absPosition);
  static std::expected<void, StarException> resize(void* file, StreamOffset size);

  String m_filename;
  void* m_file;
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  std::recursive_mutex m_cursor_mutex;
#endif
};

}

export module star.file;

export namespace Star {
  using ::Star::File;
}
