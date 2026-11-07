module;
import std;
import star.format;
import star.net_compatibility;
import star.data_stream;

import star.net_element;

namespace Star {

template <typename BaseNetElement>
class NetElementOverride : public BaseNetElement {
public:
  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  using NetStorer = std::function<std::expected<void, StarException>(DataStream&, NetCompatibilityRules)>;
  using NetLoader = std::function<std::expected<void, StarException>(DataStream&, NetCompatibilityRules)>;
  using NetDeltaWriter =
    std::function<std::expected<bool, StarException>(DataStream&, std::uint64_t, NetCompatibilityRules)>;
  using NetDeltaReader = std::function<std::expected<void, StarException>(DataStream&, float, NetCompatibilityRules)>;

  void setNetStorer(NetStorer);
  void setNetLoader(NetLoader);
  void setNetDeltaWriter(NetDeltaWriter);
  void setNetDeltaReader(NetDeltaReader);
  void setOverrides(NetStorer netStorer, NetLoader netLoader,
    NetDeltaWriter netDeltaWriter, NetDeltaReader netDeltaReader);

private:
  NetStorer m_netStorer;
  NetLoader m_netLoader;
  
  NetDeltaWriter m_netDeltaWriter;
  NetDeltaReader m_netDeltaReader;
};

template <typename BaseNetElement>
[[nodiscard]] std::expected<void, StarException> NetElementOverride<BaseNetElement>::netStore(DataStream& ds, NetCompatibilityRules rules) const {
  if (m_netStorer)
    return m_netStorer(ds, rules);
  else
    return BaseNetElement::netStore(ds, rules);
}

template <typename BaseNetElement>
[[nodiscard]] std::expected<void, StarException> NetElementOverride<BaseNetElement>::netLoad(DataStream& ds, NetCompatibilityRules rules) {
  if (m_netLoader)
    return m_netLoader(ds, rules);
  else
    return BaseNetElement::netLoad(ds, rules);
}

template <typename BaseNetElement>
[[nodiscard]] std::expected<bool, StarException> NetElementOverride<BaseNetElement>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (m_netDeltaWriter)
    return m_netDeltaWriter(ds, fromVersion, rules);
  else
    return BaseNetElement::writeNetDelta(ds, fromVersion, rules);
}

template <typename BaseNetElement>
[[nodiscard]] std::expected<void, StarException> NetElementOverride<BaseNetElement>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (m_netDeltaReader)
    return m_netDeltaReader(ds, interpolationTime, rules);
  else
    return BaseNetElement::readNetDelta(ds, interpolationTime, rules);
}

template <typename BaseNetElement>
inline void NetElementOverride<BaseNetElement>::setNetStorer(NetStorer f) { m_netStorer = std::move(f); }
template <typename BaseNetElement>
inline void NetElementOverride<BaseNetElement>::setNetLoader(NetLoader f) { m_netLoader = std::move(f); }
template <typename BaseNetElement>
inline void NetElementOverride<BaseNetElement>::setNetDeltaWriter(NetDeltaWriter f) { m_netDeltaWriter = std::move(f); }
template <typename BaseNetElement>
inline void NetElementOverride<BaseNetElement>::setNetDeltaReader(NetDeltaReader f) { m_netDeltaReader = std::move(f); }

template <typename BaseNetElement>
inline void NetElementOverride<BaseNetElement>::setOverrides(NetStorer netStorer, NetLoader netLoader, NetDeltaWriter netDeltaWriter, NetDeltaReader netDeltaReader) {
  m_netStorer = std::move(netStorer);
  m_netLoader = std::move(netLoader);
  m_netDeltaWriter = std::move(netDeltaWriter);
  m_netDeltaReader = std::move(netDeltaReader);
}

}
export module star.net_element_ext;

export namespace Star {
  using ::Star::NetElementOverride;
}
