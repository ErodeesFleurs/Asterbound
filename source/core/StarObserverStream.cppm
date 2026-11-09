module;
import std;
import star.list;

namespace Star {

// Holds a stream of values which separate observers can query and track
// occurrences in the stream without pulling them from the stream.  Each
// addition to the stream is given an abstract step value, and queries to the
// stream can reference a given step value in order to track events since the
// last query.
template <typename T>
class ObserverStream {
public:
  ObserverStream();
  ObserverStream(std::uint64_t historyLimit);

  // If a history limit is set, then any entries with step values older than
  // the given limit will be discarded automatically.  A historyLimit of 0
  // means that no values will be forgotten.  The step value increases by one
  // with each entry added, or can be increased artificially by a call to
  // tickStep.
  std::uint64_t historyLimit() const;
  void setHistoryLimit(std::uint64_t historyLimit = 0);

  // Add a value to the end of the stream and increment the step value by 1.
  void add(T value);

  // Artificially tick the step by the given delta, which can be used to clear
  // older values.
  void tick(std::uint64_t delta = 1);

  // Query values in the stream since the given step value.  Will return the
  // values in the stream, and a new since value to pass to query on the next
  // call.
  std::pair<List<T>, std::uint64_t> query(std::uint64_t since = 0) const;

  // Resets the step value to 0 and clears all values.
  void reset();

private:
  std::uint64_t m_historyLimit;
  std::uint64_t m_nextStep;
  Deque<std::pair<std::uint64_t, T>> m_values;
};

template <typename T>
ObserverStream<T>::ObserverStream()
  : ObserverStream(0) {}

template <typename T>
ObserverStream<T>::ObserverStream(std::uint64_t historyLimit)
  : m_historyLimit(historyLimit), m_nextStep(0) {}

template <typename T>
std::uint64_t ObserverStream<T>::historyLimit() const {
  return m_historyLimit;
}

template <typename T>
void ObserverStream<T>::setHistoryLimit(std::uint64_t historyLimit) {
  m_historyLimit = historyLimit;
  tick(0);
}

template <typename T>
void ObserverStream<T>::add(T value) {
  m_values.push_back({m_nextStep, std::move(value)});
  tick(1);
}

template <typename T>
void ObserverStream<T>::tick(std::uint64_t delta) {
  m_nextStep += delta;
  std::uint64_t removeBefore = m_nextStep - std::min(m_nextStep, m_historyLimit);
  while (!m_values.empty() && m_values.front().first < removeBefore)
    m_values.pop_front();
}

template <typename T>
std::pair<List<T>, std::uint64_t> ObserverStream<T>::query(std::uint64_t since) const {
  List<T> res;
  auto i = std::lower_bound(m_values.begin(),
      m_values.end(),
      since,
      [](std::pair<std::uint64_t, T> const& p, std::uint64_t step) { return p.first < step; });
  while (i != m_values.end()) {
    res.push_back(i->second);
    ++i;
  }
  return {res, m_nextStep};
}

template <typename T>
void ObserverStream<T>::reset() {
  m_nextStep = 0;
  m_values.clear();
}

}

export module star.observer_stream;

export namespace Star {
  using ::Star::ObserverStream;
}
