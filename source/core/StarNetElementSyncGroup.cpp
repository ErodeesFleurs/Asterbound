import std;
import star.format;
import star.data_stream;

import star.net_element_group;
import star.net_element_sync_group;

import star.net_compatibility;
namespace Star {

std::expected<void, StarException> NetElementSyncGroup::enable_net_interpolation(float extrapolation_hint) {
  { auto result = NetElementGroup::enable_net_interpolation(extrapolation_hint); if (!result) return std::unexpected(std::move(result.error())); }
  if (m_has_recent_changes)
    { auto result = net_elements_need_load(false); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

std::expected<void, StarException> NetElementSyncGroup::disable_net_interpolation() {
  { auto result = NetElementGroup::disable_net_interpolation(); if (!result) return std::unexpected(std::move(result.error())); }
  if (m_has_recent_changes)
    { auto result = net_elements_need_load(false); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

std::expected<void, StarException> NetElementSyncGroup::tick_net_interpolation(float dt) {
  { auto result = NetElementGroup::tick_net_interpolation(dt); if (!result) return std::unexpected(std::move(result.error())); }
  if (m_has_recent_changes) {
    m_recent_delta_time -= dt;
    if (net_interpolation_enabled())
      { auto result = net_elements_need_load(false); if (!result) return std::unexpected(std::move(result.error())); }

    if (m_recent_delta_time < 0.0f && m_recent_delta_was_blank)
      m_has_recent_changes = false;
  }
  return {};
}

std::expected<void, StarException> NetElementSyncGroup::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  { auto result = const_cast<NetElementSyncGroup&>(*this).net_elements_need_store(); if (!result) return std::unexpected(std::move(result.error())); }
  return NetElementGroup::net_store(ds, rules);
}

std::expected<void, StarException> NetElementSyncGroup::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  { auto result = NetElementGroup::net_load(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
  { auto result = net_elements_need_load(true); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

std::expected<bool, StarException> NetElementSyncGroup::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;
  { auto result = const_cast<NetElementSyncGroup&>(*this).net_elements_need_store(); if (!result) return std::unexpected(std::move(result.error())); }
  return NetElementGroup::write_net_delta(ds, from_version, rules);
}

std::expected<void, StarException> NetElementSyncGroup::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  { auto result = NetElementGroup::read_net_delta(ds, interpolation_time, rules); if (!result) return std::unexpected(std::move(result.error())); }

  m_has_recent_changes = true;
  m_recent_delta_time = interpolation_time;
  m_recent_delta_was_blank = false;

  { auto result = net_elements_need_load(false); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

std::expected<void, StarException> NetElementSyncGroup::blank_net_delta(float interpolation_time) {
  { auto result = NetElementGroup::blank_net_delta(interpolation_time); if (!result) return std::unexpected(std::move(result.error())); }

  if (!m_recent_delta_was_blank) {
    m_recent_delta_time = interpolation_time;
    m_recent_delta_was_blank = true;
  }

  if (m_has_recent_changes && net_interpolation_enabled())
    { auto result = net_elements_need_load(false); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

std::expected<void, StarException> NetElementSyncGroup::net_elements_need_load(bool) { return {}; }

std::expected<void, StarException> NetElementSyncGroup::net_elements_need_store() { return {}; }

void NetElementCallbackGroup::set_needs_load_callback(std::function<std::expected<void, StarException>(bool)> needs_load_callback) {
  m_net_elements_need_load = std::move(needs_load_callback);
}

void NetElementCallbackGroup::set_needs_store_callback(std::function<std::expected<void, StarException>()> needs_store_callback) {
  m_net_elements_need_store = std::move(needs_store_callback);
}

std::expected<void, StarException> NetElementCallbackGroup::net_elements_need_load(bool load) {
  if (m_net_elements_need_load)
    return m_net_elements_need_load(load);
  return {};
}

std::expected<void, StarException> NetElementCallbackGroup::net_elements_need_store() {
  if (m_net_elements_need_store)
    return m_net_elements_need_store();
  return {};
}

}
