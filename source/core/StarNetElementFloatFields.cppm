module;
import star.net_compatibility;

import std;
import star.list;
import star.format;
import star.data_stream;
import star.interpolation;

import star.net_element;

import star.math_common;
namespace Star {

struct StepStreamExceptionTag {
  static constexpr std::string_view name() { return "StepStreamException"; }
};
using StepStreamException = StarError<StepStreamExceptionTag, StarException>;

template <std::floating_point T>
class NetElementFloating : public NetElement {
public:
  NetElementFloating() = default;
  NetElementFloating(NetElementFloating const&) = default;
  NetElementFloating& operator=(NetElementFloating const&) = default;
  NetElementFloating(NetElementFloating&& other);
  NetElementFloating& operator=(NetElementFloating&& other);

  T get() const;
  void set(T value);

  // If a fixed point base is given, then instead of transmitting the value as
  // a float, it is transmitted as a VLQ of the value divided by the fixed
  // point base.  Any NetElementFloating that is transmitted to must also have
  // the same fixed point base set.
  void set_fixed_point_base(std::optional<T> fixed_point_base = {});

  // If interpolation is enabled on the NetStepStates parent, and an
  // interpolator is set, then on steps in between data points this will be
  // used to interpolate this value.  It is not necessary that senders and
  // receivers both have matching interpolation functions, or any interpolation
  // functions at all.
  void set_interpolator(std::function<T(T, T, T)> interpolator);

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
  [[nodiscard]] std::expected<void, StarException> write_value(DataStream& ds, T t) const;
  [[nodiscard]] std::expected<T, StarException> read_value(DataStream& ds) const;

  T interpolate() const;

  std::optional<T> m_fixed_point_base;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_net_version = std::nullopt;
  std::uint64_t m_latest_update_version = 0;
  T m_value = T();

