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
  static constexpr std::size_t MaxModDataSize = 4 * 1024 * 1024;
  static constexpr unsigned WriteDelayMillis = 2000;

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
    std::string keyJson;
    std::string valueJson;
    std::size_t size() const { return keyJson.size() + 1 + valueJson.size(); }
  };
  struct ModData {
    std::unordered_map<String, Entry, hash<String>> entries;
    String filename;
    // Complete compact object size, including braces, colons and commas.
    std::size_t bytes = 2;
    bool dirty = false;
  };

  ModStorage(Logger& logger, String directory);
  [[nodiscard]] std::expected<std::reference_wrapper<ModData>, StarException> load(String const& mod);
  void markDirty(ModData& data);
  [[nodiscard]] std::expected<void, StarException> flushLocked();
  [[nodiscard]] std::expected<void, StarException> run();

  Logger& m_logger;
  String m_directory;
  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::unordered_map<String, ModData, hash<String>> m_mods;
  std::optional<std::chrono::steady_clock::time_point> m_nextWrite;
  bool m_stop = false;
  // Declared last so joining always precedes destruction of borrowed state.
  ThreadFunction<void> m_thread;
};

}

export module star.mod_storage;

export namespace Star {
  using ::Star::ModStorage;
}
