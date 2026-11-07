module;
import std;

namespace Star {

extern std::string_view const OpenStarVersionString;
extern std::string_view const StarVersionString;
extern std::string_view const StarSourceIdentifierString;
extern std::string_view const StarArchitectureString;

using VersionNumber = std::uint32_t;
}

export module star.version;

export namespace Star {
  using ::Star::OpenStarVersionString;
  using ::Star::StarVersionString;
  using ::Star::StarSourceIdentifierString;
  using ::Star::StarArchitectureString;
  using ::Star::VersionNumber;
}
