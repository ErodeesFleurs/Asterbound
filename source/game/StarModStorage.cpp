import std;
import star.mod_storage;
import star.format;
import star.string;
import star.json;
import star.json_parser;
import star.json_builder;
import star.file;
import star.io_device;
import star.sha256;
import star.encode;
import star.logging;
import star.thread;

namespace Star {
namespace {

// Includes the enclosing, top-level namespace object. Bound recursive engine
// parsers/streamers before descending, not after allocating their full output.
constexpr unsigned MaxStorageDepth = 256;

StarException sizeError() {
  return StarException("Mod storage exceeds the 4 MiB compact JSON limit");
}

std::expected<void, StarException> checkDouble(std::string_view token) {
  double value = 0;
  auto result = std::from_chars(token.data(), token.data() + token.size(), value);
  auto finite = (std::bit_cast<std::uint64_t>(value) & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || !finite)
    return std::unexpected(StarException("Mod storage requires finite, representable JSON numbers"));
  return {};
}

class StorageOutput {
public:
  StorageOutput(std::string& text, std::size_t limit) : m_text(text), m_limit(limit) {}
  std::expected<void, StarException> push(char byte) {
    if (m_text.size() == m_limit)
      return std::unexpected(sizeError());
    m_text.push_back(byte);
    return {};
  }
  std::expected<void, StarException> append(std::string_view bytes) {
    if (bytes.size() > m_limit - m_text.size())
      return std::unexpected(sizeError());
    m_text.append(bytes);
    return {};
  }
private:
  std::string& m_text;
  std::size_t m_limit;
};

class StorageWriter : public JsonWriter<StorageOutput, true, true> {
  using Base = JsonWriter<StorageOutput, true, true>;
public:
  explicit StorageWriter(StorageOutput output) : Base(output) {}
  std::expected<void, StarException> beginObject() override {
    if (m_depth == MaxStorageDepth)
      return depthError();
    ++m_depth;
    return Base::beginObject();
  }
  std::expected<void, StarException> endObject() override {
    --m_depth;
    return Base::endObject();
  }
  std::expected<void, StarException> beginArray() override {
    if (m_depth == MaxStorageDepth)
      return depthError();
    ++m_depth;
    return Base::beginArray();
  }
  std::expected<void, StarException> endArray() override {
    --m_depth;
    return Base::endArray();
  }
  std::expected<void, StarException> putDouble(std::string_view token) override {
    auto valid = checkDouble(token);
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    auto written = Base::putDouble(token);
    if (!written)
      return std::unexpected(std::move(written.error()));
    // The engine parser treats decimal-free tokens as int64. A finite float
    // such as 1e19 must not be reloaded through its permissive integer parser.
    if (token.find_first_of(".eE") == std::string_view::npos)
      return Base::putDouble(".0");
    return {};
  }
  std::expected<void, StarException> putInteger(std::string_view token) override { return Base::putDouble(token); }
  std::expected<void, StarException> putBoolean(bool value) override { return Base::putDouble(value ? "true" : "false"); }
  std::expected<void, StarException> putNull() override { return Base::putDouble("null"); }
private:
  static std::expected<void, StarException> depthError() {
    return std::unexpected(StarException("Mod storage JSON exceeds 256 container levels"));
  }
  unsigned m_depth = 1;
};

// The ordinary engine parser intentionally preserves permissive numeric
// conversion. Storage opts into strict conversion without changing that API.
class StorageBuilder : public JsonBuilderStream {
public:
  std::expected<void, StarException> beginObject() override {
    auto valid = descend();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return JsonBuilderStream::beginObject();
  }
  std::expected<void, StarException> endObject() override {
    --m_depth;
    return JsonBuilderStream::endObject();
  }
  std::expected<void, StarException> beginArray() override {
    auto valid = descend();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return JsonBuilderStream::beginArray();
  }
  std::expected<void, StarException> endArray() override {
    --m_depth;
    return JsonBuilderStream::endArray();
  }
  std::expected<void, StarException> putDouble(std::string_view token) override {
    auto valid = checkDouble(token);
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return JsonBuilderStream::putDouble(token);
  }
  std::expected<void, StarException> putInteger(std::string_view token) override {
    std::int64_t value;
    auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
      return std::unexpected(StarException("Mod storage JSON integer is outside int64 range"));
    return JsonBuilderStream::putInteger(token);
  }
private:
  std::expected<void, StarException> descend() {
    if (m_depth == MaxStorageDepth)
      return std::unexpected(StarException("Mod storage JSON exceeds 256 container levels"));
    ++m_depth;
    return {};
  }
  unsigned m_depth = 0;
};

std::expected<std::string, StarException> serializeValue(Json const& value, std::size_t limit) {
  std::string text;
  StorageWriter writer{StorageOutput(text, limit)};
  auto written = JsonStreamer<Json>::toJsonStream(value, writer, false);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return text;
}

std::expected<std::string, StarException> serializeKey(String const& key, std::size_t limit) {
  std::string text;
  StorageWriter writer{StorageOutput(text, limit)};
  auto written = writer.putStringUtf8(key.utf8());
  if (!written)
    return std::unexpected(std::move(written.error()));
  return text;
}

std::expected<bool, StarException> storageFileExists(String const& filename) {
#ifdef STAR_SYSTEM_FAMILY_WINDOWS
  auto native = filename.wstring();
  if (!native)
    return std::unexpected(std::move(native.error()));
  std::filesystem::path path(*native);
#else
  std::filesystem::path path(filename.utf8());
#endif
  std::error_code error;
  auto status = std::filesystem::status(path, error);
  // File::exists also accepts ENOTDIR as missing; that is not an empty store.
  if (error == std::errc::no_such_file_or_directory)
    return false;
  if (error)
    return std::unexpected(StarException::format("Cannot inspect mod storage '{}': {}", filename, error.message()));
  if (status.type() == std::filesystem::file_type::not_found)
    return false;
  if (!std::filesystem::is_regular_file(status))
    return std::unexpected(StarException::format("Mod storage '{}' is not a regular file", filename));
  return true;
}

std::expected<std::string, StarException> readBounded(File& file) {
  auto size = file.size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (*size < 0 || *size > static_cast<StreamOffset>(ModStorage::MaxModDataSize))
    return std::unexpected(sizeError());
  std::string text;
  text.reserve(static_cast<std::size_t>(*size));
  std::array<char, 8192> buffer;
  for (;;) {
    auto count = file.read(buffer);
    if (!count)
      return std::unexpected(std::move(count.error()));
    if (*count == 0)
      return text;
    // Read through EOF, bounding growth as well as the initial size hint.
    if (*count > ModStorage::MaxModDataSize - text.size())
      return std::unexpected(sizeError());
    text.append(buffer.data(), *count);
  }
}

}

ModStorage::ModStorage(Logger& logger, String directory) : m_logger(logger), m_directory(std::move(directory)) {}

std::expected<std::unique_ptr<ModStorage>, StarException> ModStorage::create(Logger& logger, String directory) {
  if (directory.empty() || directory.utf8().find('\0') != std::string::npos)
    return std::unexpected(StarException("Mod storage directory must be a nonempty path without NUL bytes"));
  auto created = File::create_directories(directory);
  if (!created)
    return std::unexpected(std::move(created.error()));
  auto result = std::unique_ptr<ModStorage>(new ModStorage(logger, std::move(directory)));
  auto thread = Thread::invoke("ModStorage", [storage = result.get()] { return storage->run(); });
  if (!thread)
    return std::unexpected(std::move(thread.error()));
  result->m_thread = std::move(*thread);
  return result;
}

ModStorage::~ModStorage() {
  {
    std::lock_guard lock(m_mutex);
    m_stop = true;
    m_condition.notify_all();
  }
  auto joined = m_thread.finish();
  if (!joined)
    EmergencyLog::error("Mod storage worker shutdown failed: {}", joined.error().what());
  if (!m_thread.isFinished())
    Star::fatal_error("Cannot destroy mod storage with an unjoined writer", false);
  auto written = flush();
  if (!written)
    EmergencyLog::error("Mod storage final flush failed: {}", written.error().what());
}

std::expected<std::reference_wrapper<ModStorage::ModData>, StarException> ModStorage::load(String const& mod) {
  if (auto found = m_mods.find(mod); found != m_mods.end())
    return std::ref(found->second);
  ModData data;
  data.filename = File::relative_to(m_directory, "mod-" + hex_encode(sha256(mod)) + ".json");
  auto exists = storageFileExists(data.filename);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    File file(data.filename);
    auto opened = file.open(IOMode::Read);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    auto contents = readBounded(file);
    auto closed = file.close();
    if (!closed) {
      if (!contents)
        return std::unexpected(contents.error().with_related(closed.error(), "Closing mod storage also failed"));
      return std::unexpected(std::move(closed.error()));
    }
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto parsed = inputUtf8Json<std::string::const_iterator, StorageBuilder>(
        contents->cbegin(), contents->cend(), JsonParseType::Top);
    if (!parsed)
      return std::unexpected(parsed.error().with_context(Star::format("Loading mod storage '{}'", data.filename)));
    auto object = parsed->objectPtr();
    if (!object)
      return std::unexpected(std::move(object.error()));
    for (auto const& [key, value] : **object) {
      auto keyJson = serializeKey(key, MaxModDataSize - data.bytes);
      if (!keyJson)
        return std::unexpected(std::move(keyJson.error()));
      auto overhead = keyJson->size() + 1 + (data.entries.empty() ? 0 : 1);
      if (overhead > MaxModDataSize - data.bytes)
        return std::unexpected(sizeError());
      auto valueJson = serializeValue(value, MaxModDataSize - data.bytes - overhead);
      if (!valueJson)
        return std::unexpected(std::move(valueJson.error()));
      data.bytes += overhead + valueJson->size();
      data.entries.emplace(key, Entry{value, std::move(*keyJson), std::move(*valueJson)});
    }
  }
  return std::ref(m_mods.emplace(mod, std::move(data)).first->second);
}

