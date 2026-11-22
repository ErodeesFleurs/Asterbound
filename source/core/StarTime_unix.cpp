#include <time.h>
import std;
import star.format;

namespace Star {
namespace Time {

std::expected<std::tm, StarException> local_calendar_time(std::time_t time) {
  std::tm calendar;
  if (!localtime_r(&time, &calendar))
    return std::unexpected(StarException("Could not convert system time to local calendar time"));
  return calendar;
}


}

}
