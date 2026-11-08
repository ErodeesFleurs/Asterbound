import std;
import star.data_stream;
import star.format;
import star.data_stream_devices;

import star.net_element;
import star.net_element_group;

import star.net_compatibility;
import star.version;
namespace Star {

std::expected<void, StarException> NetElementGroup::addNetElement(NetElement& element, bool propagateInterpolation) {
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((!m_elements.any([&element](auto p) { return &p.first.get() == &element; })));
  }

  element.initNetVersion(m_version);
  if (m_interpolationEnabled && propagateInterpolation) {
    auto result = element.enableNetInterpolation(m_extrapolationHint);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  m_elements.push_back(std::pair<std::reference_wrapper<NetElement>, bool>(std::ref(element), propagateInterpolation));

  for (VersionNumber i = 0; i < (CurrentStreamVersion + 1); i++) {
    if (element.checkWithRules(NetCompatibilityRules(i)))
      m_elementCounts[i]++;
  }
  return {};
}

void NetElementGroup::clearNetElements() {
  m_elementCounts.clear();
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

  auto expectedSize = m_elementCounts.maybe(rules.version()).value_or(m_elements.size());

  if (expectedSize == 0) {
    return false;
  } else if (expectedSize == 1) {
    for (auto& element : m_elements) {
      if (element.first.get().checkWithRules(rules)) {
        return element.first.get().writeNetDelta(ds, fromVersion, rules);
      }
    }
  } else {
    bool deltaWritten = false;
    std::uint64_t i = 0;
    m_buffer.setStreamCompatibilityVersion(rules);
    for (auto& element : m_elements) {
      if (i > expectedSize)
        break;
      if (!element.first.get().checkWithRules(rules))
        continue;
      ++i;

      auto written = element.first.get().writeNetDelta(m_buffer, fromVersion, rules);
      if (!written) {
        m_buffer.clear();
        return std::unexpected(std::move(written.error()));
      }
      if (*written) {
        deltaWritten = true;
        auto indexResult = ds.writeVlqU(i);
        if (!indexResult) { m_buffer.clear(); return std::unexpected(std::move(indexResult.error())); }
        auto bytesResult = ds.write_bytes(m_buffer.data());
        m_buffer.clear();
        if (!bytesResult) return std::unexpected(std::move(bytesResult.error()));
      }
    }
    if (deltaWritten) {
      auto result = ds.writeVlqU(0);
      if (!result) return std::unexpected(std::move(result.error()));
    }
    return deltaWritten;
  }
  return false;
}

std::expected<void, StarException> NetElementGroup::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  auto expectedSize = m_elementCounts.maybe(rules.version()).value_or(m_elements.size());
  if (expectedSize == 0)
    return std::unexpected(IOException("readNetDelta called on empty NetElementGroup"));
  if (expectedSize == 1) {
    for (auto& element : m_elements)
      if (element.first.get().checkWithRules(rules))
        return element.first.get().readNetDelta(ds, interpolationTime, rules);
    return {};
  }
  auto indexResult = ds.readVlqU();
  if (!indexResult) return std::unexpected(std::move(indexResult.error()));
  std::uint64_t readIndex = *indexResult;
  std::uint64_t i = 0;
  for (auto& element : m_elements) {
    if (!element.first.get().checkWithRules(rules)) continue;
    if (readIndex == 0 || readIndex - 1 > i) {
      if (m_interpolationEnabled) {
        auto result = element.first.get().blankNetDelta(interpolationTime);
        if (!result) return std::unexpected(std::move(result.error()));
      }
    } else if (readIndex - 1 == i) {
      auto result = element.first.get().readNetDelta(ds, interpolationTime, rules);
      if (!result) return std::unexpected(std::move(result.error()));
      auto next = ds.readVlqU();
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
