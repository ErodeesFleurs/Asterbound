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
  NetElementSignal(std::size_t max_signal_queue = 32);

  void init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version = std::nullopt) override;

  [[nodiscard]] std::expected<void, StarException> net_store(DataStream& ds, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> net_load(DataStream& ds, NetCompatibilityRules rules) override;

  [[nodiscard]] std::expected<void, StarException> enable_net_interpolation(float extrapolation_hint = 0.0f) override;
  [[nodiscard]] std::expected<void, StarException> disable_net_interpolation() override;
  [[nodiscard]] std::expected<void, StarException> tick_net_interpolation(float dt) override;

  [[nodiscard]] std::expected<bool, StarException> write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules = {}) const override;
  [[nodiscard]] std::expected<void, StarException> read_net_delta(DataStream& ds, float interpolation_time = 0.0f, NetCompatibilityRules rules = {}) override;

  void send(Signal signal);
  List<Signal> receive();

private:
  struct SignalEntry {
    std::uint64_t version;
    Signal signal;
    bool received;
  };

  std::size_t m_max_signal_queue;
  std::optional<std::reference_wrapper<NetElementVersion const>> m_net_version = std::nullopt;
  bool m_net_interpolation_enabled = false;
  Deque<SignalEntry> m_signals;
  Deque<std::pair<float, Signal>> m_pending_signals;
};

template <typename Signal>
NetElementSignal<Signal>::NetElementSignal(std::size_t max_signal_queue) {
  m_max_signal_queue = max_signal_queue;
}

template <typename Signal>
void NetElementSignal<Signal>::init_net_version(std::optional<std::reference_wrapper<NetElementVersion const>> version) {
  m_net_version = version;
  m_signals.clear();
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::net_store(DataStream&, NetCompatibilityRules) const { return {}; }

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::net_load(DataStream&, NetCompatibilityRules) {
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::enable_net_interpolation(float) {
  m_net_interpolation_enabled = true;
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::disable_net_interpolation() {
  m_net_interpolation_enabled = false;
  for (auto& p : std::exchange(m_pending_signals, {}))
    send(std::move(p.second));
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::tick_net_interpolation(float dt) {
  for (auto& p : m_pending_signals)
    p.first -= dt;

  while (!m_pending_signals.empty() && m_pending_signals.front().first <= 0.0f) {
    send(std::move(m_pending_signals.front().second));
    m_pending_signals.pop_front();
  }
  return {};
}

template <typename Signal>
[[nodiscard]] std::expected<bool, StarException> NetElementSignal<Signal>::write_net_delta(DataStream& ds, std::uint64_t from_version, NetCompatibilityRules rules) const {
  if (!check_with_rules(rules)) return false;
  std::size_t num_to_write = 0;
  for (auto const& p : m_signals) {
    if (p.version >= from_version)
      ++num_to_write;
  }
  if (num_to_write == 0)
    return false;

  {
    auto result = ds.write_vlq_u(num_to_write);
    if (!result)
      return std::unexpected(std::move(result.error()));
  }

  for (auto const& p : m_signals) {
    if (p.version >= from_version)
      { auto result = ds.write(p.signal); if (!result) return std::unexpected(std::move(result.error())); }
  }

  return true;
}

template <typename Signal>
[[nodiscard]] std::expected<void, StarException> NetElementSignal<Signal>::read_net_delta(DataStream& ds, float interpolation_time, NetCompatibilityRules rules) {
  if (!check_with_rules(rules)) return {};
  auto count = ds.read_vlq_u();
  if (!count) return std::unexpected(std::move(count.error()));
  std::size_t num_to_read = *count;
  for (std::size_t i = 0; i < num_to_read; ++i) {
    Signal s;
    { auto result = ds.read(s); if (!result) return std::unexpected(std::move(result.error())); }
    if (m_net_interpolation_enabled && interpolation_time > 0.0f) {
      if (!m_pending_signals.empty() && m_pending_signals.back().first > interpolation_time) {
        for (auto& p : std::exchange(m_pending_signals, {}))
          send(std::move(p.second));
      }
      m_pending_signals.push_back({interpolation_time, std::move(s)});
    } else {
      send(std::move(s));
    }
  }
  return {};
}

template <typename Signal>
void NetElementSignal<Signal>::send(Signal signal) {
  m_signals.emplace_back(m_net_version ? m_net_version->get().current() : 0, std::move(signal), false);
  while (m_signals.size() > m_max_signal_queue)
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
