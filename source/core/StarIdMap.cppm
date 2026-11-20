module;
import std;
import star.output_proxy;
import star.format;
import star.map;
import star.data_stream;

namespace Star {

struct IdMapExceptionTag {
  static constexpr std::string_view name() { return "IdMapException"; }
};
using IdMapException = StarError<IdMapExceptionTag, StarException>;

template <typename Key>
concept id_key = std::integral<Key> && std::same_as<Key, std::remove_cv_t<Key>> && !std::same_as<Key, bool>;

template <typename Base>
concept id_map_storage = map_container<Base> && id_key<typename Base::key_type>;

// Only these comparators put consecutive integral IDs next to each other in ascending order.
template <typename Base>
concept ascending_id_map = id_map_storage<Base> && requires(Base const& base, typename Base::key_type id) {
  base.lower_bound(id);
  requires (std::same_as<typename Base::key_compare, std::less<typename Base::key_type>>
            || std::same_as<typename Base::key_compare, std::less<>>
            || std::same_as<typename Base::key_compare, std::ranges::less>);
};

// Maps key ids to values with auto generated ids in a given id range.  Tries
// to cycle through ids as new values are added and avoid re-using ids until
// the id space wraps around.
template <id_map_storage BaseMap>
class IdMapWrapper : private BaseMap {
public:
  using iterator = typename BaseMap::iterator;
  using const_iterator = typename BaseMap::const_iterator;
  using key_type = typename BaseMap::key_type;
  using value_type = typename BaseMap::value_type;
  using mapped_type = typename BaseMap::mapped_type;

  IdMapWrapper() : IdMapWrapper(std::numeric_limits<key_type>::lowest(), std::numeric_limits<key_type>::max()) {}

  [[nodiscard]] static std::expected<IdMapWrapper, StarException> create(key_type min_id, key_type max_id) {
    if (max_id <= min_id)
      return std::unexpected(IdMapException("Invalid id range in IdMapWrapper"));
    return IdMapWrapper(min_id, max_id);
  }

  template <key_type min_id, key_type max_id>
    requires (min_id < max_id)
  static IdMapWrapper with_range() { return IdMapWrapper(min_id, max_id); }

  // Advance the cursor but do not reserve the chosen ID.
  [[nodiscard]] std::expected<key_type, StarException> next_id() {
    key_type id = m_next_id;
    if constexpr (ascending_id_map<BaseMap>) {
      const_iterator position = BaseMap::lower_bound(id);
      if (find_gap(id, position))
        return advance_cursor(id);
    } else {
      do {
        if (!BaseMap::contains(id))
          return advance_cursor(id);
        id = increment_id(id);
      } while (id != m_next_id);
    }
    return exhausted();
  }

  // Returns an error if the key already exists.
  // Explicit ids retain the legacy contract: they need not be in the automatic
  // allocation range and do not advance its cursor.
  template <typename... Args>
    requires std::constructible_from<mapped_type, Args...>
  [[nodiscard]] std::expected<void, StarException> try_emplace(key_type id, Args&&... args) {
    if (!BaseMap::try_emplace(id, std::forward<Args>(args)...).second)
      return std::unexpected(map_diagnostic_error<IdMapException>(
          "IdMapWrapper::try_emplace called with pre-existing id '{}'",
          "IdMapWrapper::try_emplace: id already present while rendering its diagnostic", id));
    return {};
  }

  // Construct only at a free ID; neither collisions nor a throwing constructor commit the cursor.
  template <typename... Args>
    requires std::constructible_from<mapped_type, Args...>
  [[nodiscard]] std::expected<key_type, StarException> emplace(Args&&... args) {
    key_type id = m_next_id;
    do {
      auto [position, inserted] = BaseMap::try_emplace(id, std::forward<Args>(args)...);
      if (inserted)
        return advance_cursor(id);
      if constexpr (ascending_id_map<BaseMap>) {
        const_iterator hint = position;
        if (!find_gap(id, hint))
          break;
        BaseMap::emplace_hint(hint, std::piecewise_construct, std::forward_as_tuple(id),
                              std::forward_as_tuple(std::forward<Args>(args)...));
        return advance_cursor(id);
      } else {
        // Once a run of occupied keys starts, probe without preparing an insertion.
        do {
          id = increment_id(id);
        } while (id != m_next_id && BaseMap::contains(id));
      }
    } while (id != m_next_id);
    return exhausted();
  }

  void clear() {
    BaseMap::clear();
    m_next_id = m_min;
  }

  bool operator==(IdMapWrapper const& other) const {
    return m_min == other.m_min && m_max == other.m_max && BaseMap::operator==(other);
  }

