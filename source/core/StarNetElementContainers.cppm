module;
import std;
import star.list;
import star.data_stream;
import star.format;
import star.map;
import star.data_stream_extra;
import star.strong_typedef;

import star.net_element;

import star.output_proxy;
import star.net_compatibility;
import star.algorithm;
namespace Star {

// NetElement map container that is more efficient than the naive serialization
// of an entire Map, because it delta encodes changes to save networking
// traffic.
template <typename BaseMap>
class NetElementMapWrapper : public NetElement, private BaseMap {
public:
  using iterator = typename BaseMap::iterator;
  using const_iterator = typename BaseMap::const_iterator;

  using key_type = typename BaseMap::key_type;
  using mapped_type = typename BaseMap::mapped_type;
  using value_type = typename BaseMap::value_type;

  void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  bool shouldWriteNetDelta(std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  [[nodiscard]] std::expected<std::reference_wrapper<mapped_type const>, StarException> get(key_type const& key) const;
  std::optional<std::reference_wrapper<mapped_type const>> find_ref(key_type const& key) const;

  const_iterator begin() const;
  const_iterator end() const;

  using BaseMap::keys;
  using BaseMap::values;
  using BaseMap::pairs;
  using BaseMap::contains;
  using BaseMap::size;
  using BaseMap::empty;
  using BaseMap::maybe;
  using BaseMap::value;

  std::pair<const_iterator, bool> insert(value_type v);
  std::pair<const_iterator, bool> insert(key_type k, mapped_type v);

  [[nodiscard]] std::expected<void, StarException> add(key_type k, mapped_type v);
  // Calling set with a matching key and value does not cause a delta to be
  // produced
  void set(key_type k, mapped_type v);
  // set requires that mapped_type implement operator==, push always generates
  // a delta and does not require mapped_type operator==
  void push(key_type k, mapped_type v);

  bool remove(key_type const& k);

  const_iterator erase(const_iterator i);

  [[nodiscard]] std::expected<mapped_type, StarException> extract_value(key_type const& k);
  std::optional<mapped_type> try_extract_value(key_type const& k);

  void clear();

  BaseMap const& baseMap() const;
  void reset(BaseMap values);
  bool pullUpdated();

  // Sets this map to contain the same keys / values as the given map.  All
  // values in this map not found in the given map are removed.  (Same as
  // reset, but with arbitrary map type).
  template <typename MapType>
  void setContents(MapType const& values);

  std::uint64_t changeDataLastVersion() const;

private:
  // If a delta is written from further back than this many steps, the delta
  // will fall back to a full serialization of the entire state.
  static std::int64_t const MaxChangeDataVersions = 100;

  struct SetChange {
    key_type key;
    mapped_type value;
  };
  struct RemoveChange {
    key_type key;
  };
  struct ClearChange {};

  using ElementChange = std::variant<SetChange, RemoveChange, ClearChange>;

  [[nodiscard]] static std::expected<void, StarException> writeSetChange(DataStream& ds, key_type const& key,
                                                                         mapped_type const& value);
  [[nodiscard]] static std::expected<void, StarException> writeChange(DataStream& ds, ElementChange const& change);
  [[nodiscard]] static std::expected<ElementChange, StarException> readChange(DataStream& ds);

  void addChangeData(ElementChange change);

  void addPendingChangeData(ElementChange change, float interpolationTime);
  void applyChange(ElementChange change);

  Deque<std::pair<std::uint64_t, ElementChange>> m_changeData;
  Deque<std::pair<float, ElementChange>> m_pendingChangeData;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion = std::nullopt;
  std::uint64_t m_changeDataLastVersion = 0;
  bool m_updated = false;
  bool m_interpolationEnabled = false;
};

template <typename Key, typename Value>
using NetElementMap = NetElementMapWrapper<Map<Key, Value>>;

template <typename Key, typename Value>
using NetElementHashMap = NetElementMapWrapper<HashMap<Key, Value>>;

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;

  m_changeData.clear();
  m_changeDataLastVersion = 0;

  for (auto& change : std::exchange(m_pendingChangeData, {}))
    applyChange(std::move(change.second));

