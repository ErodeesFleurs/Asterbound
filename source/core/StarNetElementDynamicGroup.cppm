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
  static ElementId const NullElementId = 0;

  NetElementDynamicGroup() requires std::default_initializable<Element>
    : m_createElement([]() -> std::expected<std::shared_ptr<Element>, StarException> { return std::make_shared<Element>(); }) {}

  explicit NetElementDynamicGroup(std::function<std::expected<std::shared_ptr<Element>, StarException>()> createElement)
    : m_createElement(std::move(createElement)) {}

  NetElementDynamicGroup(NetElementDynamicGroup const&) = delete;
  NetElementDynamicGroup& operator=(NetElementDynamicGroup const&) = delete;

  // Must not call addNetElement / removeNetElement when being used as a slave,
  // id errors will result.
  [[nodiscard]] std::expected<ElementId, StarException> addNetElement(std::shared_ptr<Element> element);
  void removeNetElement(ElementId id);

  // Remove all elements
  void clearNetElements();

  List<ElementId> netElementIds() const;
  [[nodiscard]] std::expected<std::shared_ptr<Element>, StarException> getNetElement(ElementId id) const;

  List<std::shared_ptr<Element>> netElements() const;

  void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  // Values are never interpolated, but they will be delayed for the given
  // interpolationTime.
  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;
  [[nodiscard]] std::expected<void, StarException> blankNetDelta(float interpolationTime = 0.0f) override;

private:
  // If a delta is written from further back than this many versions, the delta
  // will fall back to a full serialization of the entire state.
  static std::int64_t const MaxChangeDataVersions = 100;

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

  void addChangeData(ElementChange change);
  [[nodiscard]] std::expected<void, StarException> load_element(ElementId id, ByteArray&& bytes, NetCompatibilityRules rules);

  [[nodiscard]] std::expected<void, StarException> readyElement(std::shared_ptr<Element> const& element);

  std::function<std::expected<std::shared_ptr<Element>, StarException>()> m_createElement;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion = std::nullopt;
  bool m_interpolationEnabled = false;
  float m_extrapolationHint = 0.0f;

  ElementMap m_idMap = ElementMap::template with_range<1, std::numeric_limits<ElementId>::max()>();

  Deque<std::pair<std::uint64_t, ElementChange>> m_changeData;
  std::uint64_t m_changeDataLastVersion = 0;

  mutable DataStreamBuffer m_buffer;
  mutable HashSet<ElementId> m_receivedDeltaIds;
};