  using BaseMap::begin;
  using BaseMap::contains;
  using BaseMap::empty;
  using BaseMap::end;
  using BaseMap::erase;
  using BaseMap::extract_value;
  using BaseMap::find_ref;
  using BaseMap::get;
  using BaseMap::keys;
  using BaseMap::maybe;
  using BaseMap::pairs;
  using BaseMap::remove;
  using BaseMap::size;
  using BaseMap::try_extract_value;
  using BaseMap::value;
  using BaseMap::values;

  template <id_map_storage Base>
    requires (stream_read_factory<typename Base::key_type> && stream_read_factory<typename Base::mapped_type>)
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, IdMapWrapper<Base>& map);
  template <id_map_storage Base>
    requires (stream_writable<typename Base::key_type> && stream_writable<typename Base::mapped_type>)
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, IdMapWrapper<Base> const& map);

private:
  IdMapWrapper(key_type min_id, key_type max_id) : m_min(min_id), m_max(max_id), m_next_id(min_id) {}

  key_type increment_id(key_type id) const { return id == m_max ? m_min : static_cast<key_type>(id + 1); }

  key_type advance_cursor(key_type id) {
    m_next_id = increment_id(id);
    return id;
  }

  static auto exhausted() { return std::unexpected(IdMapException("No id space left in IdMapWrapper")); }

  // position is lower_bound(id), or the equivalent iterator from a failed try_emplace.
  // On success it is the insertion hint immediately after the gap.
  bool find_gap(key_type& id, const_iterator& position) const requires ascending_id_map<BaseMap> {
    auto scan_through = [&](key_type last) {
      while (position != BaseMap::end() && position->first == id) {
        if (id == last)
          return false;
        ++id;
        ++position;
      }
      return true;
    };
    if (scan_through(m_max))
      return true;
    if (m_min == m_next_id)
      return false;
    id = m_min;
    position = BaseMap::lower_bound(id);
    return scan_through(static_cast<key_type>(m_next_id - 1));
  }

  key_type m_min;
  key_type m_max;
  key_type m_next_id;
};

template <id_key Key, typename Value>
using IdMap = IdMapWrapper<Map<Key, Value>>;

template <id_key Key, typename Value>
using IdHashMap = IdMapWrapper<HashMap<Key, Value>>;

template <id_map_storage BaseMap>
  requires (stream_read_factory<typename BaseMap::key_type> && stream_read_factory<typename BaseMap::mapped_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, IdMapWrapper<BaseMap>& map) {
  // Decode into empty storage with this owner's policies. Neither truncated
  // entries nor invalid metadata may replace the existing map.
  auto const& base = static_cast<BaseMap const&>(map);
  auto entries_map = [&] {
    if constexpr (requires { base.hash_function(); base.key_eq(); }) {
      BaseMap empty(0, base.hash_function(), base.key_eq(), base.get_allocator());
      if constexpr (requires { empty.max_load_factor(base.max_load_factor()); })
        empty.max_load_factor(base.max_load_factor());
      return empty;
    } else {
      return BaseMap(base.key_comp(), base.get_allocator());
    }
  }();
  auto entries = ds.read_map_container(entries_map);
  if (!entries) return std::unexpected(std::move(entries.error()));
  typename BaseMap::key_type min_id, max_id, next_id;
  auto metadata = ds >> min_id >> max_id >> next_id;
  if (!metadata) return std::unexpected(std::move(metadata.error()));
  if (max_id <= min_id || next_id < min_id || next_id > max_id)
    return std::unexpected(IdMapException("Invalid id range or cursor in serialized IdMapWrapper"));
  // Explicit ids outside the automatic range are valid legacy entries.
  static_cast<BaseMap&>(map) = std::move(entries_map);
  map.m_min = min_id;
  map.m_max = max_id;
  map.m_next_id = next_id;
  return std::ref(ds);
}

template <id_map_storage BaseMap>
  requires (stream_writable<typename BaseMap::key_type> && stream_writable<typename BaseMap::mapped_type>)
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, IdMapWrapper<BaseMap> const& map) {
  auto entries = ds.write_map_container(static_cast<BaseMap const&>(map));
  if (!entries) return std::unexpected(std::move(entries.error()));
  return ds << map.m_min << map.m_max << map.m_next_id;
}

}

export module star.id_map;

export namespace Star {
  using ::Star::id_key;
  using ::Star::IdMapExceptionTag;
  using ::Star::IdMapException;
  using ::Star::IdMapWrapper;
  using ::Star::IdMap;
  using ::Star::IdHashMap;
  using ::Star::operator>>;
  using ::Star::operator<<;
}