  std::function<T(T, T, T)> m_interpolator;
  float m_extrapolation = 0.0f;
  std::optional<Deque<std::pair<float, T>>> m_interpolation_data_points;
};

using NetElementFloat = NetElementFloating<float>;
using NetElementDouble = NetElementFloating<double>;

template <std::floating_point T>
NetElementFloating<T>::NetElementFloating(NetElementFloating&& other)
  : NetElement(std::move(other)),
    m_fixed_point_base(std::move(other.m_fixed_point_base)),
    m_net_version(other.m_net_version),
    m_latest_update_version(other.m_latest_update_version),
    m_value(std::move(other.m_value)),
    m_interpolator(std::move(other.m_interpolator)),
    m_extrapolation(other.m_extrapolation),
    m_interpolation_data_points(std::move(other.m_interpolation_data_points)) {
  // A moved-from field must no longer advertise fixed-point or interpolation state.
  other.m_fixed_point_base.reset();
  other.m_interpolation_data_points.reset();
}

template <std::floating_point T>
NetElementFloating<T>& NetElementFloating<T>::operator=(NetElementFloating&& other) {
  if (this == &other)
    return *this;
  NetElement::operator=(std::move(other));
  m_fixed_point_base = std::move(other.m_fixed_point_base);
  other.m_fixed_point_base.reset();
  m_net_version = other.m_net_version;
  m_latest_update_version = other.m_latest_update_version;
  m_value = std::move(other.m_value);
  m_interpolator = std::move(other.m_interpolator);
  m_extrapolation = other.m_extrapolation;
  m_interpolation_data_points = std::move(other.m_interpolation_data_points);
  other.m_interpolation_data_points.reset();
  return *this;
}

template <std::floating_point T>
T NetElementFloating<T>::get() const {
  return m_value;
}

template <std::floating_point T>
void NetElementFloating<T>::set(T value) {
  if (m_value != value) {
    // Only mark the step as updated here if it actually would change the
    // transmitted value.
    if (!m_fixed_point_base || std::round(m_value / *m_fixed_point_base) != std::round(value / *m_fixed_point_base))
      m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;

    m_value = value;

    if (m_interpolation_data_points) {
      m_interpolation_data_points->clear();
      m_interpolation_data_points->push_back({0.0f, m_value});
    }
  }
}

template <std::floating_point T>
void NetElementFloating<T>::set_fixed_point_base(std::optional<T> fixed_point_base) {
  m_fixed_point_base = fixed_point_base;
}

template <std::floating_point T>
void NetElementFloating<T>::set_interpolator(std::function<T(T, T, T)> interpolator) {
  m_interpolator = std::move(interpolator);
}

template <std::floating_point T>
void NetElementFloating<T>::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_net_version = version;
  m_latest_update_version = 0;
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::enable_net_interpolation(float extrapolation_hint) {
  m_extrapolation = extrapolation_hint;
  if (!m_interpolation_data_points) {
    m_interpolation_data_points.emplace();
    m_interpolation_data_points->push_back({0.0f, m_value});
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::disable_net_interpolation() {
  if (m_interpolation_data_points) {
    m_value = m_interpolation_data_points->back().second;
    m_interpolation_data_points.reset();
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::tick_net_interpolation(float dt) {
  if (m_interpolation_data_points) {
    for (auto& p : *m_interpolation_data_points)
      p.first -= dt;

    while (m_interpolation_data_points->size() > 2 && (*m_interpolation_data_points)[1].first <= 0.0f)
      m_interpolation_data_points->pop_front();

    m_value = interpolate();
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::net_store(DataStream& ds, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return {};
  if (m_interpolation_data_points)
    return write_value(ds, m_interpolation_data_points->back().second);
  else
    return write_value(ds, m_value);
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::net_load(DataStream& ds, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  auto result = read_value(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  m_value = *result;
  m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;
  if (m_interpolation_data_points) {
    m_interpolation_data_points->clear();
    m_interpolation_data_points->push_back({0.0f, m_value});
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<bool, StarException> NetElementFloating<T>::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;
  if (m_latest_update_version < from_version)
    return false;

  auto result = m_interpolation_data_points
      ? write_value(ds, m_interpolation_data_points->back().second) : write_value(ds, m_value);
  if (!result) return std::unexpected(std::move(result.error()));

  return true;
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  auto result = read_value(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  T t = *result;

  m_latest_update_version = m_net_version ? m_net_version->get().current() : 0;
  if (m_interpolation_data_points) {
    if (interpolation_time < m_interpolation_data_points->back().first)
      m_interpolation_data_points->clear();
    m_interpolation_data_points->push_back({interpolation_time, t});
    m_value = interpolate();
  } else {
    m_value = t;
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::blank_net_delta(float interpolation_time) {
  if (m_interpolation_data_points) {
    auto last_point = m_interpolation_data_points->back();
    float last_time = last_point.first;
    last_point.first = interpolation_time;
    if (interpolation_time < last_time)
      *m_interpolation_data_points = {last_point};
    else
      m_interpolation_data_points->push_back(last_point);

    m_value = interpolate();
  }
  return {};
}

template <std::floating_point T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::write_value(DataStream& ds, T t) const {
  if (m_fixed_point_base) {
    auto value = std::round(t / *m_fixed_point_base);
    if (!std::isfinite(value) || value < static_cast<T>(std::numeric_limits<std::int64_t>::min())
        || value >= -static_cast<T>(std::numeric_limits<std::int64_t>::min()))
      return std::unexpected(StepStreamException("Fixed point value outside signed VLQ range"));
    return ds.viwrite(static_cast<std::int64_t>(value));
  }
  return ds.write(t);
}

template <std::floating_point T>
[[nodiscard]] std::expected<T, StarException> NetElementFloating<T>::read_value(DataStream& ds) const {
  if (m_fixed_point_base) {
    auto result = ds.read_vlq_i();
    if (!result) return std::unexpected(std::move(result.error()));
    return *result * *m_fixed_point_base;
  }
  return ds.read<T>();
}

template <std::floating_point T>
T NetElementFloating<T>::interpolate() const {
  auto& data_points = *m_interpolation_data_points;

  float ipos = inverse_linear_interpolate_upper(
    data_points.begin(), data_points.end(), 0.0f, [](float lhs, auto const& rhs) { return lhs < rhs.first; },
    [](auto const& data_point) { return data_point.first; });
  auto bound = get_bound2(ipos, data_points.size(), BoundMode::Extrapolate);

  if (m_interpolator) {
    auto const& min_point = data_points[bound.i0];
    auto const& max_point = data_points[bound.i1];

    // If step separation is less than 1.0, don't normalize extrapolation to
    // the very small step difference, because this can result in large jumps
    // during jitter.
    float step_dist = std::max(max_point.first - min_point.first, 1.0f);
    float offset = clamp<float>(bound.offset, 0.0f, 1.0f + m_extrapolation / step_dist);
    return m_interpolator(offset, min_point.second, max_point.second);

  } else {
    if (bound.offset < 1.0f)
      return data_points[bound.i0].second;
    else
      return data_points[bound.i1].second;
  }
}

}

export module star.net_element_float_fields;

export namespace Star {
  using ::Star::StepStreamExceptionTag;
  using ::Star::StepStreamException;
  using ::Star::NetElementFloating;
  using ::Star::NetElementFloat;
  using ::Star::NetElementDouble;
}
