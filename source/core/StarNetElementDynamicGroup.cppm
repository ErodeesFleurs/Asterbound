module;
import std;
import star.list;
import star.set;
import star.data_stream;
import star.format;
import star.net_compatibility;
import star.byte_array;

import star.id_map;
import star.data_stream_extra;
import star.data_stream_devices;

import star.net_element;

import star.math_common;
import star.algorithm;
namespace Star {

template <typename Element>
concept dynamic_net_element = std::derived_from<Element, NetElement>
    && std::same_as<Element, std::remove_cv_t<Element>>;

// A dynamic group of NetElements that manages creation and destruction of
// individual elements, that is itself a NetElement.  Element changes are not
// delayed by the interpolation delay, they will always happen immediately, but
// this does not inhibit the Elements themselves from handling their own delta
// update delays normally.
template <dynamic_net_element Element>
class NetElementDynamicGroup : public NetElement {
public:
  using ElementId = std::uint32_t;
  static ElementId const null_element_id = 0;

  NetElementDynamicGroup() requires std::default_initializable<Element>
    : m_create_element([]() -> std::expected<std::shared_ptr<Element>, StarException> { return std::make_shared<Element>(); }) {}

  explicit NetElementDynamicGroup(std::function<std::expected<std::shared_ptr<Element>, StarException>()> create_element)
    : m_create_element(std::move(create_element)) {}

  NetElementDynamicGroup(NetElementDynamicGroup const&) = delete;
  NetElementDynamicGroup& operator=(NetElementDynamicGroup const&) = delete;

  // Must not call add_net_element / remove_net_element when being used as a slave,
  // id errors will result.
  [[nodiscard]] std::expected<ElementId, StarException> add_net_element(std::shared_ptr<Element> element);
  // Prepare must not edit this group. A failed preparation preserves its elements and wire history.
  template <std::invocable Prepare>
    requires std::same_as<std::invoke_result_t<Prepare>, std::expected<void, StarException>>
  [[nodiscard]] std::expected<void, StarException> replace_net_elements(std::shared_ptr<Element> element, Prepare&& prepare);
  void remove_net_element(ElementId id);

  // Remove all elements
  void clear_net_elements();

  List<ElementId> net_element_ids() const;
  [[nodiscard]] std::expected<std::shared_ptr<Element>, StarException> get_net_element(ElementId id) const;
  // Borrows the element with the largest ID, matching net_elements().last().
  [[nodiscard]] std::expected<std::reference_wrapper<Element>, StarException> last_net_element() const&;
  std::expected<std::reference_wrapper<Element>, StarException> last_net_element() const&& = delete;

  List<std::shared_ptr<Element>> net_elements() const;

