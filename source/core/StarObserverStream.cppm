module;
import std;
import star.format;
import star.list;

namespace Star {

// Independent observers track retained events with a cursor, without consuming them.
template <typename T>
  requires std::is_object_v<T>
class ObserverStream {
  using Entry = std::pair<std::uint64_t, T>;

public:
  // History is measured in steps, not event count. A zero window retains nothing.
  explicit ObserverStream(std::uint64_t history_limit = 0) : m_history_limit(history_limit) {}

  std::uint64_t history_limit() const { return m_history_limit; }

  void set_history_limit(std::uint64_t history_limit = 0) {
    m_history_limit = history_limit;
    prune();
  }

  void push_back(T const& value) requires std::constructible_from<T, T const&> {
    append(value);
  }

  void push_back(T&& value) requires std::constructible_from<T, T&&> {
    append(std::move(value));
  }

  void tick(std::uint64_t delta = 1) {
    m_next_step = advanced_step(delta);
    prune();
  }

  // Return an independent snapshot together with the cursor for the next query.
  [[nodiscard]] std::pair<List<T>, std::uint64_t> query(std::uint64_t since = 0) const
    requires std::constructible_from<T, T const&>
  {
    auto entries = entries_since(since);
    List<T> result;
    result.reserve(entries.size());
    for (auto const& entry : entries)
      result.emplace_back(entry.second);
    return {std::move(result), m_next_step};
  }

  // Synchronously borrow each event. The callback must not mutate this stream
  // or retain references to its events.
  template <typename U, typename Transform>
    requires (std::constructible_from<U, U&&> || std::constructible_from<U, U const&>)
      && requires(Transform& transform, T const& event) {
        { std::invoke(transform, event) } -> std::same_as<std::expected<U, StarException>>;
      }
  [[nodiscard]] std::expected<std::pair<List<U>, std::uint64_t>, StarException>
  transform_since(std::uint64_t since, Transform&& transform) const {
    auto entries = entries_since(since);
    List<U> result;
    result.reserve(entries.size());
    for (auto const& entry : entries) {
      auto projected = std::invoke(transform, entry.second);
      if (!projected)
        return std::unexpected(std::move(projected).error());
      if constexpr (std::constructible_from<U, U&&>)
        result.emplace_back(*std::move(projected));
      else
        result.emplace_back(std::as_const(*projected));
    }
    return std::expected<std::pair<List<U>, std::uint64_t>, StarException>(
        std::in_place, std::move(result), m_next_step);
  }

  void reset() {
    m_next_step = 0;
    m_values.clear();
  }

private:
  std::uint64_t advanced_step(std::uint64_t delta) const {
    if (delta > std::numeric_limits<std::uint64_t>::max() - m_next_step)
      Star::fatal_error("ObserverStream cursor exhausted", false);
    return m_next_step + delta;
  }

  template <typename Value>
    requires std::constructible_from<T, Value&&>
  void append(Value&& value) {
    auto next_step = advanced_step(1);
    if (m_history_limit == 0) {
      m_next_step = next_step;
      return;
    }
    m_values.emplace_back(m_next_step, std::forward<Value>(value));
    m_next_step = next_step;
    prune();
  }

  void prune() {
    auto oldest = m_next_step - std::min(m_next_step, m_history_limit);
    while (!m_values.empty() && m_values.front().first < oldest)
      m_values.pop_front();
  }

  auto entries_since(std::uint64_t since) const {
    auto end = m_values.end();
    auto first = end;
    if (since < m_next_step && !m_values.empty()) {
      first = since <= m_values.front().first ? m_values.begin()
          : std::ranges::lower_bound(m_values, since, {}, &Entry::first);
    }
    return std::ranges::subrange(first, end);
  }

  std::uint64_t m_history_limit;
  std::uint64_t m_next_step = 0;
  Deque<Entry> m_values;
};

}

export module star.observer_stream;

export namespace Star {
  using ::Star::ObserverStream;
}