std::expected<Json, StarException> ModStorage::get(String const& mod, String const& key) {
  std::lock_guard lock(m_mutex);
  auto loaded = load(mod);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto& data = loaded->get();
  auto found = data.entries.find(key);
  return found == data.entries.end() ? Json() : found->second.value;
}

void ModStorage::markDirty(ModData& data) {
  data.dirty = true;
  if (!m_nextWrite)
    m_nextWrite = std::chrono::steady_clock::now() + std::chrono::milliseconds(WriteDelayMillis);
  m_condition.notify_all();
}

std::expected<void, StarException> ModStorage::set(String const& mod, String const& key, Json const& value) {
  std::lock_guard lock(m_mutex);
  auto loaded = load(mod);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto& data = loaded->get();
  auto found = data.entries.find(key);
  if (value.isNull()) {
    if (found != data.entries.end()) {
      data.bytes -= found->second.size() + (data.entries.size() > 1 ? 1 : 0);
      data.entries.erase(found);
      markDirty(data);
    }
    return {};
  }
  if (found != data.entries.end() && found->second.value == value)
    return {};

  std::string newKey;
  std::size_t fixed;
  if (found == data.entries.end()) {
    auto serialized = serializeKey(key, MaxModDataSize - data.bytes);
    if (!serialized)
      return std::unexpected(std::move(serialized.error()));
    newKey = std::move(*serialized);
    fixed = data.bytes + newKey.size() + 1 + (data.entries.empty() ? 0 : 1);
  } else {
    fixed = data.bytes - found->second.valueJson.size();
  }
  if (fixed > MaxModDataSize)
    return std::unexpected(sizeError());
  auto serialized = serializeValue(value, MaxModDataSize - fixed);
  if (!serialized)
    return std::unexpected(std::move(serialized.error()));
  data.bytes = fixed + serialized->size();
  if (found == data.entries.end())
    data.entries.emplace(key, Entry{value, std::move(newKey), std::move(*serialized)});
  else {
    found->second.value = value;
    found->second.valueJson = std::move(*serialized);
  }
  markDirty(data);
  return {};
}

