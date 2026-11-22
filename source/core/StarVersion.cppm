module;
import std;

namespace Star {

extern std::string_view const product_version;
extern std::string_view const bundled_assets_version;
extern std::string_view const base_game_version;
extern std::string_view const source_identifier;
extern std::string_view const target_architecture;

using VersionNumber = std::uint32_t;
}

export module star.version;

export namespace Star {
  using ::Star::product_version;
  using ::Star::bundled_assets_version;
  using ::Star::base_game_version;
  using ::Star::source_identifier;
  using ::Star::target_architecture;
  using ::Star::VersionNumber;
}