  void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  // Values are never interpolated, but they will be delayed for the given
  // interpolation_time.
  [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
  [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blank_net_delta(float interpolation_time = 0.0f) override;

private:
  // If a delta is written from further back than this many versions, the delta
  // will fall back to a full serialization of the entire state.
  static std::int64_t const max_change_data_versions = 100;

  using Reset = std::monostate;
  struct Remove {
    ElementId id;
    friend auto operator<<(DataStream& ds, Remove const& change) { return ds << change.id; }
    friend auto operator>>(DataStream& ds, Remove& change) { return ds >> change.id; }
  };
  struct Add {
    ElementId id;
    friend auto operator<<(DataStream& ds, Add const& change) { return ds << change.id; }
    friend auto operator>>(DataStream& ds, Add& change) { return ds >> change.id; }
  };
  // These slots are the wire tags. An Add header is followed by a length-prefixed
  // child record on the wire, never in the history: each peer gets the live state.
  using ElementChange = std::variant<Reset, Remove, Add>;

  using ElementMap = IdMap<ElementId, std::shared_ptr<Element>>;

  void add_change_data(ElementChange change);
  [[nodiscard]] std::expected<void, StarException> load_element(ElementId id, ByteArray&& bytes, NetCompatibilityRules rules);

  [[nodiscard]] std::expected<void, StarException> ready_element(std::shared_ptr<Element> const& element);

  std::function<std::expected<std::shared_ptr<Element>, StarException>()> m_create_element;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_net_version = std::nullopt;
  bool m_interpolation_enabled = false;
  float m_extrapolation_hint = 0.0f;

  ElementMap m_id_map = ElementMap::template with_range<1, std::numeric_limits<ElementId>::max()>();

  Deque<std::pair<std::uint64_t, ElementChange>> m_change_data;
  std::uint64_t m_change_data_last_version = 0;

  mutable DataStreamBuffer m_buffer;
  mutable HashSet<ElementId> m_received_delta_ids;
};

template <dynamic_net_element Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::add_net_element(std::shared_ptr<Element> element) -> std::expected<ElementId, StarException> {
  { auto result = ready_element(element); if (!result) return std::unexpected(std::move(result.error())); }
  auto id = m_id_map.emplace(std::move(element));
  if (!id) return std::unexpected(std::move(id.error()));
  add_change_data(Add{*id});
  return *id;
}

template <dynamic_net_element Element>
template <std::invocable Prepare>
  requires std::same_as<std::invoke_result_t<Prepare>, std::expected<void, StarException>>
std::expected<void, StarException> NetElementDynamicGroup<Element>::replace_net_elements(
    std::shared_ptr<Element> element, Prepare&& prepare) {
  if (auto result = ready_element(element); !result)
    return std::unexpected(std::move(result.error()));

  auto size = m_id_map.size();
  if (size == std::numeric_limits<ElementId>::max())
    return std::unexpected(IdMapException("No id space left in dynamic net element group"));

  std::shared_ptr<Element> previous;
  List<std::shared_ptr<Element>> remaining;
  if (size > 1)
    remaining.reserve(size - 1);
  if (auto result = std::invoke(std::forward<Prepare>(prepare)); !result)
    return std::unexpected(std::move(result.error()));

  // There is room for the candidate without removing or reusing any existing ID.
  auto id = m_id_map.emplace(std::move(element));
  if (!id)
    return std::unexpected(std::move(id.error()));
  // Displaced elements may reenter the group when released. Finish all
  // structural changes and record Remove(s) before Add before releasing them.
  for (auto it = m_id_map.begin(); it != m_id_map.end();) {
    if (auto old_id = it->first; old_id != *id) {
      if (previous)
        remaining.push_back(std::move(it->second));
      else
        previous = std::move(it->second);
      it = m_id_map.erase(it);
      add_change_data(Remove{old_id});
    } else {
      ++it;
    }
  }
  add_change_data(Add{*id});
  return {};
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::remove_net_element(ElementId id) {
  // Finish erasing the node before releasing the element: its destructor can reenter the group.
  m_id_map.try_extract_value(id);
  add_change_data(Remove{id});
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::clear_net_elements() {
  // Snapshot the original ids in removal order. Destructors may add or remove elements.
  for (auto const& id : net_element_ids())
    remove_net_element(id);
}

template <dynamic_net_element Element>
auto NetElementDynamicGroup<Element>::net_element_ids() const -> List<ElementId> {
  return m_id_map.keys();
}

template <dynamic_net_element Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::get_net_element(ElementId id) const -> std::expected<std::shared_ptr<Element>, StarException> {
  auto element = m_id_map.get(id);
  if (!element) return std::unexpected(std::move(element.error()));
  return element->get();
}

template <dynamic_net_element Element>
auto NetElementDynamicGroup<Element>::last_net_element() const& -> std::expected<std::reference_wrapper<Element>, StarException> {
  if (m_id_map.empty())
    return std::unexpected(IOException("Dynamic net element group is empty"));
  return std::ref(*std::prev(m_id_map.end())->second);
}

template <dynamic_net_element Element>
auto NetElementDynamicGroup<Element>::net_elements() const -> List<std::shared_ptr<Element>> {
  return m_id_map.values();
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_net_version = version;
  m_change_data.clear();
  m_change_data_last_version = 0;

  add_change_data(Reset{});
  for (auto& pair : m_id_map) {
    pair.second->init_net_version(m_net_version);
    add_change_data(Add{pair.first});
  }
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::enable_net_interpolation(float extrapolation_hint) {
  m_interpolation_enabled = true;
  m_extrapolation_hint = extrapolation_hint;
  for (auto& p : m_id_map)
    { auto result = p.second->enable_net_interpolation(extrapolation_hint); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::disable_net_interpolation() {
  m_interpolation_enabled = false;
  m_extrapolation_hint = 0.0f;
  for (auto& p : m_id_map)
    { auto result = p.second->disable_net_interpolation(); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::tick_net_interpolation(float dt) {
  for (auto& p : m_id_map)
    { auto result = p.second->tick_net_interpolation(dt); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  {
    auto result = ds.write_vlq_u(m_id_map.size());
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  m_buffer.set_stream_compatibility_version(rules);
  for (auto const& pair : m_id_map) {
    {
      auto result = ds.write_vlq_u(pair.first);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    { auto result = pair.second->net_store(m_buffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.write(m_buffer.data()); if (!result) return std::unexpected(std::move(result.error())); }
    m_buffer.clear();
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  m_change_data.clear();
  m_change_data_last_version = m_net_version ? m_net_version->get().current() : 0;
  m_id_map.clear();
  add_change_data(Reset{});
  auto count = ds.read_vlq_u();
  if (!count) return std::unexpected(std::move(count.error()));
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto id = ds.read_vlq_u();
    if (!id) return std::unexpected(std::move(id.error()));
    if (*id == null_element_id || *id > std::numeric_limits<ElementId>::max())
      return std::unexpected(IOException("Invalid dynamic net element id"));
    auto bytes = ds.read<ByteArray>();
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    if (auto result = load_element(static_cast<ElementId>(*id), std::move(*bytes), rules); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<bool, StarException> NetElementDynamicGroup<Element>::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  if (from_version < m_change_data_last_version) {
    { auto result = ds.write<bool>(true); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = net_store(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    return true;

  } else {
    bool delta_written = false;
    auto write_code = [&](std::uint64_t code) -> std::expected<void, StarException> {
      if (!delta_written) {
        delta_written = true;
        { auto result = ds.write<bool>(false); if (!result) return std::unexpected(std::move(result.error())); }
      }
      if (auto result = ds.write_vlq_u(code); !result)
        return std::unexpected(std::move(result.error()));
      return {};
    };

    for (auto const& [version, change] : m_change_data) {
      if (version < from_version)
        continue;
      if (std::holds_alternative<Add>(change)) {
        auto element = m_id_map.find_ref(std::get<Add>(change).id);
        if (!element)
          continue;
        if (auto result = write_code(1); !result)
          return std::unexpected(std::move(result.error()));
        m_buffer.set_stream_compatibility_version(rules);
        if (auto result = element->get()->net_store(m_buffer, rules); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = ds.write(change); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = ds.write(m_buffer.data()); !result)
          return std::unexpected(std::move(result.error()));
        m_buffer.clear();
      } else {
        if (auto result = write_code(1); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = ds.write(change); !result)
          return std::unexpected(std::move(result.error()));
      }
    }

    m_buffer.set_stream_compatibility_version(rules);
    for (auto& p : m_id_map) {
      auto written = p.second->write_net_delta(m_buffer, from_version, rules);
      if (!written) return std::unexpected(std::move(written.error()));
      if (*written) {
        if (auto result = write_code(std::uint64_t{p.first} + 1); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = ds.write_bytes(m_buffer.data()); !result)
          return std::unexpected(std::move(result.error()));
        m_buffer.clear();
      }
    }

    if (delta_written) {
      auto result = ds.write_vlq_u(0);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }

    return delta_written;
  }
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  m_received_delta_ids.clear();
  struct ClearIds { HashSet<ElementId>& ids; ~ClearIds() { ids.clear(); } } clear{m_received_delta_ids};
  auto full = ds.read<bool>();
  if (!full) return std::unexpected(std::move(full.error()));
  if (*full) return net_load(ds, rules);
  while (true) {
    auto code = ds.read_vlq_u();
    if (!code) return std::unexpected(std::move(code.error()));
    if (*code == 0) break;
    if (*code == 1) {
      auto change = ds.read<ElementChange>();
      if (!change) return std::unexpected(std::move(change.error()));
      if (std::holds_alternative<Add>(*change)) {
        auto bytes = ds.read<ByteArray>();
        if (!bytes) return std::unexpected(std::move(bytes.error()));
        auto id = std::get<Add>(*change).id;
        if (id == null_element_id)
          return std::unexpected(IOException("Invalid dynamic net element id"));
        if (auto result = load_element(id, std::move(*bytes), rules); !result)
          return std::unexpected(std::move(result.error()));
      } else if (std::holds_alternative<Remove>(*change)) {
        remove_net_element(std::get<Remove>(*change).id);
      } else {
        m_id_map.clear();
        add_change_data(Reset{});
      }
    } else {
      if (*code - 1 > std::numeric_limits<ElementId>::max())
        return std::unexpected(IOException("Dynamic net element id out of range"));
      ElementId id = static_cast<ElementId>(*code - 1);
      auto element = m_id_map.get(id);
      if (!element) return std::unexpected(std::move(element.error()));
      { auto result = element->get()->read_net_delta(ds, interpolation_time, rules); if (!result) return std::unexpected(std::move(result.error())); }
      if (m_interpolation_enabled) m_received_delta_ids.add(id);
    }
  }
  if (m_interpolation_enabled) {
    for (auto& p : m_id_map) {
      if (!m_received_delta_ids.contains(p.first))
        { auto result = p.second->blank_net_delta(interpolation_time); if (!result) return std::unexpected(std::move(result.error())); }
    }
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::blank_net_delta(float interpolation_time) {
  if (m_interpolation_enabled) {
    for (auto& p : m_id_map)
      { auto result = p.second->blank_net_delta(interpolation_time); if (!result) return std::unexpected(std::move(result.error())); }
  }
  return {};
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::add_change_data(ElementChange change) {
  std::uint64_t current_version = m_net_version ? m_net_version->get().current() : 0;
  if constexpr (Star::debug_enabled) {
    Star::star_assert_failed((m_change_data.empty() || m_change_data.back().first <= current_version));
  }

  m_change_data.push_back({current_version, std::move(change)});

  m_change_data_last_version =
    std::max<std::int64_t>(static_cast<std::int64_t>(current_version) - max_change_data_versions, 0);
  while (!m_change_data.empty() && m_change_data.front().first < m_change_data_last_version)
    m_change_data.pop_front();
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::load_element(
    ElementId id, ByteArray&& bytes, NetCompatibilityRules rules) {
  if (!m_create_element) return std::unexpected(IOException("Missing dynamic net element factory"));
  auto element = m_create_element();
  if (!element) return std::unexpected(std::move(element.error()));
  if (!*element) return std::unexpected(IOException("Null dynamic net element"));
  DataStreamBuffer buffer(std::move(bytes));
  buffer.set_stream_compatibility_version(rules);
  if (auto result = (*element)->net_load(buffer, rules); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = ready_element(*element); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_id_map.try_emplace(id, std::move(*element)); !result)
    return std::unexpected(std::move(result.error()));
  add_change_data(Add{id});
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::ready_element(std::shared_ptr<Element> const& element) {
  if (!element) return std::unexpected(IOException("Null dynamic net element"));
  element->init_net_version(m_net_version);
  if (m_interpolation_enabled)
    return element->enable_net_interpolation(m_extrapolation_hint);
  return element->disable_net_interpolation();
}

}

export module star.net_element_dynamic_group;

export namespace Star {
  using ::Star::dynamic_net_element;
  using ::Star::NetElementDynamicGroup;
}
