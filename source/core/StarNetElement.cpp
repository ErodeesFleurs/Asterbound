import std;
import star.format;
import star.data_stream;

import star.net_element;


namespace Star {

std::uint64_t NetElementVersion::current() const {
  return m_version;
}

std::uint64_t NetElementVersion::increment() {
  return ++m_version;
}

std::expected<void, StarException> NetElement::enable_net_interpolation(float) { return {}; }

std::expected<void, StarException> NetElement::disable_net_interpolation() { return {}; }

std::expected<void, StarException> NetElement::tick_net_interpolation(float) { return {}; }

std::expected<void, StarException> NetElement::blank_net_delta(float) { return {}; }

}
