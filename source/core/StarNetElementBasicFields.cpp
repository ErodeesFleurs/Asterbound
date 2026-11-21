import std;
import star.memory;
import star.format;
import star.data_stream;

import star.net_element_basic_fields;

import star.net_compatibility;
namespace Star {

std::expected<void, StarException> NetElementSize::read_data(DataStream& ds, std::size_t& v) const {
  auto result = ds.read_vlq_u();
  if (!result) return std::unexpected(std::move(result.error()));
  v = *result == 0 ? NPos : *result - 1;
  return {};
}

std::expected<void, StarException> NetElementSize::write_data(DataStream& ds, std::size_t const& v) const {
  if (v == NPos)
    return ds.vuwrite(std::uint64_t{0});
  else
    return ds.vuwrite(v + 1);
}

std::expected<void, StarException> NetElementBool::read_data(DataStream& ds, bool& v) const {
  return ds.read(v);
}

std::expected<void, StarException> NetElementBool::write_data(DataStream& ds, bool const& v) const {
  return ds.write(v);
}

void NetElementEvent::trigger() {
  set(get() + 1);
}

std::uint64_t NetElementEvent::pull_occurrences() {
  std::uint64_t occurrences = get();
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((occurrences >= m_pulled_occurrences));
  }
  std::uint64_t unchecked = occurrences - m_pulled_occurrences;
  m_pulled_occurrences = occurrences;
  return unchecked;
}

bool NetElementEvent::pull_occurred() {
  return pull_occurrences() != 0;
}

void NetElementEvent::ignore_occurrences() {
  m_pulled_occurrences = get();
}

void NetElementEvent::set_ignore_occurrences_on_net_load(bool ignore_occurrences_on_net_load) {
  m_ignore_occurrences_on_net_load = ignore_occurrences_on_net_load;
}

std::expected<void, StarException> NetElementEvent::net_load(DataStream& ds, NetCompatibilityRules rules) {
  { auto result = NetElementUInt::net_load(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
  if (m_ignore_occurrences_on_net_load)
    ignore_occurrences();
  return {};
}

void NetElementEvent::updated() {
  NetElementBasicField::updated();
  if (m_pulled_occurrences > get())
    m_pulled_occurrences = get();
}

}
