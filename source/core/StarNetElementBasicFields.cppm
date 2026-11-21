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

template <typename Mutator, typename T>
concept net_field_mutator = requires(Mutator& mutator, T& value) {
  mutator(value) ? true : false;
};

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

  // Has this field been updated since the last call to pull_updated?
  bool pull_updated();

  // Update the value in place.  The mutator will be called as bool
  // mutator(T&), return true to signal that the value was updated.
  template <typename Mutator>
    requires net_field_mutator<Mutator, T>
  void update(Mutator&& mutator);

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

protected:
  [[nodiscard]] virtual std::expected<void, StarException> read_data(DataStream& ds, T& t) const = 0;
  [[nodiscard]] virtual std::expected<void, StarException> write_data(DataStream& ds, T const& t) const = 0;

  virtual void updated();

private:
  T const& wire_value() const &;
  void mark_local_update();

  std::optional<std::reference_wrapper<NetElementVersion const>> m_net_version = std::nullopt;
  std::uint64_t m_latest_update_version = 0;
  T m_value = T();
  bool m_updated = false;
  std::optional<Deque<std::pair<float, T>>> m_pending_interpolated_values;
};

template <typename T>
class NetElementIntegral : public NetElementBasicField<T> {
protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, T& v) const override;
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, T const& v) const override;
};

using NetElementInt = NetElementIntegral<std::int64_t>;
using NetElementUInt = NetElementIntegral<std::uint64_t>;

// Properly encodes NPos no matter the platform width of size_t NetElement
// size_t values are NOT clamped when setting.
class NetElementSize : public NetElementBasicField<std::size_t> {
protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, std::size_t& v) const override;
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, std::size_t const& v) const override;
};

class NetElementBool : public NetElementBasicField<bool> {
protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, bool& v) const override;
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, bool const& v) const override;
};

template <typename Enum>
class NetElementEnum : public NetElementBasicField<Enum> {
protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, Enum& v) const override;
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, Enum const& v) const override;
};

// Wraps a uint64_t to give a simple event stream.  Every trigger is an
// increment to a held uint64_t value, and slaves can see how many triggers
// have occurred since the last check.
class NetElementEvent : public NetElementUInt {
public:
  void trigger();

  // Returns the number of times this event has been triggered since the last
  // pull_occurrences call.
  std::uint64_t pull_occurrences();

  // Pulls whether this event occurred at all, ignoring the number
  bool pull_occurred();

  // Ignore all the existing ocurrences
  void ignore_occurrences();
  void set_ignore_occurrences_on_net_load(bool ignore_occurrences_on_net_load);

  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

protected:
  void updated() override;

private:
  using NetElementUInt::get;
  using NetElementUInt::set;
  using NetElementUInt::push;
  using NetElementUInt::update;

  std::uint64_t m_pulled_occurrences = 0;
  bool m_ignore_occurrences_on_net_load = false;
};

template <typename T>
concept net_field_value = requires(T& value) {
  T();
  T(std::move(value));
  value = std::move(value);
};

// The common case needs only the payload and field state.
template <net_field_value T>
  requires (stream_readable<T> && stream_writable<T>)
class NetElementData : public NetElementBasicField<T> {
protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, T& v) const override {
    return ds.read(v);
  }
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, T const& v) const override {
    return ds.write(v);
  }
};

