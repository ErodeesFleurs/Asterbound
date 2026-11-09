import std;
import star.memory;
import star.format;
import star.data_stream;

import star.net_element_basic_fields;

import star.net_compatibility;
namespace Star {

std::expected<void, StarException> NetElementSize::readData(DataStream& ds, std::size_t& v) const {
  auto result = ds.readVlqU();
  if (!result) return std::unexpected(std::move(result.error()));
  v = *result == 0 ? NPos : *result - 1;
  return {};
}

std::expected<void, StarException> NetElementSize::writeData(DataStream& ds, std::size_t const& v) const {
  if (v == NPos)
    return ds.vuwrite(std::uint64_t{0});
  else
    return ds.vuwrite(v + 1);
}

std::expected<void, StarException> NetElementBool::readData(DataStream& ds, bool& v) const {
  return ds.read(v);
}

std::expected<void, StarException> NetElementBool::writeData(DataStream& ds, bool const& v) const {
  return ds.write(v);
}

void NetElementEvent::trigger() {
  set(get() + 1);
}

std::uint64_t NetElementEvent::pullOccurrences() {
  std::uint64_t occurrences = get();
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((occurrences >= m_pulledOccurrences));
  }
  std::uint64_t unchecked = occurrences - m_pulledOccurrences;
  m_pulledOccurrences = occurrences;
  return unchecked;
}

bool NetElementEvent::pullOccurred() {
  return pullOccurrences() != 0;
}

void NetElementEvent::ignoreOccurrences() {
  m_pulledOccurrences = get();
}

void NetElementEvent::setIgnoreOccurrencesOnNetLoad(bool ignoreOccurrencesOnNetLoad) {
  m_ignoreOccurrencesOnNetLoad = ignoreOccurrencesOnNetLoad;
}

std::expected<void, StarException> NetElementEvent::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  { auto result = NetElementUInt::netLoad(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
  if (m_ignoreOccurrencesOnNetLoad)
    ignoreOccurrences();
  return {};
}

void NetElementEvent::updated() {
  NetElementBasicField::updated();
  if (m_pulledOccurrences > get())
    m_pulledOccurrences = get();
}

}
