module;
import std;
import star.format;

import star.io_device;
import star.string;
import star.list;

import star.byte_array;
import star.random;
namespace Star {

// All file methods are thread safe.
class File : public IODevice {
public:
  // Converts the passed in path to use the platform specific directory
  // separators only (Windows supports '/' just fine, this is mostly for
  // uniform appearance).  Does *nothing else* (no validity checks, etc).
  static String convertDirSeparators(String const& path);

  // Fallible file operations return their IO diagnostics.
  // get the current working directory
  [[nodiscard]] static std::expected<String, StarException> currentDirectory();
  // set the current working directory.
  [[nodiscard]] static std::expected<void, StarException> changeDirectory(String const& dirName);
  [[nodiscard]] static std::expected<void, StarException> makeDirectory(String const& dirName);
  [[nodiscard]] static std::expected<void, StarException> makeDirectoryRecursive(String const& dirName);

  // List all files or directories under given directory.  skipDots skips the
  // special '.' and '..' entries.  Bool value is true for directories.
  [[nodiscard]] static std::expected<List<std::pair<String, bool>>, StarException> dirList(String const& dirName, bool skipDots = true);

  // Returns the final component of the given path with no directory separators
  static String baseName(String const& fileName);
  // All components of the given path minus the final component, separated by
  // the directory separator
  static String dirName(String const& fileName);

  // Resolve a path relative to another path.  If the given path is absolute,
  // then the given path is returned unmodified.
  static String relativeTo(String const& relativeTo, String const& path);

  // Resolve the given possibly relative path into an absolute path.
  [[nodiscard]] static std::expected<String, StarException> fullPath(String const& path);

  [[nodiscard]] static std::expected<String, StarException> temporaryFileName(RandomService& random);

  // Creates and opens a new ReadWrite temporary file with a real path that can
  // be closed and re-opened.  Will not be removed automatically.
  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> temporaryFile(RandomService& random);

  // Creates and opens new ReadWrite temporary file and opens it.  This file
  // has no filename and will be removed on close.
  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> ephemeralFile(RandomService& random);

  // Creates a new temporary directory and reutrns the path.  Will not be
  // removed automatically.
  [[nodiscard]] static std::expected<String, StarException> temporaryDirectory(RandomService& random);

  [[nodiscard]] static std::expected<bool, StarException> exists(String const& path);

  // Does the file exist and is it a regular file (not a directory or special
  // file)?
  [[nodiscard]] static std::expected<bool, StarException> isFile(String const& path);
  // Is the file a directory?
  [[nodiscard]] static std::expected<bool, StarException> isDirectory(String const& path);

  // Is the path a symbolic link?  Directory walks must not follow those: a link
  // can point outside of the tree being walked, or at one of its own parents.
  [[nodiscard]] static std::expected<bool, StarException> isSymlink(String const& path);

  [[nodiscard]] static std::expected<void, StarException> remove(String const& filename);
  [[nodiscard]] static std::expected<void, StarException> removeDirectoryRecursive(String const& filename);

  // Moves the source file to the target path, overwriting the target path if
  // it already exists.
  [[nodiscard]] static std::expected<void, StarException> rename(String const& source, String const& target);

  // Copies a regular file, overwriting an existing target. Source and target
  // must identify different files; symlinks are followed as by filesystem::copy_file.
  [[nodiscard]] static std::expected<void, StarException> copy(String const& source, String const& target);

  [[nodiscard]] static std::expected<ByteArray, StarException> readFile(String const& filename);
  [[nodiscard]] static std::expected<String, StarException> readFileString(String const& filename);

  [[nodiscard]] static std::expected<void, StarException> writeFile(std::span<char const> data, String const& filename);
  [[nodiscard]] static std::expected<void, StarException> writeFile(ByteArray const& data, String const& filename);
  [[nodiscard]] static std::expected<void, StarException> writeFile(String const& data, String const& filename);

  // Write a new file, potentially overwriting an existing file, in the safest
  // way possible while preserving the old file in the same directory until the
  // operation completes.  Writes to the same path as the existing file to
  // avoid different partition copying.  This may clobber anything in the given
  // path that matches filename + newSuffix.
  [[nodiscard]] static std::expected<void, StarException> overwriteFileWithRename(std::span<char const> data, String const& filename, String const& newSuffix = ".new");
  [[nodiscard]] static std::expected<void, StarException> overwriteFileWithRename(ByteArray const& data, String const& filename, String const& newSuffix = ".new");
  [[nodiscard]] static std::expected<void, StarException> overwriteFileWithRename(String const& data, String const& filename, String const& newSuffix = ".new");

  [[nodiscard]] static std::expected<void, StarException> backupFileInSequence(String const& initialFile, String const& targetFile, unsigned maximumBackups, String const& backupExtensionPrefix = ".");
  [[nodiscard]] static std::expected<void, StarException> backupFileInSequence(String const& targetFile, unsigned maximumBackups, String const& backupExtensionPrefix = ".");

  [[nodiscard]] static std::expected<std::shared_ptr<File>, StarException> open(String const& filename, IOMode mode);

  File();
  File(String filename);
  ~File() noexcept override;

  String fileName() const;
  [[nodiscard]] std::expected<void, StarException> setFilename(String filename);

  // File is closed before removal.
  [[nodiscard]] std::expected<void, StarException> remove();

  [[nodiscard]] std::expected<StreamOffset, StarException> pos() override;
  [[nodiscard]] std::expected<void, StarException> seek(StreamOffset pos, IOSeek seek = IOSeek::Absolute) override;
  [[nodiscard]] std::expected<void, StarException> resize(StreamOffset size) override;
  [[nodiscard]] std::expected<StreamOffset, StarException> size() override;
  [[nodiscard]] std::expected<bool, StarException> atEnd() override;
  [[nodiscard]] std::expected<std::size_t, StarException> read(std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> write(std::span<char const> data) override;

  // Do an immediate read / write of an absolute location in the file, without
  // modifying the current file cursor.  Safe to call in a threaded context
  // with other reads and writes, but not safe vs changing the File state like
  // open and close.
  [[nodiscard]] std::expected<std::size_t, StarException> readAbsolute(StreamOffset readPosition, std::span<char> data) override;
  [[nodiscard]] std::expected<std::size_t, StarException> writeAbsolute(StreamOffset writePosition, std::span<char const> data) override;

  [[nodiscard]] std::expected<void, StarException> open(IOMode mode) override;
  // Consumes the handle even on error.  Call explicitly after writing so that
  // delayed close errors can be reported to the caller; destruction only logs.
  [[nodiscard]] std::expected<void, StarException> close() override;

  [[nodiscard]] std::expected<void, StarException> sync() override;

  [[nodiscard]] std::expected<String, StarException> deviceName() const override;

  [[nodiscard]] std::expected<std::shared_ptr<IODevice>, StarException> clone() override;

private:
  static std::expected<void*, StarException> fopen(char const* filename, IOMode mode);
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
};

}

export module star.file;

export namespace Star {
  using ::Star::File;
}