template <net_field_value T>
class NetElementCustomData : public NetElementBasicField<T> {
public:
  NetElementCustomData(std::function<std::expected<void, StarException>(DataStream&, T&)> reader,
      std::function<std::expected<void, StarException>(DataStream&, T const&)> writer)
    : m_reader(std::move(reader)), m_writer(std::move(writer)) {}

protected:
  [[nodiscard]] std::expected<void, StarException> read_data(DataStream& ds, T& v) const override {
    if (!m_reader) return std::unexpected(StarException("Missing NetElementData reader"));
    return m_reader(ds, v);
  }
  [[nodiscard]] std::expected<void, StarException> write_data(DataStream& ds, T const& v) const override {
    if (!m_writer) return std::unexpected(StarException("Missing NetElementData writer"));
    return m_writer(ds, v);
  }

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
T const& NetElementBasicField<T>::wire_value() const & {
  return m_pending_interpolated_values && !m_pending_interpolated_values->empty()
      ? m_pending_interpolated_values->back().second : m_value;
}

template <typename T>
void NetElementBasicField<T>::set(T const& value) {
  if (!(m_value == value))
    push(value);
}

template <typename T>
void NetElementBasicField<T>::push(T value) {
  m_value = std::move(value);
  mark_local_update();
}

template <typename T>
void NetElementBasicField<T>::mark_local_update() {
  updated();
  m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;
  if (m_pending_interpolated_values)
    m_pending_interpolated_values->clear();
}

template <typename T>
bool NetElementBasicField<T>::pull_updated() {
  return std::exchange(m_updated, false);
}

template <typename T>
template <typename Mutator>
  requires net_field_mutator<Mutator, T>
void NetElementBasicField<T>::update(Mutator&& mutator) {
  if (mutator(m_value))
    mark_local_update();
}

template <typename T>
void NetElementBasicField<T>::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_net_version = version;
  m_latest_update_version = 0;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::enable_net_interpolation(float) {
  if (!m_pending_interpolated_values)
    m_pending_interpolated_values.emplace();
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::disable_net_interpolation() {
  if (m_pending_interpolated_values) {
    if (!m_pending_interpolated_values->empty())
      m_value = std::move(m_pending_interpolated_values->back().second);
    m_pending_interpolated_values.reset();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::tick_net_interpolation(float dt) {
  if (m_pending_interpolated_values) {
    for (auto& p : *m_pending_interpolated_values)
      p.first -= dt;
    while (!m_pending_interpolated_values->empty() && m_pending_interpolated_values->front().first <= 0.0f) {
      m_value = std::move(m_pending_interpolated_values->front().second);
      m_pending_interpolated_values->pop_front();
      updated();
    }
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  return write_data(ds, wire_value());
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  { auto result = read_data(ds, m_value); if (!result) return std::unexpected(std::move(result.error())); }
  m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;
  updated();
  if (m_pending_interpolated_values)
    m_pending_interpolated_values->clear();
  return {};
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> NetElementBasicField<T>::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;
  if (m_latest_update_version < from_version)
    return false;

  auto result = write_data(ds, wire_value());
  if (!result) return std::unexpected(std::move(result.error()));

  return true;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementBasicField<T>::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  T t;
  { auto result = read_data(ds, t); if (!result) return std::unexpected(std::move(result.error())); }
  m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;
  // A non-forward step replaces the visible value and cancels queued steps.
  if (m_pending_interpolated_values && interpolation_time > 0.0f
      && (m_pending_interpolated_values->empty() || interpolation_time >= m_pending_interpolated_values->back().first)) {
    m_pending_interpolated_values->emplace_back(interpolation_time, std::move(t));
    return {};
  }
  m_value = std::move(t);
  if (m_pending_interpolated_values)
    m_pending_interpolated_values->clear();
  updated();
  return {};
}

template <typename T>
void NetElementBasicField<T>::updated() {
  m_updated = true;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementIntegral<T>::read_data(DataStream& ds, T& v) const {
  if constexpr (sizeof(T) == 1) {
    return ds.read(v);
  } else if constexpr (std::is_unsigned_v<T>) {
    auto result = ds.read_vlq_u();
    if (!result) return std::unexpected(std::move(result.error()));
    v = *result;
  } else {
    auto result = ds.read_vlq_i();
    if (!result) return std::unexpected(std::move(result.error()));
    v = *result;
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementIntegral<T>::write_data(DataStream& ds, T const& v) const {
  if constexpr (sizeof(T) == 1) return ds.write(v);
  else if constexpr (std::is_unsigned_v<T>) return ds.vuwrite(v);
  else return ds.viwrite(v);
}

template <typename Enum>
[[nodiscard]] std::expected<void, StarException> NetElementEnum<Enum>::read_data(DataStream& ds, Enum& v) const {
  if constexpr (sizeof(Enum) == 1) return ds.read(v);
  else {
    auto result = ds.read_vlq_i();
    if (!result) return std::unexpected(std::move(result.error()));
    v = static_cast<Enum>(*result);
    return {};
  }
}

template <typename Enum>
[[nodiscard]] std::expected<void, StarException> NetElementEnum<Enum>::write_data(DataStream& ds, Enum const& v) const {
  if constexpr (sizeof(Enum) == 1) return ds.write(v);
  else
    return ds.viwrite(static_cast<std::int64_t>(v));
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
  using ::Star::NetElementCustomData;
  using ::Star::NetElementString;
  using ::Star::NetElementBytes;
}
