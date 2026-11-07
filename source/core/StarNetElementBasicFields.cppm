module;
import star.net_compatibility;
import star.format;

import std;
import star.list;
import star.data_stream;
import star.data_stream_extra;
import star.string;
import star.byte_array;

import star.net_element;

import star.algorithm;
namespace Star {

template <typename T>
class NetElementBasicField : public NetElement {
public:
  ~NetElementBasicField() override = default;

  T const& get() const;

  // Updates the value if the value is different than the existing value,
  // requires T have operator==
  void set(T const& value);

  // Always updates the value and marks it as updated.
  void push(T value);

  // Has this field been updated since the last call to pullUpdated?
  bool pullUpdated();

  // Update the value in place.  The mutator will be called as bool
  // mutator(T&), return true to signal that the value was updated.
  template <typename Mutator>
  void update(Mutator&& mutator);

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

protected:
  [[nodiscard]] virtual std::expected<void, StarException> readData(DataStream& ds, T& t) const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> writeData(DataStream& ds, T const& t) const = 0;

  virtual void updated();

private:
  std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion = std::nullopt;
  std::uint64_t m_latestUpdateVersion = 0;
  T m_value = T();
  bool m_updated = false;
  std::optional<Deque<std::pair<float, T>>> m_pendingInterpolatedValues;
};

template <typename T>
class NetElementIntegral : public NetElementBasicField<T> {
protected:
  [[nodiscard]] std::expected<void, StarException> readData(DataStream& ds, T& v) const override;
  [[nodiscard]] std::expected<void, StarException> writeData(DataStream& ds, T const& v) const override;
};

using NetElementInt = NetElementIntegral<std::int64_t>;
using NetElementUInt = NetElementIntegral<std::uint64_t>;

// Properly encodes NPos no matter the platform width of size_t NetElement
// size_t values are NOT clamped when setting.
class NetElementSize : public NetElementBasicField<std::size_t> {
protected:
  [[nodiscard]] std::expected<void, StarException> readData(DataStream& ds, std::size_t& v) const override;
  [[nodiscard]] std::expected<void, StarException> writeData(DataStream& ds, std::size_t const& v) const override;
};

class NetElementBool : public NetElementBasicField<bool> {
protected:
  [[nodiscard]] std::expected<void, StarException> readData(DataStream& ds, bool& v) const override;
  [[nodiscard]] std::expected<void, StarException> writeData(DataStream& ds, bool const& v) const override;
};

template <typename Enum>
class NetElementEnum : public NetElementBasicField<Enum> {
protected:
  [[nodiscard]] std::expected<void, StarException> readData(DataStream& ds, Enum& v) const override;
  [[nodiscard]] std::expected<void, StarException> writeData(DataStream& ds, Enum const& v) const override;
};

// Wraps a uint64_t to give a simple event stream.  Every trigger is an
// increment to a held uint64_t value, and slaves can see how many triggers
// have occurred since the last check.
class NetElementEvent : public NetElementUInt {
public:
  void trigger();

  // Returns the number of times this event has been triggered since the last
  // pullOccurrences call.
  std::uint64_t pullOccurrences();

  // Pulls whether this event occurred at all, ignoring the number
  bool pullOccurred();

  // Ignore all the existing ocurrences
  void ignoreOccurrences();
  void setIgnoreOccurrencesOnNetLoad(bool ignoreOccurrencesOnNetLoad);

  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

protected:
  void updated() override;

private:
  using NetElementUInt::get;
  using NetElementUInt::set;
  using NetElementUInt::push;
  using NetElementUInt::update;

