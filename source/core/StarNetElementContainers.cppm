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

  void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
  [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  bool should_write_delta(std::uint64_t from_version, NetCompatibilityRules rules = {}) const;
  [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

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

  BaseMap const& base_map() const;
  void reset(BaseMap values);
  bool pull_updated();

  // Sets this map to contain the same keys / values as the given map.  All
  // values in this map not found in the given map are removed.  (Same as
  // reset, but with arbitrary map type).
  template <typename MapType>
  void set_contents(MapType const& values);

  std::uint64_t change_data_last_version() const;

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

  [[nodiscard]] static std::expected<void, StarException> write_set_change(DataStream& ds, key_type const& key,
                                                                         mapped_type const& value);
  [[nodiscard]] static std::expected<void, StarException> write_change(DataStream& ds, ElementChange const& change);
  [[nodiscard]] static std::expected<ElementChange, StarException> read_change(DataStream& ds);

  void add_change_data(ElementChange change);

  void add_pending_change_data(ElementChange change, float interpolation_time);
  void apply_change(ElementChange change);

  Deque<std::pair<std::uint64_t, ElementChange>> m_change_data;
  Deque<std::pair<float, ElementChange>> m_pending_change_data;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_net_version = std::nullopt;
  std::uint64_t m_change_data_last_version = 0;
  bool m_updated = false;
  bool m_interpolation_enabled = false;
};

template <typename Key, typename Value>
using NetElementMap = NetElementMapWrapper<Map<Key, Value>>;

template <typename Key, typename Value>
using NetElementHashMap = NetElementMapWrapper<HashMap<Key, Value>>;

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_net_version = version;

  m_change_data.clear();
  m_change_data_last_version = 0;

  for (auto& change : std::exchange(m_pending_change_data, {}))
    apply_change(std::move(change.second));

  add_change_data(ClearChange());
  for (auto const& p : *this)
    add_change_data(SetChange{p.first, p.second});
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::enable_net_interpolation(float) {
  m_interpolation_enabled = true;
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::disable_net_interpolation() {
  m_interpolation_enabled = false;
  for (auto& change : std::exchange(m_pending_change_data, {}))
    apply_change(std::move(change.second));
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::tick_net_interpolation(float dt) {
  for (auto& p : m_pending_change_data)
    p.first -= dt;

  while (!m_pending_change_data.empty() && m_pending_change_data.front().first <= 0.0f) {
    apply_change(std::move(m_pending_change_data.front().second));
    m_pending_change_data.pop_front();
  }
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  {
    auto result = ds.write_vlq_u(BaseMap::size() + m_pending_change_data.size());
    if (!result)
      return std::unexpected(std::move(result.error()));
  }
  for (auto const& pair : *this) {
    auto result = write_set_change(ds, pair.first, pair.second);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  for (auto const& p : m_pending_change_data)
    { auto result = write_change(ds, p.second); if (!result) return std::unexpected(std::move(result.error())); }
  return {};
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  m_change_data.clear();
  m_change_data_last_version = m_net_version ? m_net_version->get().current() : 0;
  m_pending_change_data.clear();
  BaseMap::clear();

  add_change_data(ClearChange());

  auto count_result = ds.read_vlq_u();
  if (!count_result) return std::unexpected(std::move(count_result.error()));
  std::uint64_t count = *count_result;
  for (std::uint64_t i = 0; i < count; ++i) {
    auto change = read_change(ds);
    if (!change) return std::unexpected(std::move(change.error()));
    add_change_data(*change);
    apply_change(std::move(*change));
  }

  m_updated = true;
  return {};
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::should_write_delta(std::uint64_t from_version, NetCompatibilityRules rules) const {
  return check_with_rules(rules) && (from_version < m_change_data_last_version ||
      (!m_change_data.empty() && m_change_data.back().first >= from_version));
}

template <typename BaseMap>
[[nodiscard]] std::expected<bool, StarException> NetElementMapWrapper<BaseMap>::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!should_write_delta(from_version, rules)) return false;

  if (from_version < m_change_data_last_version) {
    {
      auto result = ds.write_vlq_u(1);
      if (!result)
        return std::unexpected(std::move(result.error()));
    }
    { auto result = net_store(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }

  } else {
    for (auto const& p : m_change_data) {
      if (p.first >= from_version) {
        {
          auto result = ds.write_vlq_u(2);
          if (!result)
            return std::unexpected(std::move(result.error()));
        }
        { auto result = write_change(ds, p.second); if (!result) return std::unexpected(std::move(result.error())); }
      }
    }
  }

  auto result = ds.write_vlq_u(0);
  if (!result) return std::unexpected(std::move(result.error()));
  return true;
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  while (true) {
    auto code_result = ds.read_vlq_u();
    if (!code_result) return std::unexpected(std::move(code_result.error()));
    std::uint64_t code = *code_result;
    if (code == 0) {
      break;
    } else if (code == 1) {
      { auto result = net_load(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    } else if (code == 2) {
      auto change = read_change(ds);
      if (!change) return std::unexpected(std::move(change.error()));
      add_change_data(*change);

      if (m_interpolation_enabled && interpolation_time > 0.0f)
        add_pending_change_data(std::move(*change), interpolation_time);
      else
        apply_change(std::move(*change));
    } else {
      return std::unexpected(IOException("Improper delta code received in NetElementMapWrapper::read_net_delta"));
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
    add_change_data(SetChange{std::move(v.first), std::move(v.second)});
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
    return std::unexpected(MapException::format("Entry with key '{}' already present.", Star::output_any(k)));
  insert(value_type(std::move(k), std::move(v)));
  return {};
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::set(key_type k, mapped_type v) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    if (!(i->second == v)) {
      add_change_data(SetChange{std::move(k), v});
      i->second = std::move(v);
      m_updated = true;
    }
  } else {
    add_change_data(SetChange{k, v});
    BaseMap::insert(value_type(std::move(k), std::move(v)));
    m_updated = true;
  }
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::push(key_type k, mapped_type v) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    add_change_data(SetChange(std::move(k), v));
    i->second = std::move(v);
  } else {
    add_change_data(SetChange(k, v));
    BaseMap::insert(value_type(std::move(k), std::move(v)));
  }
  m_updated = true;
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::remove(key_type const& k) {
  auto i = BaseMap::find(k);
  if (i != BaseMap::end()) {
    erase(i);
    return true;
  }
  return false;
}

template <typename BaseMap>
auto NetElementMapWrapper<BaseMap>::erase(const_iterator i) -> const_iterator {
  add_change_data(RemoveChange(i->first));
  m_updated = true;
  return BaseMap::erase(i);
}

template <typename BaseMap>
[[nodiscard]] auto NetElementMapWrapper<BaseMap>::extract_value(key_type const& k)
  -> std::expected<mapped_type, StarException> {
  auto i = BaseMap::find(k);
  if (i == BaseMap::end())
    return std::unexpected(MapException::format("Key '{}' not found in Map::extract_value()", Star::output_any(k)));
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
    add_change_data(ClearChange());
    m_updated = true;
    BaseMap::clear();
  }
}

template <typename BaseMap>
BaseMap const& NetElementMapWrapper<BaseMap>::base_map() const {
  return *this;
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::reset(BaseMap values) {
  for (auto const& p : *this) {
    if (!values.contains(p.first)) {
      add_change_data(RemoveChange{p.first});
      m_updated = true;
    }
  }

  for (auto const& p : values) {
    auto v = find_ref(p.first);
    if (!v || !(v->get() == p.second)) {
      add_change_data(SetChange{p.first, p.second});
      m_updated = true;
    }
  }

  BaseMap::operator=(std::move(values));
}

template <typename BaseMap>
bool NetElementMapWrapper<BaseMap>::pull_updated() {
  return std::exchange(m_updated, false);
}

template <typename BaseMap>
template <typename MapType>
void NetElementMapWrapper<BaseMap>::set_contents(MapType const& values) {
  reset(BaseMap::from(values));
}

template <typename BaseMap>
std::uint64_t NetElementMapWrapper<BaseMap>::change_data_last_version() const {
  return m_change_data_last_version;
}

template <typename BaseMap>
[[nodiscard]] std::expected<void, StarException>
NetElementMapWrapper<BaseMap>::write_set_change(DataStream& ds, key_type const& key, mapped_type const& value) {
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
[[nodiscard]] std::expected<void, StarException> NetElementMapWrapper<BaseMap>::write_change(DataStream& ds, ElementChange const& change) {
  if (std::holds_alternative<SetChange>(change)) {
    auto const& set = std::get<SetChange>(change);
    return write_set_change(ds, set.key, set.value);
  } else if (std::holds_alternative<RemoveChange>(change)) {
    { auto result = ds.write<std::uint8_t>(1); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = ds.write(std::get<RemoveChange>(change).key); if (!result) return std::unexpected(std::move(result.error())); }
  } else {
    { auto result = ds.write<std::uint8_t>(2); if (!result) return std::unexpected(std::move(result.error())); }
  }
  return {};
}

template <typename BaseMap>
[[nodiscard]] auto NetElementMapWrapper<BaseMap>::read_change(DataStream& ds) -> std::expected<ElementChange, StarException> {
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
  return std::unexpected(IOException("Improper type code received in NetElementMapWrapper::read_change"));
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::add_change_data(ElementChange change) {
  std::uint64_t current_version = m_net_version ? m_net_version->get().current() : 0;
  if constexpr (Star::DebugEnabled) {
    Star::star_assert_failed((m_change_data.empty() || m_change_data.back().first <= current_version));
  }

  m_change_data.push_back({current_version, std::move(change)});

  m_change_data_last_version =
    std::max<std::int64_t>(static_cast<std::int64_t>(current_version) - MaxChangeDataVersions, 0);
  while (!m_change_data.empty() && m_change_data.front().first < m_change_data_last_version)
    m_change_data.pop_front();
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::add_pending_change_data(ElementChange change, float interpolation_time) {
  if (!m_pending_change_data.empty() && interpolation_time < m_pending_change_data.back().first) {
    for (auto& change : std::exchange(m_pending_change_data, {}))
      apply_change(std::move(change.second));
  }
  m_pending_change_data.push_back({interpolation_time, std::move(change)});
}

template <typename BaseMap>
void NetElementMapWrapper<BaseMap>::apply_change(ElementChange change) {
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