template <dynamic_net_element Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::addNetElement(std::shared_ptr<Element> element) -> std::expected<ElementId, StarException> {
  { auto result = readyElement(element); if (!result) return std::unexpected(std::move(result.error())); }
  auto id = m_idMap.emplace(std::move(element));
  if (!id) return std::unexpected(std::move(id.error()));
  addChangeData(Add{*id});
  return *id;
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::removeNetElement(ElementId id) {
  // Finish erasing the node before releasing the element: its destructor can reenter the group.
  m_idMap.try_extract_value(id);
  addChangeData(Remove{id});
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::clearNetElements() {
  // Snapshot the original ids in removal order. Destructors may add or remove elements.
  for (auto const& id : netElementIds())
    removeNetElement(id);
}

template <dynamic_net_element Element>
auto NetElementDynamicGroup<Element>::netElementIds() const -> List<ElementId> {
  return m_idMap.keys();
}

template <dynamic_net_element Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::getNetElement(ElementId id) const -> std::expected<std::shared_ptr<Element>, StarException> {
  auto element = m_idMap.get(id);
  if (!element) return std::unexpected(std::move(element.error()));
  return element->get();
}

template <dynamic_net_element Element>
auto NetElementDynamicGroup<Element>::netElements() const -> List<std::shared_ptr<Element>> {
  return m_idMap.values();
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_changeData.clear();
  m_changeDataLastVersion = 0;

  addChangeData(Reset{});
  for (auto& pair : m_idMap) {
    pair.second->initNetVersion(m_netVersion);
    addChangeData(Add{pair.first});
  }
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::enableNetInterpolation(float extrapolationHint) {
  m_interpolationEnabled = true;
  m_extrapolationHint = extrapolationHint;
  for (auto& p : m_idMap)
    { auto result = p.second->enableNetInterpolation(extrapolationHint); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::disableNetInterpolation() {
  m_interpolationEnabled = false;
  m_extrapolationHint = 0.0f;
  for (auto& p : m_idMap)
    { auto result = p.second->disableNetInterpolation(); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::tickNetInterpolation(float dt) {
  for (auto& p : m_idMap)
    { auto result = p.second->tickNetInterpolation(dt); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  {
    auto result = ds.write_vlq_u(m_idMap.size());
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  m_buffer.set_stream_compatibility_version(rules);
  for (auto const& pair : m_idMap) {
    {
      auto result = ds.write_vlq_u(pair.first);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    { auto result = pair.second->netStore(m_buffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.write(m_buffer.data()); if (!result) return std::unexpected(std::move(result.error())); }
    m_buffer.clear();
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  m_changeData.clear();
  m_changeDataLastVersion = m_netVersion ? m_netVersion->get().current() : 0;
  m_idMap.clear();
  addChangeData(Reset{});
  auto count = ds.read_vlq_u();
  if (!count) return std::unexpected(std::move(count.error()));
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto id = ds.read_vlq_u();
    if (!id) return std::unexpected(std::move(id.error()));
    if (*id == NullElementId || *id > std::numeric_limits<ElementId>::max())
      return std::unexpected(IOException("Invalid dynamic net element id"));
    auto bytes = ds.read<ByteArray>();
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    if (auto result = load_element(static_cast<ElementId>(*id), std::move(*bytes), rules); !result)
      return std::unexpected(std::move(result.error()));
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<bool, StarException> NetElementDynamicGroup<Element>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  if (fromVersion < m_changeDataLastVersion) {
    { auto result = ds.write<bool>(true); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = netStore(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    return true;

  } else {
    bool deltaWritten = false;
    auto write_code = [&](std::uint64_t code) -> std::expected<void, StarException> {
      if (!deltaWritten) {
        deltaWritten = true;
        { auto result = ds.write<bool>(false); if (!result) return std::unexpected(std::move(result.error())); }
      }
      if (auto result = ds.write_vlq_u(code); !result)
        return std::unexpected(std::move(result.error()));
      return {};
    };

    for (auto const& [version, change] : m_changeData) {
      if (version < fromVersion)
        continue;
      if (std::holds_alternative<Add>(change)) {
        auto element = m_idMap.find_ref(std::get<Add>(change).id);
        if (!element)
          continue;
        if (auto result = write_code(1); !result)
          return std::unexpected(std::move(result.error()));
        m_buffer.set_stream_compatibility_version(rules);
        if (auto result = element->get()->netStore(m_buffer, rules); !result)
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
    for (auto& p : m_idMap) {
      auto written = p.second->writeNetDelta(m_buffer, fromVersion, rules);
      if (!written) return std::unexpected(std::move(written.error()));
      if (*written) {
        if (auto result = write_code(std::uint64_t{p.first} + 1); !result)
          return std::unexpected(std::move(result.error()));
        if (auto result = ds.write_bytes(m_buffer.data()); !result)
          return std::unexpected(std::move(result.error()));
        m_buffer.clear();
      }
    }

    if (deltaWritten) {
      auto result = ds.write_vlq_u(0);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }

    return deltaWritten;
  }
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  m_receivedDeltaIds.clear();
  struct ClearIds { HashSet<ElementId>& ids; ~ClearIds() { ids.clear(); } } clear{m_receivedDeltaIds};
  auto full = ds.read<bool>();
  if (!full) return std::unexpected(std::move(full.error()));
  if (*full) return netLoad(ds, rules);
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
        if (id == NullElementId)
          return std::unexpected(IOException("Invalid dynamic net element id"));
        if (auto result = load_element(id, std::move(*bytes), rules); !result)
          return std::unexpected(std::move(result.error()));
      } else if (std::holds_alternative<Remove>(*change)) {
        removeNetElement(std::get<Remove>(*change).id);
      } else {
        m_idMap.clear();
        addChangeData(Reset{});
      }
    } else {
      if (*code - 1 > std::numeric_limits<ElementId>::max())
        return std::unexpected(IOException("Dynamic net element id out of range"));
      ElementId id = static_cast<ElementId>(*code - 1);
      auto element = m_idMap.get(id);
      if (!element) return std::unexpected(std::move(element.error()));
      { auto result = element->get()->readNetDelta(ds, interpolationTime, rules); if (!result) return std::unexpected(std::move(result.error())); }
      if (m_interpolationEnabled) m_receivedDeltaIds.add(id);
    }
  }
  if (m_interpolationEnabled) {
    for (auto& p : m_idMap) {
      if (!m_receivedDeltaIds.contains(p.first))
        { auto result = p.second->blankNetDelta(interpolationTime); if (!result) return std::unexpected(std::move(result.error())); }
    }
  }
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::blankNetDelta(float interpolationTime) {
  if (m_interpolationEnabled) {
    for (auto& p : m_idMap)
      { auto result = p.second->blankNetDelta(interpolationTime); if (!result) return std::unexpected(std::move(result.error())); }
  }
  return {};
}

template <dynamic_net_element Element>
void NetElementDynamicGroup<Element>::addChangeData(ElementChange change) {
  std::uint64_t currentVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((m_changeData.empty() || m_changeData.back().first <= currentVersion));
  }

  m_changeData.push_back({currentVersion, std::move(change)});

  m_changeDataLastVersion =
    std::max<std::int64_t>(static_cast<std::int64_t>(currentVersion) - MaxChangeDataVersions, 0);
  while (!m_changeData.empty() && m_changeData.front().first < m_changeDataLastVersion)
    m_changeData.pop_front();
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::load_element(
    ElementId id, ByteArray&& bytes, NetCompatibilityRules rules) {
  if (!m_createElement) return std::unexpected(IOException("Missing dynamic net element factory"));
  auto element = m_createElement();
  if (!element) return std::unexpected(std::move(element.error()));
  if (!*element) return std::unexpected(IOException("Null dynamic net element"));
  DataStreamBuffer buffer(std::move(bytes));
  buffer.set_stream_compatibility_version(rules);
  if (auto result = (*element)->netLoad(buffer, rules); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = readyElement(*element); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = m_idMap.try_emplace(id, std::move(*element)); !result)
    return std::unexpected(std::move(result.error()));
  addChangeData(Add{id});
  return {};
}

template <dynamic_net_element Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::readyElement(std::shared_ptr<Element> const& element) {
  if (!element) return std::unexpected(IOException("Null dynamic net element"));
  element->initNetVersion(m_netVersion);
  if (m_interpolationEnabled)
    return element->enableNetInterpolation(m_extrapolationHint);
  return element->disableNetInterpolation();
}

}

export module star.net_element_dynamic_group;

export namespace Star {
  using ::Star::dynamic_net_element;
  using ::Star::NetElementDynamicGroup;
}
