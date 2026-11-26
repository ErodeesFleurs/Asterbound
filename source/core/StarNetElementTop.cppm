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
template <std::derived_from<NetElement> BaseNetElement>
class NetElementTop : public BaseNetElement {
  struct InitializeTag { explicit InitializeTag() = default; };

public:
  NetElementTop() requires std::default_initializable<BaseNetElement>;
  // Children borrow the publisher's clock; moving or copying it breaks that binding.
  NetElementTop(NetElementTop const&) = delete;
  NetElementTop& operator=(NetElementTop const&) = delete;

  template <typename... Params>
    requires (sizeof...(Params) > 0
        && (!std::same_as<std::remove_cvref_t<Params>, NetElementTop> && ...)
        && std::constructible_from<BaseNetElement, Params...>)
  explicit NetElementTop(Params&&... params)
    : BaseNetElement(std::forward<Params>(params)...) {
    BaseNetElement::init_net_version(std::cref(m_net_version));
  }

  // The private tag lets make_shared construct a checked publisher in place.
  template <typename... Params>
    requires requires(Params&&... args) { BaseNetElement(std::forward<Params>(args)...); }
  NetElementTop(InitializeTag, Params&&... args)
    : BaseNetElement(std::forward<Params>(args)...) {
    BaseNetElement::init_net_version(std::cref(m_net_version));
  }

  template <typename... Params>
    requires requires(BaseNetElement& base, Params&&... args) {
      BaseNetElement(std::forward<Params>(args)...);
      { base.initialize() } -> std::same_as<std::expected<void, StarException>>;
    }
  [[nodiscard]] static std::expected<std::shared_ptr<NetElementTop>, StarException> create(Params&&... params) {
    auto element = std::make_shared<NetElementTop>(InitializeTag{}, std::forward<Params>(params)...);
    auto result = element->BaseNetElement::initialize();
    if (!result) return std::unexpected(std::move(result.error()));
    return element;
  }

  // Writes the state update to the given DataStream then returns the version
  // code that should be passed to the next call to write_net_state.  If
  // 'from_version' is 0, then this is a full write for an initial read of a
  // slave NetElementTop.
  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> write_net_state(std::uint64_t from_version = 0, NetCompatibilityRules rules = {});

  // Uses the same full-state wire record as write_net_state(0), without advancing
  // the publisher's version or consuming a delta. The version is observational.
  [[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> snapshot_net_state(NetCompatibilityRules rules = {}) const;
  // Reads a state produced by a call to write_net_state, optionally with the
  // interpolation delay time for the data contained in this state update.  If
  // the state is a full update rather than a delta, the interpolation delay
  // will be ignored.  Blank updates are not necessary to send to be read by
  // read_net_state, *unless* extrapolation is enabled.  If extrapolation is
  // enabled, reading a blank update calls 'blank_net_delta' which is necessary
  // to not improperly extrapolate past the end of incoming deltas.
  [[nodiscard]] std::expected<void, StarException> read_net_state(ByteArray const& data, float interpolation_time = 0.0f, NetCompatibilityRules rules = {});

private:
  using BaseNetElement::init_net_version;
  using BaseNetElement::net_store;
  using BaseNetElement::net_load;
  using BaseNetElement::write_net_delta;
  using BaseNetElement::read_net_delta;
  using BaseNetElement::blank_net_delta;
  using BaseNetElement::check_with_rules;

  NetElementVersion m_net_version;
};

template <std::derived_from<NetElement> BaseNetElement>
NetElementTop<BaseNetElement>::NetElementTop() requires std::default_initializable<BaseNetElement> {
  BaseNetElement::init_net_version(std::cref(m_net_version));
}

template <std::derived_from<NetElement> BaseNetElement>
[[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> NetElementTop<BaseNetElement>::write_net_state(std::uint64_t from_version, NetCompatibilityRules rules) {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  if (from_version == 0) {
    { auto result = ds.write<bool>(true); if (!result) return std::unexpected(std::move(result.error())); }
    { auto result = BaseNetElement::net_store(ds, rules); if (!result) return std::unexpected(std::move(result.error())); }
    return std::pair<ByteArray, std::uint64_t>{ds.take_data(), m_net_version.increment()};
  }
  { auto result = ds.write<bool>(false); if (!result) return std::unexpected(std::move(result.error())); }
  auto written = BaseNetElement::write_net_delta(ds, from_version, rules);
  if (!written) return std::unexpected(std::move(written.error()));
  if (!*written) return std::pair<ByteArray, std::uint64_t>{ByteArray(), m_net_version.current()};
  return std::pair<ByteArray, std::uint64_t>{ds.take_data(), m_net_version.increment()};
}

template <std::derived_from<NetElement> BaseNetElement>
[[nodiscard]] std::expected<std::pair<ByteArray, std::uint64_t>, StarException> NetElementTop<BaseNetElement>::snapshot_net_state(NetCompatibilityRules rules) const {
  DataStreamBuffer ds;
  ds.set_stream_compatibility_version(rules);
  if (auto result = ds.write<bool>(true); !result)
    return std::unexpected(std::move(result.error()));
  if (auto result = BaseNetElement::net_store(ds, rules); !result)
    return std::unexpected(std::move(result.error()));
  return std::pair<ByteArray, std::uint64_t>{ds.take_data(), m_net_version.current()};
}

template <std::derived_from<NetElement> BaseNetElement>
[[nodiscard]] std::expected<void, StarException> NetElementTop<BaseNetElement>::read_net_state(ByteArray const& data, float interpolation_time, NetCompatibilityRules rules) {
  if (data.empty()) return BaseNetElement::blank_net_delta(interpolation_time);
  DataStreamReader ds(data.span());
  ds.set_stream_compatibility_version(rules);
  auto full = ds.read<bool>();
  if (!full) return std::unexpected(std::move(full.error()));
  if (*full) return BaseNetElement::net_load(ds, rules);
  return BaseNetElement::read_net_delta(ds, interpolation_time, rules);
}

}
export module star.net_element_top;

export namespace Star {
  using ::Star::NetElementTop;
}
