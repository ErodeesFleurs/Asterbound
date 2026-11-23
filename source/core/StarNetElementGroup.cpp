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
  while (!entries.empty() && !entries.front().first.get().check_with_rules(rules))
    entries = entries.subspan(1);
  while (entries.size() > 1 && !entries.back().first.get().check_with_rules(rules))
    entries = entries.first(entries.size() - 1);
  return entries;
}

std::expected<void, StarException> NetElementGroup::add_net_element(NetElement& element, bool propagate_interpolation) {
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((!m_elements.any([&element](GroupEntry const& p) { return &p.first.get() == &element; })));
  }

  element.init_net_version(m_version);
  if (m_interpolation_enabled && propagate_interpolation) {
    auto result = element.enable_net_interpolation(m_extrapolation_hint);
    if (!result) return std::unexpected(std::move(result.error()));
  }
  m_elements.emplace_back(std::ref(element), propagate_interpolation);
  return {};
}

void NetElementGroup::clear_net_elements() {
  m_elements.clear();
}

void NetElementGroup::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_version = version;
  for (auto& p : m_elements)
    p.first.get().init_net_version(m_version);
}

std::expected<void, StarException> NetElementGroup::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  for (auto& p : m_elements) {
    if (p.first.get().check_with_rules(rules)) {
      auto result = p.first.get().net_store(ds, rules);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  for (auto& p : m_elements) {
    if (p.first.get().check_with_rules(rules)) {
      auto result = p.first.get().net_load(ds, rules);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::enable_net_interpolation(float extrapolation_hint) {
  m_interpolation_enabled = true;
  m_extrapolation_hint = extrapolation_hint;
  for (auto& p : m_elements) {
    if (p.second) {
      auto result = p.first.get().enable_net_interpolation(extrapolation_hint);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::disable_net_interpolation() {
  m_interpolation_enabled = false;
  m_extrapolation_hint = 0;
  for (auto& p : m_elements) {
    if (p.second) {
      auto result = p.first.get().disable_net_interpolation();
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<void, StarException> NetElementGroup::tick_net_interpolation(float dt) {
  if (m_interpolation_enabled) {
    for (auto& p : m_elements) {
      auto result = p.first.get().tick_net_interpolation(dt);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

std::expected<bool, StarException> NetElementGroup::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;

  auto entries = visible_bounds(rules);
  if (entries.empty())
    return false;
  if (entries.size() == 1)
    return entries.front().first.get().write_net_delta(ds, from_version, rules);

  bool delta_written = false;
  std::uint64_t index = 0;
  m_buffer.set_stream_compatibility_version(rules);
  for (auto const& entry : entries) {
    auto& element = entry.first.get();
    if (!element.check_with_rules(rules))
      continue;
    ++index;

    auto written = element.write_net_delta(m_buffer, from_version, rules);
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

std::expected<void, StarException> NetElementGroup::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  auto entries = visible_bounds(rules);
  if (entries.empty())
    return std::unexpected(IOException("read_net_delta called on empty NetElementGroup"));
  if (entries.size() == 1)
    return entries.front().first.get().read_net_delta(ds, interpolation_time, rules);
  auto index_result = ds.read_vlq_u();
  if (!index_result) return std::unexpected(std::move(index_result.error()));
  std::uint64_t read_index = *index_result;
  std::uint64_t i = 0;
  for (auto const& element : entries) {
    if (!element.first.get().check_with_rules(rules)) continue;
    if (read_index == 0 || read_index - 1 > i) {
      if (m_interpolation_enabled) {
        auto result = element.first.get().blank_net_delta(interpolation_time);
        if (!result) return std::unexpected(std::move(result.error()));
      }
    } else if (read_index - 1 == i) {
      auto result = element.first.get().read_net_delta(ds, interpolation_time, rules);
      if (!result) return std::unexpected(std::move(result.error()));
      auto next = ds.read_vlq_u();
      if (!next) return std::unexpected(std::move(next.error()));
      read_index = *next;
    } else {
      return std::unexpected(IOException("group indexes out of order in NetElementGroup::read_net_delta"));
    }
    ++i;
  }
  if (read_index != 0)
    return std::unexpected(IOException("group index out of range in NetElementGroup::read_net_delta"));
  return {};
}

std::expected<void, StarException> NetElementGroup::blank_net_delta(float interpolation_time) {
  if (m_interpolation_enabled) {
    for (auto& p : m_elements) {
      auto result = p.first.get().blank_net_delta(interpolation_time);
      if (!result) return std::unexpected(std::move(result.error()));
    }
  }
  return {};
}

}
