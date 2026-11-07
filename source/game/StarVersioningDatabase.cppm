module;

import star.json;
import star.data_stream;
import star.thread;
import std;
import star.string;
import star.list;
import star.map;
import star.lua;
import star.format;
import star.version;
import star.lua_root;

namespace Star {
class Root;
class ItemDatabase;


struct VersionedJsonExceptionTag {
  static constexpr std::string_view name() { return "VersionedJsonException"; }
};
using VersionedJsonException = StarError<VersionedJsonExceptionTag, StarException>;
struct VersioningDatabaseExceptionTag {
  static constexpr std::string_view name() { return "VersioningDatabaseException"; }
};
using VersioningDatabaseException = StarError<VersioningDatabaseExceptionTag, StarException>;

struct VersionedJson {
  static std::string_view const Magic;
  static std::size_t const MagicStringSize;
  static VersionNumber const SubVersioning;

  // Writes and reads a binary file containing a versioned json with a magic
  // header marking it as a starbound versioned json file.  Writes using a
  // safe write/flush/swap.
  [[nodiscard]] static std::expected<VersionedJson, StarException> read_file(String const& filename);
  [[nodiscard]] static std::expected<void, StarException> write_file(VersionedJson const& versionedJson,
                                                                     String const& filename);
  [[nodiscard]] static std::expected<void, StarException> writeSubVersioning(DataStream& ds, VersionedJson const& versionedJson);
  [[nodiscard]] static std::expected<void, StarException> readSubVersioning(DataStream& ds, VersionedJson& versionedJson);

  // Writes and reads a json containing a versioned json
  // This allows embedding versioned metadata within a file
  [[nodiscard]] static std::expected<VersionedJson, StarException> fromJson(Json const& source);
  Json toJson() const;

  bool empty() const;

  // Returns a VersionedJsonException if the identifier does not match.
  [[nodiscard]] std::expected<void, StarException> expectIdentifier(String const& expectedIdentifier) const;

  String identifier;
  VersionNumber version;
  Json content;
  StringMap<VersionNumber> subVersions;
};

[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, VersionedJson& versionedJson);
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, VersionedJson const& versionedJson);

class VersioningDatabase {
public:
  [[nodiscard]] static std::expected<std::shared_ptr<VersioningDatabase>, StarException> create(Root& root);

  // Converts the given content Json to a VersionedJson by marking it with the
  // given identifier and the current version configured in the versioning
  // config file.
  [[nodiscard]] std::expected<VersionedJson, StarException> makeCurrentVersionedJson(String const& identifier, Json const& content) const;

  // Returns true if the version in this VersionedJson matches the configured
  // current version and does not need updating.
  [[nodiscard]] std::expected<bool, StarException> versionedJsonCurrent(VersionedJson const& versionedJson) const;

  // Brings versioned JSON up to the configured latest version using scripts.
  // Script and upgrade failures are returned as VersioningDatabaseException.
  [[nodiscard]] std::expected<VersionedJson, StarException> updateVersionedJson(VersionedJson const& versionedJson) const;
  [[nodiscard]] std::expected<VersionedJson, StarException> updateVersionedJson(VersionedJson const& versionedJson, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  // Convenience method, checkts the versionedJson expected identifier and then
  // brings the given versionedJson up to date and returns the content.
  [[nodiscard]] std::expected<Json, StarException> loadVersionedJson(VersionedJson const& versionedJson, String const& expectedIdentifier) const;
  [[nodiscard]] std::expected<Json, StarException> loadVersionedJson(VersionedJson const& versionedJson, String const& expectedIdentifier, std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

private:
  explicit VersioningDatabase(Root& root) : m_root(root) {}
  [[nodiscard]] std::expected<void, StarException> init();
  struct VersionUpdateScript {
    String script;
    VersionNumber fromVersion;
    VersionNumber toVersion;
  };

  [[nodiscard]] std::expected<LuaCallbacks, StarException> makeVersioningCallbacks(std::optional<std::reference_wrapper<ItemDatabase const>> itemCatalog) const;

  Root& m_root;
  mutable std::recursive_mutex m_mutex;
  std::shared_ptr<LuaRoot> m_luaRoot;

  StringMap<VersionNumber> m_currentVersions;
  StringMap<List<VersionUpdateScript>> m_versionUpdateScripts;

  StringMap<StringMap<VersionNumber>> m_currentSubVersions;
  StringMap<HashMap<VersionNumber,StringMap<List<VersionUpdateScript>>>> m_subVersionUpdateScripts;
};

}

export module star.versioning_database;

export namespace Star {
  using ::Star::VersionNumber;
  using ::Star::VersionedJson;
  using ::Star::VersionedJsonExceptionTag;
  using ::Star::VersionedJsonException;
  using ::Star::VersioningDatabaseExceptionTag;
  using ::Star::VersioningDatabaseException;
  using ::Star::VersioningDatabase;
  using ::Star::operator>>;
  using ::Star::operator<<;
}
