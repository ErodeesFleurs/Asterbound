module;
import std;
import star.algorithm;
import star.format;
import star.json;
import star.map;
import star.rpc_promise;

namespace Star {

template <typename Key>
class EntityMessagePromiseQueue {
public:
  bool empty() const { return m_entries.empty(); }

  void insert_or_assign(Key const& key, RpcPromise<Json> promise) {
    auto generation = ++m_generation;
    RpcPromise<Json> retired;
    auto [where, inserted] = m_entries.try_emplace(key, generation, std::move(promise));
    if (!inserted) {
      retired = std::exchange(where->second.promise, std::move(promise));
      where->second.generation = generation;
    }
  }

  template <typename Predicate>
  void discard_if(Predicate&& matches) {
    // Release captures after detaching all matching registrations.
    std::vector<RpcPromise<Json>> retired;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
      if (matches(it->first)) {
        retired.push_back(std::move(it->second.promise));
        it = m_entries.erase(it);
      } else {
        ++it;
      }
    }
  }

  void clear() {
    // A callback may clear the queue mid-drain, but cannot reset its scratch
    // or allow a newly registered key to match an old generation.
    auto retired = std::exchange(m_entries, {});
  }

  template <typename Deliver>
  [[nodiscard]] std::expected<void, StarException> drain(Deliver&& deliver) {
    // A nested update leaves both the current snapshot and new work alone.
    if (m_draining || m_entries.empty())
      return {};
    m_draining = true;
    auto done = make_scope_exit([this] {
      m_snapshot.clear();
      m_draining = false;
    });
    for (auto const& [key, entry] : m_entries)
      m_snapshot.emplace_back(key, entry.generation);

    for (auto const& [key, generation] : m_snapshot) {
      RpcPromise<Json> promise;
      {
        auto it = m_entries.find(key);
        if (it == m_entries.end() || it->second.generation != generation)
          continue;
        promise = it->second.promise;
      }
      // Progress can replace this registration or clear the entire world.
      auto observed = promise.poll_result();
      if (observed && !*observed)
        continue;
      {
        auto it = m_entries.find(key);
        if (it == m_entries.end() || it->second.generation != generation)
          continue;
        if (!observed)
          return std::unexpected(std::move(observed.error()));
        m_entries.erase(it);
      }
      deliver(key, **std::move(observed));
    }
    return {};
  }

private:
  struct Entry {
    std::uint64_t generation;
    RpcPromise<Json> promise;
  };

  HashMap<Key, Entry> m_entries;
  std::vector<std::pair<Key, std::uint64_t>> m_snapshot;
  std::uint64_t m_generation = 0;
  bool m_draining = false;
};

}

export module star.entity_message_promise_queue;

export namespace Star {
using ::Star::EntityMessagePromiseQueue;
}