std::expected<JsonArray, StarException> ModStorage::keys(String const& mod) {
  std::lock_guard lock(m_mutex);
  auto loaded = load(mod);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  JsonArray keys;
  keys.reserve(loaded->get().entries.size());
  for (auto const& [key, entry] : loaded->get().entries)
    keys.emplace_back(key);
  return keys;
}

std::expected<void, StarException> ModStorage::clear(String const& mod) {
  std::lock_guard lock(m_mutex);
  auto loaded = load(mod);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto& data = loaded->get();
  if (!data.entries.empty()) {
    data.entries.clear();
    data.bytes = 2;
    markDirty(data);
  }
  return {};
}

std::expected<void, StarException> ModStorage::flushLocked() {
  std::optional<StarException> failure;
  std::string contents;
  bool pending = false;
  for (auto& [mod, data] : m_mods) {
    if (!data.dirty)
      continue;
    contents.clear();
    contents.reserve(data.bytes);
    contents.push_back('{');
    bool first = true;
    for (auto const& [key, entry] : data.entries) {
      if (!first)
        contents.push_back(',');
      first = false;
      contents.append(entry.keyJson);
      contents.push_back(':');
      contents.append(entry.valueJson);
    }
    contents.push_back('}');
    auto written = File::overwrite_file_with_rename(std::span<char const>(contents), data.filename);
    if (written)
      data.dirty = false;
    else {
      pending = true;
      if (!failure)
        failure = written.error().with_context(Star::format("Saving mod storage '{}'", data.filename));
    }
  }
  if (pending)
    m_nextWrite = std::chrono::steady_clock::now() + std::chrono::milliseconds(WriteDelayMillis);
  else
    m_nextWrite.reset();
  m_condition.notify_all();
  if (failure)
    return std::unexpected(std::move(*failure));
  return {};
}

std::expected<void, StarException> ModStorage::flush() {
  std::lock_guard lock(m_mutex);
  return flushLocked();
}

std::expected<void, StarException> ModStorage::run() {
  std::unique_lock lock(m_mutex);
  while (!m_stop) {
    if (!m_nextWrite) {
      m_condition.wait(lock, [this] { return m_stop || m_nextWrite.has_value(); });
      continue;
    }
    auto deadline = *m_nextWrite;
    if (m_condition.wait_until(lock, deadline, [this, deadline] {
          return m_stop || !m_nextWrite || *m_nextWrite != deadline;
        }))
      continue;
    auto written = flushLocked();
    // Logger sinks are user callbacks; never invoke them under the storage
    // mutex. A sink may itself call one of the storage APIs.
    lock.unlock();
    if (!written) {
      auto logged = m_logger.error_exception("Mod storage background flush failed", ExceptionDiagnostic(written.error()));
      if (!logged)
        EmergencyLog::error("Mod storage background flush diagnostic failed: {}", logged.error().what());
    }
    lock.lock();
  }
  return {};
}

}
