module;
import std;
import star.format;
import star.string;
import star.text_formatter;
import star.hash;

import star.data_stream;
import star.directives;

namespace Star {

// Asset paths use byte delimiters, not platform filesystem separators.
// Only '/' separates directories; Assets validates source containment.
// Source indexing may ignore case; locator equality/hash preserve exact bytes.
//
// In addition to the path portion of the asset path, some asset types may also
// have a sub-path, which is always separated from the path portion of the asset
// by ':'.  There can be at most 1 sub-path component.
//
// Image paths may also have a directives portion of the full asset path, which
// must come after the path and optional sub-path component. The directives
// portion starts with '?'; empty directive entries are omitted when joined.
struct AssetPath {
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, AssetPath& path);
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, AssetPath const& path);
  friend void write_text(TextBuffer& buffer, AssetPath const& rhs);
  friend struct hash<AssetPath>;
  static AssetPath split(String const& path);
  static String join(AssetPath const& path);
  // Embedded payloads are byte-sensitive, not filesystem assets.
  static bool is_embedded(String const& path);
  static bool is_remote(String const& path);

  // Modify sub-paths while preserving normalized directive entries.
  static String set_sub_path(String const& joined_path, String const& sub_path);
  static String remove_sub_path(String const& joined_path);

  // Get or modify directives directly on a joined path string.
  static String get_directives(String const& joined_path);
  static String add_directives(String const& joined_path, String const& directives);
  static String remove_directives(String const& joined_path);

  // The base directory name for any given path, including the trailing '/'.
  // Ignores sub-path and directives.
  static String directory(String const& path);

  // The file part of any given path, ignoring sub-path and directives.  Path
  // must be a file not a directory.
  static String filename(String const& path);

  // The file extension of a given file path, ignoring directives and
  // sub-paths.
  static String extension(String const& path);

  // Resolves given_path relative to the directory of source_path.
  // source_path is an absolute file or directory; its sub-path and
  // directives are ignored. Absolute given_path values are returned unchanged.
  // Relative values are appended to the source directory without filesystem
  // normalization.
  static String relative_to(String const& source_path, String const& given_path);

  AssetPath() = default;
  AssetPath(AssetPath const&) = default;
  AssetPath& operator=(AssetPath const&) = default;
  AssetPath(AssetPath&& path) noexcept;
  AssetPath& operator=(AssetPath&& path) noexcept;
  AssetPath(std::string_view path);
  template <std::size_t size>
    requires(size > 0)
  AssetPath(char const (&path)[size]) : AssetPath(std::string_view(path, size - 1)) {}
  AssetPath(String const& path);
  AssetPath(String&& base_path, std::optional<String>&& sub_path, DirectivesGroup&& directives) noexcept;
  AssetPath(String const& base_path, std::optional<String> const& sub_path, DirectivesGroup const& directives);
  String m_base_path;
  std::optional<String> m_sub_path;
  DirectivesGroup m_directives;

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