  addChangeData(ClearChange());
  for (auto const& p : *this)
    addChangeData(SetChange{p.first, p.second});
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::enableNetInterpolation(float) {
  m_interpolationEnabled = true;
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::disableNetInterpolation() {
  m_interpolationEnabled = false;
  for (auto& change : std::exchange(m_pendingChangeData, {}))
    applyChange(std::move(change.second));
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::tickNetInterpolation(float dt) {
  for (auto& p : m_pendingChangeData)
    p.first -= dt;

  while (!m_pendingChangeData.empty() && m_pendingChangeData.front().first <= 0.0f) {
    applyChange(std::move(m_pendingChangeData.front().second));
    m_pendingChangeData.pop_front();
  }
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  { auto result = ds.writeVlqU(BaseMap::size() + m_pendingChangeData.size()); if (!result) return std::unexpected(std::move(result.error())); }
  for (auto const& pair : *this) {
    auto result = writeSetChange(ds, pair.first, pair.second);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  for (auto const& p : m_pendingChangeData)
    { auto result = writeChange(ds, p.second); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  m_changeData.clear();
  m_changeDataLastVersion = m_netVersion ? m_netVersion->get().current() : 0;
  m_pendingChangeData.clear();
  BaseMap::clear();

  addChangeData(ClearChange());

  auto countResult = ds.readVlqU();
  if (!countResult) return std::unexpected(std::move(countResult.error()));
  std::uint64_t count = *countResult;
  for (std::uint64_t i = 0; i < count; ++i) {
    auto change = readChange(ds);
    if (!change) return std::unexpected(std::move(change.error()));
    addChangeData(*change);
    applyChange(std::move(*change));
  }

  m_updated = true;
  return {};
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::shouldWriteNetDelta(std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  if (fromVersion < m_changeDataLastVersion)
    return true;

  for (auto const& p : m_changeData)
    if (p.first >= fromVersion)
      return true;

  return false;
}

template <typename BaseMap>
[[nodiscard]] std::expected<bool, StarException> NetElementMapWrapper<BaseMap>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  bool deltaWritten = false;

  if (fromVersion < m_changeDataLastVersion) {
    deltaWritten = true;
    { auto result = ds.writeVlqU(1); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = netStore(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }

  } else {
    for (auto const& p : m_changeData) {
      if (p.first >= fromVersion) {
        deltaWritten = true;
        { auto result = ds.writeVlqU(2); if (!result) return std::unexpected(std::move(result.error())); }
        { auto result = writeChange(ds, p.second); if (!result) return std::unexpected(std::move(result.error())); }
      }
    }
  }

  if (deltaWritten)
    { auto result = ds.writeVlqU(0); if (!result) return std::unexpected(std::move(result.error())); }

  return deltaWritten;
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  while (true) {
    auto codeResult = ds.readVlqU();
    if (!codeResult) return std::unexpected(std::move(codeResult.error()));
    std::uint64_t code = *codeResult;
    if (code == 0) {
      break;
    } else if (code == 1) {
      { auto result = netLoad(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    } else if (code == 2) {
      auto change = readChange(ds);
      if (!change) return std::unexpected(std::move(change.error()));
      addChangeData(*change);

      if (m_interpolationEnabled && interpolationTime > 0.0f)
        addPendingChangeData(std::move(*change), interpolationTime);
      else
        applyChange(std::move(*change));
    } else {
      return std::unexpected(IOException("Improper delta code received in NetElementMapWrapper::readNetDelta"));
    }
  }
  return {};
}

template <typename BaseMap>
[[nodiscard]] auto NetElementMapWrapper<BaseMap>::get(key_type const& key) const -> std::expected<std::reference_wrapper<mapped_type const>, StarException> {
  return BaseMap::get(key);
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::find_ref(key_type const& key) const
  -> std::optional<std::reference_wrapper<mapped_type const>> {
  return BaseMap::find_ref(key);
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::begin() const -> const_iterator {
  return BaseMap::begin();
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::end() const -> const_iterator {
  return BaseMap::end();
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::insert(value_type v) -> std::pair<const_iterator, bool> {
  auto res = BaseMap::insert(v);
  if (res.second) {
    addChangeData(SetChange{std::move(v.first), std::move(v.second)});
    m_updated = true;
  }
  return res;
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::insert(key_type k, mapped_type v) -> std::pair<const_iterator, bool> {
  return insert(value_type(std::move(k), std::move(v)));
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::add(key_type k, mapped_type v) {
  if (BaseMap::contains(k))
    return std::unexpected(MapException::format("Entry with key '{}' already present.", outputAny(k)));
  insert(value_type(std::move(k), std::move(v)));
  return {};
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::set(key_type k, mapped_type v) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    if (!(i->second == v)) {
      addChangeData(SetChange{std::move(k), v});
      i->second = std::move(v);
      m_updated = true;
    }
  } else {
    addChangeData(SetChange{k, v});
    BaseMap::insert(value_type(std::move(k), std::move(v)));
    m_updated = true;
  }
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::push(key_type k, mapped_type v) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    addChangeData(SetChange(std::move(k), v));
    i->second = std::move(v);
  } else {
    addChangeData(SetChange(k, v));
    BaseMap::insert(value_type(std::move(k), std::move(v)));
  }
  m_updated = true;
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::remove(key_type const& k) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    BaseMap::erase(i);
    addChangeData(RemoveChange{k});
    m_updated = true;
    return true;
  }
  return false;
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::erase(const_iterator i) -> const_iterator {
  addChangeData(RemoveChange(i->first));
  m_updated = true;
  return BaseMap::erase(i);
}

template <typename BaseMap>
[[nodiscard]] auto NetElementMapWrapper<BaseMap>::extract_value(key_type const& k)
  -> std::expected<mapped_type, StarException> {
  auto i = BaseMap::find(k);
  if (i == BaseMap::end())
    return std::unexpected(MapException::format("Key '{}' not found in Map::extract_value()", outputAny(k)));
  auto m = std::move(i->second);
  erase(i);
  return m;
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::try_extract_value(key_type const& k) -> std::optional<mapped_type> {
  auto i = BaseMap::find(k);
  if (i == BaseMap::end())
    return {};
  auto m = std::move(i->second);
  erase(i);
  return std::optional<mapped_type>(std::in_place, std::move(m));
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::clear() {
  if (!empty()) {
    addChangeData(ClearChange());
    m_updated = true;
    BaseMap::clear();
  }
}

template <typename BaseMap>
BaseMap const& NetElementMapWrapper<BaseMap>::baseMap() const {
  return *this;
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::reset(BaseMap values) {
  for (auto const& p : *this) {
    if (!values.contains(p.first)) {
      addChangeData(RemoveChange{p.first});
      m_updated = true;
    }
  }

  for (auto const& p : values) {
    auto v = find_ref(p.first);
    if (!v || !(v->get() == p.second)) {
      addChangeData(SetChange{p.first, p.second});
      m_updated = true;
    }
  }

  BaseMap::operator=(std::move(values));
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::pullUpdated() {
  return std::exchange(m_updated, false);
}

template <typename BaseMap>
template <typename MapType>
void NetElementMapWrapper<BaseMap>::setContents(MapType const& values) {
  reset(BaseMap::from(values));
}

template <typename BaseMap>
std::uint64_t NetElementMapWrapper<BaseMap>::changeDataLastVersion() const {
  return m_changeDataLastVersion;
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException>
NetElementMapWrapper<BaseMap>::writeSetChange(DataStream& ds, key_type const& key, mapped_type const& value) {
  {
    auto result = ds.write<std::uint8_t>(0);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  {
    auto result = ds.write(key);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  return ds.write(value);
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::writeChange(DataStream& ds, ElementChange const& change) {
  if (std::holds_alternative<SetChange>(change)) {
    auto const& set = std::get<SetChange>(change);
    return writeSetChange(ds, set.key, set.value);
  } else if (std::holds_alternative<RemoveChange>(change)) {
    { auto result = ds.write<std::uint8_t>(1); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.write(std::get<RemoveChange>(change).key); if (!result) return std::unexpected(std::move(result.error())); }
  } else {
    { auto result = ds.write<std::uint8_t>(2); if (!result) return std::unexpected(std::move(result.error())); }
  }
  return {};
}

template <typename BaseMap>
[[nodiscard]] auto NetElementMapWrapper<BaseMap>::readChange(DataStream& ds) -> std::expected<ElementChange, StarException> {
  auto type = ds.read<std::uint8_t>();
  if (!type) return std::unexpected(std::move(type.error()));
  if (*type == 0) {
    SetChange change;
    { auto result = ds.read(change.key); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.read(change.value); if (!result) return std::unexpected(std::move(result.error())); }
    return change;
  } else if (*type == 1) {
    RemoveChange change;
    { auto result = ds.read(change.key); if (!result) return std::unexpected(std::move(result.error())); }
    return change;
  } else if (*type == 2) {
    return ClearChange();
  }
  return std::unexpected(IOException("Improper type code received in NetElementMapWrapper::readChange"));
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::addChangeData(ElementChange change) {
  std::uint64_t currentVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if constexpr (Star::DebugEnabled) {
    Star::starAssertFailed((m_changeData.empty() || m_changeData.back().first <= currentVersion));
  }

  m_changeData.push_back({currentVersion, std::move(change)});

  m_changeDataLastVersion =
    std::max<std::int64_t>(static_cast<std::int64_t>(currentVersion) - MaxChangeDataVersions, 0);
  while (!m_changeData.empty() && m_changeData.front().first < m_changeDataLastVersion)
    m_changeData.pop_front();
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::addPendingChangeData(ElementChange change, float interpolationTime) {
  if (!m_pendingChangeData.empty() && interpolationTime < m_pendingChangeData.back().first) {
    for (auto& change : std::exchange(m_pendingChangeData, {}))
      applyChange(std::move(change.second));
  }
  m_pendingChangeData.push_back({interpolationTime, std::move(change)});
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::applyChange(ElementChange change) {
  if (std::holds_alternative<SetChange>(change))
    BaseMap::set(std::move(std::get<SetChange>(change).key), std::move(std::get<SetChange>(change).value));
  else if (std::holds_alternative<RemoveChange>(change))
    BaseMap::remove(std::move(std::get<RemoveChange>(change).key));
  else
    BaseMap::clear();
  m_updated = true;
}

}

export module star.net_element_containers;

export namespace Star {
  using ::Star::NetElementMapWrapper;
  using ::Star::NetElementMap;
  using ::Star::NetElementHashMap;
}