  std::uint64_t m_pulledOccurrences = 0;
  bool m_ignoreOccurrencesOnNetLoad = false;
};

// Holds an arbitrary serializable value
template <typename T>
class NetElementData : public NetElementBasicField<T> {
public:
  NetElementData();
  NetElementData(std::function<std::expected<void, StarException>(DataStream&, T&)> reader, std::function<std::expected<void, StarException>(DataStream&, T const&)> writer);

protected:
  [[nodiscard]] std::expected<void, StarException> readData(DataStream& ds, T& v) const override;
  [[nodiscard]] std::expected<void, StarException> writeData(DataStream& ds, T const& v) const override;

private:
  std::function<std::expected<void, StarException>(DataStream&, T&)> m_reader;
  std::function<std::expected<void, StarException>(DataStream&, T const&)> m_writer;
};

using NetElementString = NetElementData<String>;
using NetElementBytes = NetElementData<ByteArray>;

template <typename T>
T const& NetElementBasicField<T>::get() const {
  return m_value;
}

template <typename T>
void NetElementBasicField<T>::set(T const& value) {
  if (!(m_value == value))
    push(value);
}

template <typename T>
void NetElementBasicField<T>::push(T value) {
  m_value = std::move(value);
  updated();
  m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if (m_pendingInterpolatedValues)
    m_pendingInterpolatedValues->clear();
}

template <typename T>
bool NetElementBasicField<T>::pullUpdated() {
  return std::exchange(m_updated, false);
}

template <typename T>
template <typename Mutator>
void NetElementBasicField<T>::update(Mutator&& mutator) {
  if (mutator(m_value)) {
    updated();
    m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
    if (m_pendingInterpolatedValues)
      m_pendingInterpolatedValues->clear();
  }
}

template <typename T>
void NetElementBasicField<T>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_latestUpdateVersion = 0;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::enableNetInterpolation(float) {
  if (!m_pendingInterpolatedValues)
    m_pendingInterpolatedValues.emplace();
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::disableNetInterpolation() {
  if (m_pendingInterpolatedValues) {
    if (!m_pendingInterpolatedValues->empty())
      m_value = std::move(m_pendingInterpolatedValues->back().second);
    m_pendingInterpolatedValues.reset();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::tickNetInterpolation(float dt) {
  if (m_pendingInterpolatedValues) {
    for (auto& p : *m_pendingInterpolatedValues)
      p.first -= dt;
    while (!m_pendingInterpolatedValues->empty() && m_pendingInterpolatedValues->front().first <= 0.0f) {
      m_value = std::move(m_pendingInterpolatedValues->front().second);
      m_pendingInterpolatedValues->pop_front();
      updated();
    }
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  if (m_pendingInterpolatedValues && !m_pendingInterpolatedValues->empty())
    return writeData(ds, m_pendingInterpolatedValues->back().second);
  else
    return writeData(ds, m_value);
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  { auto result = readData(ds, m_value); if (!result) return std::unexpected(std::move(result.error())); }
  m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
  updated();
  if (m_pendingInterpolatedValues)
    m_pendingInterpolatedValues->clear();
  return {};
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> NetElementBasicField<T>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  if (m_latestUpdateVersion < fromVersion)
    return false;

  auto result = (m_pendingInterpolatedValues && !m_pendingInterpolatedValues->empty())
      ? writeData(ds, m_pendingInterpolatedValues->back().second) : writeData(ds, m_value);
  if (!result) return std::unexpected(std::move(result.error()));

  return true;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  T t;
  { auto result = readData(ds, t); if (!result) return std::unexpected(std::move(result.error())); }
  m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if (m_pendingInterpolatedValues) {
    // Only append an incoming delta to our pending value list if the incoming
    // step is forward in time of every other pending value.  In any other
    // case, this is an error or the step tracking is wildly off, so just clear
    // any other incoming values.
    if (interpolationTime > 0.0f && (m_pendingInterpolatedValues->empty() || interpolationTime >= m_pendingInterpolatedValues->back().first)) {
      m_pendingInterpolatedValues->append({interpolationTime, std::move(t)});
    } else {
      m_value = std::move(t);
      m_pendingInterpolatedValues->clear();
      updated();
    }
  } else {
    m_value = std::move(t);
    updated();
  }
  return {};
}

template <typename T>
void NetElementBasicField<T>::updated() {
  m_updated = true;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementIntegral<T>::readData(DataStream& ds, T& v) const {
  if constexpr (sizeof(T) == 1) {
    return ds.read(v);
  } else if constexpr (std::is_unsigned_v<T>) {
    auto result = ds.readVlqU();
    if (!result) return std::unexpected(std::move(result.error()));
    v = *result;
  } else {
    auto result = ds.readVlqI();
    if (!result) return std::unexpected(std::move(result.error()));
    v = *result;
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementIntegral<T>::writeData(DataStream& ds, T const& v) const {
  if constexpr (sizeof(T) == 1) return ds.write(v);
  else if constexpr (std::is_unsigned_v<T>) return ds.vuwrite(v);
  else return ds.viwrite(v);
}

template <typename Enum>
[[nodiscard]] std::expected<void, StarException> NetElementEnum<Enum>::readData(DataStream& ds, Enum& v) const {
  if constexpr (sizeof(Enum) == 1) return ds.read(v);
  else {
    auto result = ds.readVlqI();
    if (!result) return std::unexpected(std::move(result.error()));
    v = static_cast<Enum>(*result);
    return {};
  }
}

template <typename Enum>
[[nodiscard]] std::expected<void, StarException> NetElementEnum<Enum>::writeData(DataStream& ds, Enum const& v) const {
  if constexpr (sizeof(Enum) == 1) return ds.write(v);
  else
    return ds.viwrite(static_cast<std::int64_t>(v));
}

template <typename T>
NetElementData<T>::NetElementData()
  : NetElementData([](DataStream& ds, T& t) -> std::expected<void, StarException> { return ds.read(t); },
      [](DataStream& ds, T const& t) -> std::expected<void, StarException> { return ds.write(t); }) {}

template <typename T>
NetElementData<T>::NetElementData(std::function<std::expected<void, StarException>(DataStream&, T&)> reader, std::function<std::expected<void, StarException>(DataStream&, T const&)> writer)
  : m_reader(std::move(reader)), m_writer(std::move(writer)) {}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementData<T>::readData(DataStream& ds, T& v) const {
  if (!m_reader) return std::unexpected(StarException("Missing NetElementData reader"));
  return m_reader(ds, v);
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementData<T>::writeData(DataStream& ds, T const& v) const {
  if (!m_writer) return std::unexpected(StarException("Missing NetElementData writer"));
  return m_writer(ds, v);
}

}

export module star.net_element_basic_fields;

export namespace Star {
  using ::Star::NetElementBasicField;
  using ::Star::NetElementIntegral;
  using ::Star::NetElementInt;
  using ::Star::NetElementUInt;
  using ::Star::NetElementSize;
  using ::Star::NetElementBool;
  using ::Star::NetElementEnum;
  using ::Star::NetElementEvent;
  using ::Star::NetElementData;
  using ::Star::NetElementString;
  using ::Star::NetElementBytes;
}
