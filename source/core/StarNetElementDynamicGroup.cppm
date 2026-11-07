module;
import std;
import star.list;
import star.set;
import star.data_stream;
import star.format;
import star.net_compatibility;
import star.byte_array;

import star.id_map;
import star.strong_typedef;
import star.data_stream_extra;
import star.data_stream_devices;

import star.net_element;

import star.math_common;
import star.algorithm;
namespace Star {

// A dynamic group of NetElements that manages creation and destruction of
// individual elements, that is itself a NetElement.  Element changes are not
// delayed by the interpolation delay, they will always happen immediately, but
// this does not inhibit the Elements themselves from handling their own delta
// update delays normally.
template <typename Element>
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

  using ElementRemovalType = ElementId;
  using ElementAdditionType = std::pair<ElementId, ByteArray>;

  template <typename BaseType>
  struct ElementResetWrapper : BaseType {
    using BaseType::BaseType;

    ElementResetWrapper() : BaseType() {}

    ElementResetWrapper(ElementResetWrapper const& nt) : BaseType(nt) {}

    ElementResetWrapper(ElementResetWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit ElementResetWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit ElementResetWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    ElementResetWrapper& operator=(ElementResetWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    ElementResetWrapper& operator=(ElementResetWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    ElementResetWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                      || std::is_same_v<ElementResetWrapper, std::decay_t<Arg>>,
                    "ElementReset can not implicitly be assigned from std::monostate-derived classes or strong "
                    "std::monostate typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using ElementReset = ElementResetWrapper<std::monostate>;
  using ElementRemoval = StrongTypedefBuiltin<ElementRemovalType, struct ElementRemovalTag>;
  template <typename BaseType>
  struct ElementAdditionWrapper : BaseType {
    using BaseType::BaseType;

    ElementAdditionWrapper() : BaseType() {}

    ElementAdditionWrapper(ElementAdditionWrapper const& nt) : BaseType(nt) {}

    ElementAdditionWrapper(ElementAdditionWrapper&& nt) : BaseType(std::move(nt)) {}

    explicit ElementAdditionWrapper(BaseType const& bt) : BaseType(bt) {}

    explicit ElementAdditionWrapper(BaseType&& bt) : BaseType(std::move(bt)) {}

    ElementAdditionWrapper& operator=(ElementAdditionWrapper const& rhs) {
      BaseType::operator=(rhs);
      return *this;
    }

    ElementAdditionWrapper& operator=(ElementAdditionWrapper&& rhs) {
      BaseType::operator=(std::move(rhs));
      return *this;
    }

    template <class Arg>
    ElementAdditionWrapper& operator=(Arg&& other) {
      static_assert(std::is_base_of_v<BaseType, std::decay_t<Arg>> == false
                      || std::is_same_v<ElementAdditionWrapper, std::decay_t<Arg>>,
                    "ElementAddition can not implicitly be assigned from ElementAdditionType-derived classes or strong "
                    "ElementAdditionType typedefs");

      BaseType::operator=(std::forward<Arg>(other));
      return *this;
    }
  };
  using ElementAddition = ElementAdditionWrapper<ElementAdditionType>;

  using ElementChange = std::variant<ElementReset, ElementRemoval, ElementAddition>;

  using ElementMap = IdMap<ElementId, std::shared_ptr<Element>>;

  void addChangeData(ElementChange change);

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

template <typename Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::addNetElement(std::shared_ptr<Element> element) -> std::expected<ElementId, StarException> {
  { auto result = readyElement(element); if (!result) return std::unexpected(std::move(result.error())); }
  auto id = m_idMap.add(std::move(element));
  if (!id) return std::unexpected(std::move(id.error()));
  addChangeData(ElementAddition(*id, {}));
  return *id;
}

template <typename Element>
void NetElementDynamicGroup<Element>::removeNetElement(ElementId id) {
  m_idMap.remove(id);
  addChangeData(ElementRemoval{id});
}

template <typename Element>
void NetElementDynamicGroup<Element>::clearNetElements() {
  for (auto const& id : netElementIds())
    removeNetElement(id);
}

template <typename Element>
auto NetElementDynamicGroup<Element>::netElementIds() const -> List<ElementId> {
  return m_idMap.keys();
}

template <typename Element>
[[nodiscard]] auto NetElementDynamicGroup<Element>::getNetElement(ElementId id) const -> std::expected<std::shared_ptr<Element>, StarException> {
  auto element = m_idMap.get(id);
  if (!element) return std::unexpected(std::move(element.error()));
  return element->get();
}

template <typename Element>
auto NetElementDynamicGroup<Element>::netElements() const -> List<std::shared_ptr<Element>> {
  return m_idMap.values();
}

template <typename Element>
void NetElementDynamicGroup<Element>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_changeData.clear();
  m_changeDataLastVersion = 0;

  addChangeData(ElementReset());
  for (auto& pair : m_idMap) {
    pair.second->initNetVersion(m_netVersion);
    addChangeData(ElementAddition(pair.first, {})); // we will write the data stream once we know the rules for the one recieving
  }
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::enableNetInterpolation(float extrapolationHint) {
  m_interpolationEnabled = true;
  m_extrapolationHint = extrapolationHint;
  for (auto& p : m_idMap)
    { auto result = p.second->enableNetInterpolation(extrapolationHint); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::disableNetInterpolation() {
  m_interpolationEnabled = false;
  m_extrapolationHint = 0.0f;
  for (auto& p : m_idMap)
    { auto result = p.second->disableNetInterpolation(); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::tickNetInterpolation(float dt) {
  for (auto& p : m_idMap)
    { auto result = p.second->tickNetInterpolation(dt); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  { auto result = ds.writeVlqU(m_idMap.size()); if (!result) return std::unexpected(std::move(result.error())); }
  m_buffer.setStreamCompatibilityVersion(rules);
  for (auto const& pair : m_idMap) {
    { auto result = ds.writeVlqU(pair.first); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = pair.second->netStore(m_buffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.write(m_buffer.data()); if (!result) return std::unexpected(std::move(result.error())); }
    m_buffer.clear();
  }
  return {};
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  m_changeData.clear();
  m_changeDataLastVersion = m_netVersion ? m_netVersion->get().current() : 0;
  m_idMap.clear();
  addChangeData(ElementReset());
  auto count = ds.readVlqU();
  if (!count) return std::unexpected(std::move(count.error()));
  for (std::uint64_t i = 0; i < *count; ++i) {
    auto id = ds.readVlqU();
    if (!id) return std::unexpected(std::move(id.error()));
    if (*id == NullElementId || *id > std::numeric_limits<ElementId>::max())
      return std::unexpected(IOException("Invalid dynamic net element id"));
    auto bytes = ds.read<ByteArray>();
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    DataStreamBuffer storeBuffer(std::move(*bytes));
    if (!m_createElement) return std::unexpected(IOException("Missing dynamic net element factory"));
    auto element = m_createElement();
    if (!element) return std::unexpected(std::move(element.error()));
    if (!*element) return std::unexpected(IOException("Null dynamic net element"));
    { auto result = (*element)->netLoad(storeBuffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = readyElement(*element); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = m_idMap.add(static_cast<ElementId>(*id), std::move(*element)); if (!result) return std::unexpected(std::move(result.error())); }
    addChangeData(ElementAddition(static_cast<ElementId>(*id), storeBuffer.takeData()));
  }
  return {};
}

template <typename Element>
[[nodiscard]] std::expected<bool, StarException> NetElementDynamicGroup<Element>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  struct ClearBuffer { DataStreamBuffer& buffer; ~ClearBuffer() { buffer.clear(); } } clear{m_buffer};
  if (fromVersion < m_changeDataLastVersion) {
    { auto result = ds.write<bool>(true); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = netStore(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    return true;

  } else {
    bool deltaWritten = false;
    auto willWrite = [&]() -> std::expected<void, StarException> {
      if (!deltaWritten) {
        deltaWritten = true;
        { auto result = ds.write<bool>(false); if (!result) return std::unexpected(std::move(result.error())); }
      }
      return {};
    };

    for (auto const& p : m_changeData) {
      if (p.first >= fromVersion) {
        if (std::holds_alternative<ElementAddition>(p.second)) {
          ElementId id = std::get<ElementAddition>(p.second).first;
          if (auto element = m_idMap.find_ref(id)) {
            { auto result = willWrite(); if (!result) return std::unexpected(std::move(result.error())); }
            { auto result = ds.writeVlqU(1); if (!result) return std::unexpected(std::move(result.error())); }
            DataStreamBuffer storeBuffer;
            { auto result = element->get()->netStore(storeBuffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
            { auto result = ds.write((ElementChange)ElementAddition(id, storeBuffer.takeData())); if (!result) return std::unexpected(std::move(result.error())); }
          }
        } else {
          { auto result = willWrite(); if (!result) return std::unexpected(std::move(result.error())); }
          { auto result = ds.writeVlqU(1); if (!result) return std::unexpected(std::move(result.error())); }
          { auto result = ds.write(p.second); if (!result) return std::unexpected(std::move(result.error())); }
        }
      }
    }

    m_buffer.setStreamCompatibilityVersion(rules);
    for (auto& p : m_idMap) {
      auto written = p.second->writeNetDelta(m_buffer, fromVersion, rules);
      if (!written) return std::unexpected(std::move(written.error()));
      if (*written) {
        { auto result = willWrite(); if (!result) return std::unexpected(std::move(result.error())); }
        { auto result = ds.writeVlqU(p.first + 1); if (!result) return std::unexpected(std::move(result.error())); }
        { auto result = ds.writeBytes(m_buffer.data()); if (!result) return std::unexpected(std::move(result.error())); }
        m_buffer.clear();
      }
    }

    if (deltaWritten)
      { auto result = ds.writeVlqU(0); if (!result) return std::unexpected(std::move(result.error())); }

    return deltaWritten;
  }
}

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  m_receivedDeltaIds.clear();
  struct ClearIds { HashSet<ElementId>& ids; ~ClearIds() { ids.clear(); } } clear{m_receivedDeltaIds};
  auto full = ds.read<bool>();
  if (!full) return std::unexpected(std::move(full.error()));
  if (*full) return netLoad(ds, rules);
  while (true) {
    auto code = ds.readVlqU();
    if (!code) return std::unexpected(std::move(code.error()));
    if (*code == 0) break;
    if (*code == 1) {
      auto change = ds.read<ElementChange>();
      if (!change) return std::unexpected(std::move(change.error()));
      if (std::holds_alternative<ElementReset>(*change)) {
        m_idMap.clear();
      } else if (std::holds_alternative<ElementAddition>(*change)) {
        if (std::get<ElementAddition>(*change).first == NullElementId)
          return std::unexpected(IOException("Invalid dynamic net element id"));
        if (!m_createElement) return std::unexpected(IOException("Missing dynamic net element factory"));
        auto element = m_createElement();
        if (!element) return std::unexpected(std::move(element.error()));
        if (!*element) return std::unexpected(IOException("Null dynamic net element"));
        DataStreamBuffer storeBuffer(std::get<ElementAddition>(*change).second);
        { auto result = (*element)->netLoad(storeBuffer, rules); if (!result) return std::unexpected(std::move(result.error())); }
        { auto result = readyElement(*element); if (!result) return std::unexpected(std::move(result.error())); }
        { auto result = m_idMap.add(std::get<ElementAddition>(*change).first, std::move(*element)); if (!result) return std::unexpected(std::move(result.error())); }
      } else if (std::holds_alternative<ElementRemoval>(*change)) {
        m_idMap.remove(std::get<ElementRemoval>(*change));
      }
      addChangeData(std::move(*change));
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

template <typename Element>
[[nodiscard]] std::expected<void, StarException> NetElementDynamicGroup<Element>::blankNetDelta(float interpolationTime) {
  if (m_interpolationEnabled) {
    for (auto& p : m_idMap)
      { auto result = p.second->blankNetDelta(interpolationTime); if (!result) return std::unexpected(std::move(result.error())); }
  }
  return {};
}

template <typename Element>
void NetElementDynamicGroup<Element>::addChangeData(ElementChange change) {
  std::uint64_t currentVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((m_changeData.empty() || m_changeData.back().first <= currentVersion));
  }

  m_changeData.append({currentVersion, std::move(change)});

  m_changeDataLastVersion =
    std::max<std::int64_t>(static_cast<std::int64_t>(currentVersion) - MaxChangeDataVersions, 0);
  while (!m_changeData.empty() && m_changeData.front().first < m_changeDataLastVersion)
    m_changeData.pop_front();
}

template <typename Element>
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
  using ::Star::NetElementDynamicGroup;
}
