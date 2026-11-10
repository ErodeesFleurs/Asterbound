module;
import std;

namespace Star {

extern std::string_view const ProductVersionString;
extern std::string_view const BundledAssetsVersionString;
extern std::string_view const StarVersionString;
extern std::string_view const StarSourceIdentifierString;
extern std::string_view const StarArchitectureString;

using VersionNumber = std::uint32_t;
}

export module star.version;

export namespace Star {
  using ::Star::ProductVersionString;
  using ::Star::BundledAssetsVersionString;
  using ::Star::StarVersionString;
  using ::Star::StarSourceIdentifierString;
  using ::Star::StarArchitectureString;
  using ::Star::VersionNumber;
}
