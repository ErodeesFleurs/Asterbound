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

template <typename T>
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
  void setFixedPointBase(std::optional<T> fixedPointBase = {});

  // If interpolation is enabled on the NetStepStates parent, and an
  // interpolator is set, then on steps in between data points this will be
  // used to interpolate this value.  It is not necessary that senders and
  // receivers both have matching interpolation functions, or any interpolation
  // functions at all.
  void setInterpolator(std::function<T(T, T, T)> interpolator);

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
  [[nodiscard]] std::expected<void, StarException> writeValue(DataStream& ds, T t) const;
  [[nodiscard]] std::expected<T, StarException> readValue(DataStream& ds) const;

  T interpolate() const;

  std::optional<T> m_fixedPointBase;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion = std::nullopt;
  std::uint64_t m_latestUpdateVersion = 0;
  T m_value = T();

  std::function<T(T, T, T)> m_interpolator;
  float m_extrapolation = 0.0f;
  std::optional<Deque<std::pair<float, T>>> m_interpolationDataPoints;
};

using NetElementFloat = NetElementFloating<float>;
using NetElementDouble = NetElementFloating<double>;

template <typename T>
NetElementFloating<T>::NetElementFloating(NetElementFloating&& other)
  : NetElement(std::move(other)),
    m_fixedPointBase(std::move(other.m_fixedPointBase)),
    m_netVersion(other.m_netVersion),
    m_latestUpdateVersion(other.m_latestUpdateVersion),
    m_value(std::move(other.m_value)),
    m_interpolator(std::move(other.m_interpolator)),
    m_extrapolation(other.m_extrapolation),
    m_interpolationDataPoints(std::move(other.m_interpolationDataPoints)) {
  // A moved-from field must no longer advertise fixed-point or interpolation state.
  other.m_fixedPointBase.reset();
  other.m_interpolationDataPoints.reset();
}

template <typename T>
NetElementFloating<T>& NetElementFloating<T>::operator=(NetElementFloating&& other) {
  if (this == &other)
    return *this;
  NetElement::operator=(std::move(other));
  m_fixedPointBase = std::move(other.m_fixedPointBase);
  other.m_fixedPointBase.reset();
  m_netVersion = other.m_netVersion;
  m_latestUpdateVersion = other.m_latestUpdateVersion;
  m_value = std::move(other.m_value);
  m_interpolator = std::move(other.m_interpolator);
  m_extrapolation = other.m_extrapolation;
  m_interpolationDataPoints = std::move(other.m_interpolationDataPoints);
  other.m_interpolationDataPoints.reset();
  return *this;
}

template <typename T>
T NetElementFloating<T>::get() const {
  return m_value;
}

template <typename T>
void NetElementFloating<T>::set(T value) {
  if (m_value != value) {
    // Only mark the step as updated here if it actually would change the
    // transmitted value.
    if (!m_fixedPointBase || std::round(m_value / *m_fixedPointBase) != std::round(value / *m_fixedPointBase))
      m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;

    m_value = value;

    if (m_interpolationDataPoints) {
      m_interpolationDataPoints->clear();
      m_interpolationDataPoints->push_back({0.0f, m_value});
    }
  }
}

template <typename T>
void NetElementFloating<T>::setFixedPointBase(std::optional<T> fixedPointBase) {
  m_fixedPointBase = fixedPointBase;
}

template <typename T>
void NetElementFloating<T>::setInterpolator(std::function<T(T, T, T)> interpolator) {
  m_interpolator = std::move(interpolator);
}

