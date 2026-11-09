module;
import std;
import star.format;
import star.string;
import star.text_formatter;
import star.hash;

import star.data_stream;
import star.directives;

namespace Star {

// Asset paths are not filesystem paths.  '/' is always the directory separator,
// and it is not possible to escape any asset source directory.  '\' is never a
// valid directory separator.  All asset paths are considered case-insensitive.
//
// In addition to the path portion of the asset path, some asset types may also
// have a sub-path, which is always separated from the path portion of the asset
// by ':'.  There can be at most 1 sub-path component.
//
// Image paths may also have a directives portion of the full asset path, which
// must come after the path and optional sub-path comopnent.  The directives
// portion of the path starts with a '?', and '?' separates each subsquent
// directive.
struct AssetPath {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, AssetPath& path);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, AssetPath const& path);
  friend void write_text(TextBuffer& buffer, AssetPath const& rhs);
  friend struct hash<AssetPath>;
  static AssetPath split(String const& path);
  static String join(AssetPath const& path);

  // Get / modify sub-path directly on a joined path string
  static String setSubPath(String const& joinedPath, String const& subPath);
  static String removeSubPath(String const& joinedPath);

  // Get / modify directives directly on a joined path string
  static String getDirectives(String const& joinedPath);
  static String addDirectives(String const& joinedPath, String const& directives);
  static String removeDirectives(String const& joinedPath);

  // The base directory name for any given path, including the trailing '/'.
  // Ignores sub-path and directives.
  static String directory(String const& path);

  // The file part of any given path, ignoring sub-path and directives.  Path
  // must be a file not a directory.
  static String filename(String const& path);

  // The file extension of a given file path, ignoring directives and
  // sub-paths.
  static String extension(String const& path);

  // Computes an absolute asset path from a relative path relative to another
  // asset.  The sourcePath must be an absolute path (may point to a directory
  // or an asset in a directory, and ignores ':' sub-path or ?  directives),
  // and the givenPath may be either an absolute *or* a relative path.  If it
  // is an absolute path, it is returned unchanged.  If it is a relative path,
  // then it is computed as relative to the directory component of the
  // sourcePath.
  static String relative_to(String const& sourcePath, String const& givenPath);

  AssetPath() = default;
  AssetPath(AssetPath const&) = default;
  AssetPath& operator=(AssetPath const&) = default;
  AssetPath(AssetPath&& path);
  AssetPath& operator=(AssetPath&& path);
  AssetPath(std::string_view path);
  template <std::size_t Size>
  AssetPath(char const (&path)[Size]) : AssetPath(std::string_view(path, Size - 1)) {}
  AssetPath(String const& path);
  AssetPath(String&& basePath, std::optional<String>&& subPath, DirectivesGroup&& directives);
  AssetPath(const String& basePath, const std::optional<String>& subPath, const DirectivesGroup& directives);
  String basePath;
  std::optional<String> subPath;
  DirectivesGroup directives;

  bool operator==(AssetPath const& rhs) const;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, AssetPath& path);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, AssetPath const& path);

void write_text(TextBuffer& buffer, AssetPath const& rhs);

template <>
struct hash<AssetPath> {
  std::size_t operator()(AssetPath const& s) const;
};

}

export module star.asset_path;

export namespace Star {
  template <>
  struct hash<AssetPath>;
}


export template <> struct std::formatter<Star::AssetPath> : Star::TextFormatter {};

export namespace Star {
  using ::Star::AssetPath;
  using ::Star::write_text;
}
