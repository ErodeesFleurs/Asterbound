module;
#include <QString>

import std;
import star.string;

namespace Star {

inline String toSString(QString const& str) {
  auto utf8 = str.toUtf8();
  return String(std::string_view(utf8.constData()));
}

inline QString toQString(String const& str) {
  return QString::fromUtf8(str.utf8_data(), str.utf8_size());
}

}


export module star.string_conversion;

export namespace Star {
  using ::Star::toSString;
  using ::Star::toQString;
}
