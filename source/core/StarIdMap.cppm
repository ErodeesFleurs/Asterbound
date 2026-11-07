module;
import std;
import star.output_proxy;
import star.format;
import star.map;
import star.math_common;
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
    requires (Min < Max)
  static IdMapWrapper withRange() {
    IdMapWrapper result;
    result.m_min = Min;
    result.m_max = Max;
    result.m_nextId = Min;
    return result;
  }

  // New valid id that does not exist in this map.  Tries not to immediately
  // recycle ids, to avoid temporally close id repeats.
  [[nodiscard]] std::expected<IdType, StarException> nextId();

  // Returns an error if the key already exists.
  [[nodiscard]] std::expected<void, StarException> add(IdType id, MappedType mappedType);

  // Add with automatically allocated id
  [[nodiscard]] std::expected<IdType, StarException> add(MappedType mappedType);

  void clear();

  bool operator==(IdMapWrapper const& rhs) const;
  bool operator!=(IdMapWrapper const& rhs) const;

  using BaseMap::keys;
  using BaseMap::values;
  using BaseMap::pairs;
  using BaseMap::contains;
  using BaseMap::size;
  using BaseMap::empty;
  using BaseMap::get;
  using BaseMap::maybe;
  using BaseMap::maybeRef;
  using BaseMap::take;
  using BaseMap::maybeTake;
  using BaseMap::remove;
  using BaseMap::value;
  using BaseMap::begin;
  using BaseMap::end;
  using BaseMap::erase;

  template <typename Base>
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, IdMapWrapper<Base>& map);
  template <typename Base>
  friend std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, IdMapWrapper<Base> const& map);

private:
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

template <typename BaseMap>
auto IdMapWrapper<BaseMap>::nextId() -> std::expected<IdType, StarException> {
  if ((IdType)BaseMap::size() > m_max - m_min)
    return std::unexpected(IdMapException("No id space left in IdMapWrapper"));
  IdType nextId = m_nextId;
  while (BaseMap::contains(nextId))
    nextId = cycleIncrement(nextId, m_min, m_max);
  m_nextId = cycleIncrement(nextId, m_min, m_max);
  return nextId;
}

template <typename BaseMap>
std::expected<void, StarException> IdMapWrapper<BaseMap>::add(IdType id, MappedType mappedType) {
  if (!BaseMap::insert(std::make_pair(id, std::move(mappedType))).second)
    return std::unexpected(mapDiagnosticError<IdMapException>("IdMapWrapper::add(id, value) called with pre-existing id '{}'", "IdMapWrapper::add: id already present while rendering its diagnostic", id));
  return {};
}

template <typename BaseMap>
auto IdMapWrapper<BaseMap>::add(MappedType mappedType) -> std::expected<IdType, StarException> {
  auto id = nextId();
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
  auto entries = ds.readMapContainer((BaseMap&)map);
  if (!entries) return std::unexpected(std::move(entries.error()));
  auto min = ds.read(map.m_min);
  if (!min) return std::unexpected(std::move(min.error()));
  auto max = ds.read(map.m_max);
  if (!max) return std::unexpected(std::move(max.error()));
  auto next = ds.read(map.m_nextId);
  if (!next) return std::unexpected(std::move(next.error()));
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
}
