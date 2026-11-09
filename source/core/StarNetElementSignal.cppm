module;
import std;
import star.format;
import star.net_compatibility;
import star.list;
import star.data_stream;

import star.net_element;

import star.algorithm;
namespace Star {

// NetElement that sends signals during delta writes that can be received by
// slaves.  It has no 'state', and nothing is sent during a store / load, and
// it only keeps past signals for a maximum number of versions.  Thus, it is
// not appropriate to use to send updates to long term states, only for event
// like things that are not harmful if missed.
template <typename Signal>
class NetElementSignal : public NetElement {
public:
  NetElementSignal(std::size_t maxSignalQueue = 32);

  void initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> netStore(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> netLoad(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> enableNetInterpolation(float extrapolationHint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disableNetInterpolation() override;
  [[nodiscard]] std::expected<void, StarException> tickNetInterpolation(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> readNetDelta(DataStream& ds, float interpolationTime = 0.0f, NetCompatibilityRules rules = {}) override;

  void send(Signal signal);
  List<Signal> receive();

private:
  struct SignalEntry {
    std::uint64_t version;
    Signal signal;
    bool received;
  };

  std::size_t m_maxSignalQueue;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_netVersion = std::nullopt;
  bool m_netInterpolationEnabled = false;
  Deque<SignalEntry> m_signals;
  Deque<std::pair<float, Signal>> m_pendingSignals;
};

template <typename Signal>
NetElementSignal<Signal>::NetElementSignal(std::size_t maxSignalQueue) {
  m_maxSignalQueue = maxSignalQueue;
}

template <typename Signal>
void NetElementSignal<Signal>::initNetVersion(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_netVersion = version;
  m_signals.clear();
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::netStore(DataStream&, NetCompatibilityRules) const { return {}; }

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::netLoad(DataStream&, NetCompatibilityRules) {
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::enableNetInterpolation(float) {
  m_netInterpolationEnabled = true;
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::disableNetInterpolation() {
  m_netInterpolationEnabled = false;
  for (auto& p : std::exchange(m_pendingSignals, {}))
    send(std::move(p.second));
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::tickNetInterpolation(float dt) {
  for (auto& p : m_pendingSignals)
    p.first -= dt;

  while (!m_pendingSignals.empty() && m_pendingSignals.front().first <= 0.0f) {
    send(std::move(m_pendingSignals.front().second));
    m_pendingSignals.pop_front();
  }
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<bool, StarException> NetElementSignal<Signal>::writeNetDelta(DataStream& ds, std::uint64_t fromVersion, NetCompatibilityRules rules) const {
  if (!checkWithRules(rules)) return false;
  std::size_t numToWrite = 0;
  for (auto const& p : m_signals) {
    if (p.version >= fromVersion)
      ++numToWrite;
  }
  if (numToWrite == 0)
    return false;

  {
    auto result = ds.write_vlq_u(numToWrite);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  for (auto const& p : m_signals) {
    if (p.version >= fromVersion)
      { auto result = ds.write(p.signal); if (!result) return std::unexpected(std::move(result.error())); }
  }

  return true;
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::readNetDelta(DataStream& ds, float interpolationTime, NetCompatibilityRules rules) {
  if (!checkWithRules(rules)) return {};
  auto count = ds.read_vlq_u();
  if (!count) return std::unexpected(std::move(count.error()));
  std::size_t numToRead = *count;
  for (std::size_t i = 0; i < numToRead; ++i) {
    Signal s;
    { auto result = ds.read(s); if (!result) return std::unexpected(std::move(result.error())); }
    if (m_netInterpolationEnabled && interpolationTime > 0.0f) {
      if (!m_pendingSignals.empty() && m_pendingSignals.back().first > interpolationTime) {
        for (auto& p : std::exchange(m_pendingSignals, {}))
          send(std::move(p.second));
      }
      m_pendingSignals.push_back({interpolationTime, std::move(s)});
    } else {
      send(std::move(s));
    }
  }
  return {};
}

template <typename Signal>
void NetElementSignal<Signal>::send(Signal signal) {
  m_signals.push_back({m_netVersion ? m_netVersion->get().current() : 0, signal, false});
  while (m_signals.size() > m_maxSignalQueue)
    m_signals.pop_front();
}

template <typename Signal>
List<Signal> NetElementSignal<Signal>::receive() {
  List<Signal> received;
  for (auto& p : m_signals) {
    if (!p.received) {
      received.push_back(p.signal);
      p.received = true;
    }
  }
  return received;
}

}

export module star.net_element_signal;

export namespace Star {
  using ::Star::NetElementSignal;
}
