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

// Maps key ids to values with auto generated ids in a given id range.  Tries
// to cycle through ids as new values are added and avoid re-using ids until
// the id space wraps around.
template <typename BaseMap>
class IdMapWrapper : private BaseMap {
public:
  using iterator = typename BaseMap::iterator;
  using const_iterator = typename BaseMap::const_iterator;
  using key_type = typename BaseMap::key_type;
  using value_type = typename BaseMap::value_type;
  using mapped_type = typename BaseMap::mapped_type;

  using IdType = key_type;
  using ValueType = value_type;
  using MappedType = mapped_type;

  IdMapWrapper();
  [[nodiscard]] static std::expected<IdMapWrapper, StarException> create(IdType min, IdType max);
  template <IdType Min, IdType Max>
    requires(Min < Max)
  static IdMapWrapper with_range() {
    IdMapWrapper result;
    result.m_min = Min;
    result.m_max = Max;
    result.m_nextId = Min;
    return result;
  }

  // New valid id that does not exist in this map.  Tries not to immediately
  // recycle ids, to avoid temporally close id repeats.
  [[nodiscard]] std::expected<IdType, StarException> next_id();

  // Returns an error if the key already exists.
  // Explicit ids retain the legacy contract: they need not be in the automatic
  // allocation range and do not advance its cursor.
  [[nodiscard]] std::expected<void, StarException> add(IdType id, MappedType mappedType);

  // Add with automatically allocated id
  [[nodiscard]] std::expected<IdType, StarException> add(MappedType mappedType);

  void clear();

  bool operator==(IdMapWrapper const& rhs) const;
  bool operator!=(IdMapWrapper const& rhs) const;

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

  template <typename Base>
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, IdMapWrapper<Base>& map);
  template <typename Base>
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, IdMapWrapper<Base> const& map);

private:
  IdType increment_id(IdType id) const { return id == m_max ? m_min : static_cast<IdType>(id + 1); }

  IdType m_min;
  IdType m_max;
  IdType m_nextId;
};

template <class Key, class Value>
using IdMap = IdMapWrapper<Map<Key, Value>>;

template <class Key, class Value>
using IdHashMap = IdMapWrapper<HashMap<Key, Value>>;

template <typename BaseMap>
IdMapWrapper<BaseMap>::IdMapWrapper()
  : m_min(std::numeric_limits<IdType>::lowest()), m_max(std::numeric_limits<IdType>::max()), m_nextId(m_min) {}

template <typename BaseMap>
auto IdMapWrapper<BaseMap>::create(IdType min, IdType max) -> std::expected<IdMapWrapper, StarException> {
  if (max <= min)
    return std::unexpected(IdMapException("Invalid id range in IdMapWrapper"));
  IdMapWrapper result;
  result.m_min = min;
  result.m_max = max;
  result.m_nextId = min;
  return result;
}

template <typename BaseMap> auto IdMapWrapper<BaseMap>::next_id() -> std::expected<IdType, StarException> {
  auto next_id = m_nextId;
  do {
    if (!BaseMap::contains(next_id)) {
      m_nextId = increment_id(next_id);
      return next_id;
    }
    next_id = increment_id(next_id);
  } while (next_id != m_nextId);
  return std::unexpected(IdMapException("No id space left in IdMapWrapper"));
}

template <typename BaseMap>
std::expected<void, StarException> IdMapWrapper<BaseMap>::add(IdType id, MappedType mappedType) {
  if (!BaseMap::insert(std::make_pair(id, std::move(mappedType))).second)
    return std::unexpected(
      map_diagnostic_error<IdMapException>("IdMapWrapper::add(id, value) called with pre-existing id '{}'",
                                           "IdMapWrapper::add: id already present while rendering its diagnostic", id));
  return {};
}

template <typename BaseMap>
auto IdMapWrapper<BaseMap>::add(MappedType mappedType) -> std::expected<IdType, StarException> {
  auto id = next_id();
  if (!id)
    return std::unexpected(std::move(id.error()));
  BaseMap::insert(*id, std::move(mappedType));
  return *id;
}

template <typename BaseMap>
void IdMapWrapper<BaseMap>::clear() {
  BaseMap::clear();
  m_nextId = m_min;
}

template <typename BaseMap>
bool IdMapWrapper<BaseMap>::operator==(IdMapWrapper const& rhs) const {
  return std::tie(m_min, m_max) == std::tie(rhs.m_min, rhs.m_max) && BaseMap::operator==(rhs);
}

template <typename BaseMap>
bool IdMapWrapper<BaseMap>::operator!=(IdMapWrapper const& rhs) const {
  return !operator==(rhs);
}

template <typename BaseMap>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, IdMapWrapper<BaseMap>& map) {
  // Decode into empty storage with this owner's policies. Neither truncated
  // entries nor invalid metadata may replace the existing map.
  auto const& base = static_cast<BaseMap const&>(map);
  auto entries_map = [&] {
    if constexpr (requires {
                    base.hash_function();
                    base.key_eq();
                  })
      return BaseMap(0, base.hash_function(), base.key_eq(), base.get_allocator());
    else
      return BaseMap(base.key_comp(), base.get_allocator());
  }();
  auto entries = ds.readMapContainer(entries_map);
  if (!entries) return std::unexpected(std::move(entries.error()));
  typename IdMapWrapper<BaseMap>::IdType min_id, max_id, next_id;
  auto min = ds.read(min_id);
  if (!min) return std::unexpected(std::move(min.error()));
  auto max = ds.read(max_id);
  if (!max) return std::unexpected(std::move(max.error()));
  auto next = ds.read(next_id);
  if (!next) return std::unexpected(std::move(next.error()));
  if (max_id <= min_id || next_id < min_id || next_id > max_id)
    return std::unexpected(IdMapException("Invalid id range or cursor in serialized IdMapWrapper"));
  // Explicit ids outside the automatic range are valid legacy entries.
  static_cast<BaseMap&>(map) = std::move(entries_map);
  map.m_min = min_id;
  map.m_max = max_id;
  map.m_nextId = next_id;
  return std::ref(ds);
}

template <typename BaseMap>
[[nodiscard]] std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, IdMapWrapper<BaseMap> const& map) {
  auto entries = ds.writeMapContainer((BaseMap const&)map);
  if (!entries) return std::unexpected(std::move(entries.error()));
  auto min = ds.write(map.m_min);
  if (!min) return std::unexpected(std::move(min.error()));
  auto max = ds.write(map.m_max);
  if (!max) return std::unexpected(std::move(max.error()));
  auto next = ds.write(map.m_nextId);
  if (!next) return std::unexpected(std::move(next.error()));
  return std::ref(ds);
}

}

export module star.id_map;

export namespace Star {
  using ::Star::IdMapExceptionTag;
  using ::Star::IdMapException;
  using ::Star::IdMapWrapper;
  using ::Star::IdMap;
  using ::Star::IdHashMap;
  using ::Star::operator>>;
  using ::Star::operator<<;
}
