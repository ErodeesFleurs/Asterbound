module;

import std;
import star.format;
import star.string;
import star.json;
import star.logging;
import star.thread;

namespace Star {

// Root owns this service and must keep its Logger alive through destruction.
// The mutex serializes all state changes and complete disk commits, making
// flush a barrier for every change accepted before it acquires the mutex.
class ModStorage {
public:
  static constexpr std::size_t max_mod_data_size = 4 * 1024 * 1024;
  static constexpr auto write_delay = std::chrono::milliseconds(2000);

  [[nodiscard]] static std::expected<std::unique_ptr<ModStorage>, StarException> create(Logger& logger, String directory);
  ~ModStorage();

  ModStorage(ModStorage const&) = delete;
  ModStorage& operator=(ModStorage const&) = delete;

  [[nodiscard]] std::expected<Json, StarException> get(String const& mod, String const& key);
  [[nodiscard]] std::expected<void, StarException> set(String const& mod, String const& key, Json const& value);
  [[nodiscard]] std::expected<JsonArray, StarException> keys(String const& mod);
  [[nodiscard]] std::expected<void, StarException> clear(String const& mod);
  [[nodiscard]] std::expected<void, StarException> flush();

private:
  struct Entry {
    Json value;
    std::string key_json;
    std::string value_json;
    std::size_t size() const { return key_json.size() + 1 + value_json.size(); }
  };
  struct ModData {
    std::unordered_map<String, Entry, ValueHash<String>> entries;
    String filename;
    // Complete compact object size, including braces, colons and commas.
    std::size_t bytes = 2;
    bool dirty = false;
  };

  ModStorage(Logger& logger, String directory);
  [[nodiscard]] std::expected<std::reference_wrapper<ModData>, StarException> load(String const& mod);
  [[nodiscard]] static std::expected<void, StarException> insert_new_entry(
    ModData& data, String const& key, Json const& value);
  void mark_dirty(ModData& data);
  void set_write_deadline(std::optional<std::chrono::steady_clock::time_point> deadline);
  [[nodiscard]] std::expected<void, StarException> flush_locked();
  [[nodiscard]] std::expected<void, StarException> run();

  Logger& m_logger;
  String m_directory;
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::unordered_map<String, ModData, ValueHash<String>> m_mods;
  // Engaged iff at least one loaded namespace is dirty.
  std::optional<std::chrono::steady_clock::time_point> m_next_write;
  bool m_stop = false;
  // Declared last so joining always precedes destruction of borrowed state.
  ThreadFunction<void> m_thread;
};

}

export module star.mod_storage;

export namespace Star {
  using ::Star::ModStorage;
}
