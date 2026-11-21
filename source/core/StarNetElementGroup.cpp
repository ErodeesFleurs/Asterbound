import std;
import star.data_stream;
import star.format;
import star.data_stream_devices;

import star.net_element;
import star.net_element_group;

import star.net_compatibility;
namespace Star {

std::span<NetElementGroup::GroupEntry const> NetElementGroup::visible_bounds(NetCompatibilityRules const& rules) const {
  // Both endpoints are visible; interior entries still need filtering. The
  // resulting span distinguishes zero, one and multiple without an exact count.
  std::span<GroupEntry const> entries = m_elements;
  while (!entries.empty() && !entries.front().first.get().checkWithRules(rules))
    entries = entries.subspan(1);
  while (entries.size() > 1 && !entries.back().first.get().checkWithRules(rules))
    entries = entries.first(entries.size() - 1);
  return entries;
}

std::expected<void, StarException> NetElementGroup::addNetElement(NetElement& element, bool propagateInterpolation) {
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((!m_elements.any([&element](GroupEntry const& p) { return &p.first.get() == &element; })));
  }

  element.initNetVersion(m_version);
  if (m_interpolationEnabled && propagateInterpolation) {
    auto result = element.enableNetInterpolation(m_extrapolationHint);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  m_elements.emplace_back(std::ref(element), propagateInterpolation);
  return {};
}

void NetElementGroup::clearNetElements() {
  m_elements.clear();
}

void NetElementGroup::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_version = version;
  for (auto& p : m_elements)
    p.first.get().initNetVersion(m_version);
}

std::expected<void, StarException> NetElementGroup::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  for (auto& p : m_elements) {
    if (p.first.get().checkWithRules(rules)) {
      auto result = p.first.get().netStore(ds, rules);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  for (auto& p : m_elements) {
    if (p.first.get().checkWithRules(rules)) {
      auto result = p.first.get().netLoad(ds, rules);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::enableNetInterpolation(float extrapolationHint) {
  m_interpolationEnabled = true;
  m_extrapolationHint = extrapolationHint;
  for (auto& p : m_elements) {
    if (p.second) {
      auto result = p.first.get().enableNetInterpolation(extrapolationHint);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::disableNetInterpolation() {
  m_interpolationEnabled = false;
  m_extrapolationHint = 0;
  for (auto& p : m_elements) {
    if (p.second) {
      auto result = p.first.get().disableNetInterpolation();
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::tickNetInterpolation(float dt) {
  if (m_interpolationEnabled) {
    for (auto& p : m_elements) {
      auto result = p.first.get().tickNetInterpolation(dt);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> NetElementGroup::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;

  auto entries = visible_bounds(rules);
  if (entries.empty())
    return false;
  if (entries.size() == 1)
    return entries.front().first.get().writeNetDelta(ds, fromVersion, rules);

  bool delta_written = false;
  std::uint64_t index = 0;
  m_buffer.set_stream_compatibility_version(rules);
  for (auto const& entry : entries) {
    auto& element = entry.first.get();
    if (!element.checkWithRules(rules))
      continue;
    ++index;

    auto written = element.writeNetDelta(m_buffer, fromVersion, rules);
    if (!written) {
      m_buffer.clear();
      return std::unexpected(std::move(written.error()));
    }
    if (*written) {
      delta_written = true;
      auto index_result = ds.write_vlq_u(index);
      if (!index_result) { m_buffer.clear(); return std::unexpected(std::move(index_result.error())); }
      auto bytes_result = ds.write_bytes(m_buffer.data());
      m_buffer.clear();
      if (!bytes_result) return std::unexpected(std::move(bytes_result.error()));
    }
  }
  if (delta_written) {
    auto result = ds.write_vlq_u(0);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  return delta_written;
}

std::expected<void, StarException> NetElementGroup::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  auto entries = visible_bounds(rules);
  if (entries.empty())
    return std::unexpected(IOException("readNetDelta called on empty NetElementGroup"));
  if (entries.size() == 1)
    return entries.front().first.get().readNetDelta(ds, interpolationTime, rules);
  auto indexResult = ds.read_vlq_u();
  if (!indexResult) return std::unexpected(std::move(indexResult.error()));
  std::uint64_t readIndex = *indexResult;
  std::uint64_t i = 0;
  for (auto const& element : entries) {
    if (!element.first.get().checkWithRules(rules)) continue;
    if (readIndex == 0 || readIndex - 1 > i) {
      if (m_interpolationEnabled) {
        auto result = element.first.get().blankNetDelta(interpolationTime);
        if (!result) return std::unexpected(std::move(result.error()));
      }
    } else if (readIndex - 1 == i) {
      auto result = element.first.get().readNetDelta(ds, interpolationTime, rules);
      if (!result) return std::unexpected(std::move(result.error()));
      auto next = ds.read_vlq_u();
      if (!next) return std::unexpected(std::move(next.error()));
      readIndex = *next;
    } else {
      return std::unexpected(IOException("group indexes out of order in NetElementGroup::readNetDelta"));
    }
    ++i;
  }
  if (readIndex != 0)
    return std::unexpected(IOException("group index out of range in NetElementGroup::readNetDelta"));
  return {};
}

std::expected<void, StarException> NetElementGroup::blankNetDelta(float interpolationTime) {
  if (m_interpolationEnabled) {
    for (auto& p : m_elements) {
      auto result = p.first.get().blankNetDelta(interpolationTime);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

}
