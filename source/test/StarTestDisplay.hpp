#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

namespace Star {

// A display number alone can outlive Xvfb and be reassigned to a desktop.
// The launcher removes this private lease when the command finishes.
inline bool matches_test_display(char const* lease) {
  auto display = std::getenv("DISPLAY");
  if (!display || lease[0] != '/')
    return false;
  std::ifstream input(lease);
  std::string owned;
  return std::getline(input, owned) && owned.size() > 1 && owned[0] == ':'
      && owned.find_first_not_of("0123456789", 1) == owned.npos && owned == display;
}

}