template <typename T>
void NetElementFloating<T>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_latestUpdateVersion = 0;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::enableNetInterpolation(float extrapolationHint) {
  m_extrapolation = extrapolationHint;
  if (!m_interpolationDataPoints) {
    m_interpolationDataPoints.emplace();
    m_interpolationDataPoints->push_back({0.0f, m_value});
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::disableNetInterpolation() {
  if (m_interpolationDataPoints) {
    m_value = m_interpolationDataPoints->back().second;
    m_interpolationDataPoints.reset();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::tickNetInterpolation(float dt) {
  if (m_interpolationDataPoints) {
    for (auto& p : *m_interpolationDataPoints)
      p.first -= dt;

    while (m_interpolationDataPoints->size() > 2 && (*m_interpolationDataPoints)[1].first <= 0.0f)
      m_interpolationDataPoints->pop_front();

    m_value = interpolate();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return {};
  if (m_interpolationDataPoints)
    return writeValue(ds, m_interpolationDataPoints->back().second);
  else
    return writeValue(ds, m_value);
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  auto result = readValue(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  m_value = *result;
  m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if (m_interpolationDataPoints) {
    m_interpolationDataPoints->clear();
    m_interpolationDataPoints->push_back({0.0f, m_value});
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<bool, StarException> NetElementFloating<T>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  if (m_latestUpdateVersion < fromVersion)
    return false;

  auto result = m_interpolationDataPoints
      ? writeValue(ds, m_interpolationDataPoints->back().second) : writeValue(ds, m_value);
  if (!result) return std::unexpected(std::move(result.error()));

  return true;
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  ((void)(rules));
  auto result = readValue(ds);
  if (!result) return std::unexpected(std::move(result.error()));
  T t = *result;

  m_latestUpdateVersion = m_netVersion ? m_netVersion->get().current() : 0;
  if (m_interpolationDataPoints) {
    if (interpolationTime < m_interpolationDataPoints->back().first)
      m_interpolationDataPoints->clear();
    m_interpolationDataPoints->push_back({interpolationTime, t});
    m_value = interpolate();
  } else {
    m_value = t;
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::blankNetDelta(float interpolationTime) {
  if (m_interpolationDataPoints) {
    auto lastPoint = m_interpolationDataPoints->back();
    float lastTime = lastPoint.first;
    lastPoint.first = interpolationTime;
    if (interpolationTime < lastTime)
      *m_interpolationDataPoints = {lastPoint};
    else
      m_interpolationDataPoints->push_back(lastPoint);

    m_value = interpolate();
  }
  return {};
}

template <typename T>
[[nodiscard]] std::expected<void, StarException> NetElementFloating<T>::writeValue(DataStream& ds, T t) const {
  if (m_fixedPointBase) {
    auto value = std::round(t / *m_fixedPointBase);
    if (!std::isfinite(value) || value < static_cast<T>(std::numeric_limits<std::int64_t>::min())
        || value >= -static_cast<T>(std::numeric_limits<std::int64_t>::min()))
      return std::unexpected(StepStreamException("Fixed point value outside signed VLQ range"));
    return ds.viwrite(static_cast<std::int64_t>(value));
  }
  return ds.write(t);
}

template <typename T>
[[nodiscard]] std::expected<T, StarException> NetElementFloating<T>::readValue(DataStream& ds) const {
  if (m_fixedPointBase) {
    auto result = ds.readVlqI();
    if (!result) return std::unexpected(std::move(result.error()));
    return *result * *m_fixedPointBase;
  }
  return ds.read<T>();
}

template <typename T>
T NetElementFloating<T>::interpolate() const {
  auto& dataPoints = *m_interpolationDataPoints;

  float ipos = inverseLinearInterpolateUpper(dataPoints.begin(), dataPoints.end(), 0.0f,
      [](float lhs, auto const& rhs) {
        return lhs < rhs.first;
      }, [](auto const& dataPoint) {
        return dataPoint.first;
      });
  auto bound = getBound2(ipos, dataPoints.size(), BoundMode::Extrapolate);

  if (m_interpolator) {
    auto const& minPoint = dataPoints[bound.i0];
    auto const& maxPoint = dataPoints[bound.i1];

    // If step separation is less than 1.0, don't normalize extrapolation to
    // the very small step difference, because this can result in large jumps
    // during jitter.
    float stepDist = std::max(maxPoint.first - minPoint.first, 1.0f);
    float offset = clamp<float>(bound.offset, 0.0f, 1.0f + m_extrapolation / stepDist);
    return m_interpolator(offset, minPoint.second, maxPoint.second);

  } else {
    if (bound.offset < 1.0f)
      return dataPoints[bound.i0].second;
    else
      return dataPoints[bound.i1].second;
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
