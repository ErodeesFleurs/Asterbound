#include <time.h>
import std;
import star.format;

namespace Star {
namespace Time {

std::expected<std::tm, StarException> localCalendarTime(std::time_t time) {
  // The Windows CRT keeps localtime's buffer per thread; copy it immediately.
  auto calendar = std::localtime(&time);
  if (!calendar)
    return std::unexpected(StarException("Could not convert system time to local calendar time"));
  return *calendar;
}


}


}
