module;
import std;
import star.format;
import star.net_compatibility;
import star.byte_array;

import star.data_stream_devices;

import star.net_element;

namespace Star {

// Mixin for the NetElement that should be the top element for a network, wraps
// any NetElement class and manages the NetElementVersion.
template <typename BaseNetElement>
class NetElementTop : public BaseNetElement {
public:
  NetElementTop() requires std::default_initializable<BaseNetElement>;

  template <typename... Params>
    requires (sizeof...(Params) > 0
        && std::constructible_from<BaseNetElement, Params...>
        && (!std::same_as<std::remove_cvref_t<Params>, NetElementTop> && ...))
  explicit NetElementTop(Params&&... params)
    : BaseNetElement(std::forward<Params>(params)...) {
    BaseNetElement::initNetVersion(std::cref(m_netVersion));
  }

  template <typename... Params>
  [[nodiscard]] static std::expected<std::shared_ptr<NetElementTop>, StarException> create(Params&&... params) {
    auto element = std::shared_ptr<NetElementTop>(new NetElementTop(InitializeTag{}, std::forward<Params>(params)...));
    auto result = element->BaseNetElement::initialize();
    if (!result) return std::unexpected(std::move(result.error()));
    return element;
  }

  // Writes the state update to the given DataStream then returns the version
  // code that should be passed to the next call to writeState.  If
  // 'fromVersion' is 0, then this is a full write for an initial read of a
  // slave NetElementTop.
  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> writeNetState(std::uint64_t fromVersion = 0, NetCompatibilityRules rules = {});
  // Reads a state produced by a call to writeState, optionally with the
  // interpolation delay time for the data contained in this state update.  If
  // the state is a full update rather than a delta, the interoplation delay
  // will be ignored.  Blank updates are not necessary to send to be read by
  // readState, *unless* extrapolation is enabled.  If extrapolation is
  // enabled, reading a blank update calls 'blankNetDelta' which is necessary
  // to not improperly extrapolate past the end of incoming deltas.
  [[nodiscard]] std::expected<void, StarException> readNetState(ByteArray data, float interpolationTime = 0.0f, NetCompatibilityRules rules = {});

private:
  struct InitializeTag {};

  template <typename... Params>
  NetElementTop(InitializeTag, Params&&... params)
    : BaseNetElement(std::forward<Params>(params)...) {
    BaseNetElement::initNetVersion(std::cref(m_netVersion));
  }

  using BaseNetElement::initNetVersion;
  using BaseNetElement::netStore;
  using BaseNetElement::netLoad;
  using BaseNetElement::writeNetDelta;
  using BaseNetElement::readNetDelta;
  using BaseNetElement::blankNetDelta;
  using BaseNetElement::checkWithRules;

  NetElementVersion m_netVersion;
};

template <typename BaseNetElement>
NetElementTop<BaseNetElement>::NetElementTop() requires std::default_initializable<BaseNetElement> {
  BaseNetElement::initNetVersion(std::cref(m_netVersion));
}

template <typename BaseNetElement>
[[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> NetElementTop<BaseNetElement>::writeNetState(std::uint64_t fromVersion, NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  if (fromVersion == 0) {
    { auto result = ds.write<bool>(true); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = BaseNetElement::netStore(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    return std::pair<ByteArray, std::uint64_t>{ds.take_data(), m_netVersion.increment()};
  }
  { auto result = ds.write<bool>(false); if (!result) return std::unexpected(std::move(result.error())); }
  auto written = BaseNetElement::writeNetDelta(ds, fromVersion, rules);
  if (!written) return std::unexpected(std::move(written.error()));
  if (!*written) return std::pair<ByteArray, std::uint64_t>{ByteArray(), m_netVersion.current()};
  return std::pair<ByteArray, std::uint64_t>{ds.take_data(), m_netVersion.increment()};
}

template <typename BaseNetElement>
[[nodiscard]] std::expected<void, StarException> NetElementTop<BaseNetElement>::readNetState(ByteArray data, float interpolationTime, NetCompatibilityRules rules) {
  if (data.empty()) return BaseNetElement::blankNetDelta(interpolationTime);
  DataStreamBuffer ds(std::move(data));
  ds.set_stream_compatibility_version(rules);
  auto full = ds.read<bool>();
  if (!full) return std::unexpected(std::move(full.error()));
  if (*full) return BaseNetElement::netLoad(ds, rules);
  return BaseNetElement::readNetDelta(ds, interpolationTime, rules);
}

}
export module star.net_element_top;

export namespace Star {
  using ::Star::NetElementTop;
}
