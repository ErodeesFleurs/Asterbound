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
constexpr unsigned max_storage_depth = 256;

StarException size_error() {
  return StarException("Mod storage exceeds the 4 MiB compact JSON limit");
}

StarException number_error() {
  return StarException("Mod storage requires finite, representable JSON numbers");
}

class StorageOutput {
public:
  StorageOutput(std::string& text, std::size_t limit) : m_text(text), m_limit(limit) {}
  std::expected<void, StarException> push(char byte) {
    if (m_text.size() == m_limit)
      return std::unexpected(size_error());
    m_text.push_back(byte);
    return {};
  }
  std::expected<void, StarException> append(std::string_view bytes) {
    if (bytes.size() > m_limit - m_text.size())
      return std::unexpected(size_error());
    m_text.append(bytes);
    return {};
  }
private:
  std::string& m_text;
  std::size_t m_limit;
};

class StorageWriter final : public JsonWriter<StorageOutput, true, true> {
  using Base = JsonWriter<StorageOutput, true, true>;
public:
  explicit StorageWriter(StorageOutput output) : Base(output) {}
  std::expected<void, StarException> begin_object() override {
    if (m_depth == max_storage_depth)
      return depth_error();
    ++m_depth;
    return Base::begin_object();
  }
  std::expected<void, StarException> end_object() override {
    --m_depth;
    return Base::end_object();
  }
  std::expected<void, StarException> begin_array() override {
    if (m_depth == max_storage_depth)
      return depth_error();
    ++m_depth;
    return Base::begin_array();
  }
  std::expected<void, StarException> end_array() override {
    --m_depth;
    return Base::end_array();
  }
  std::expected<void, StarException> put_double(std::string_view token) override {
    // JsonStreamer supplies formatted numbers; finite ones begin with a digit
    // after the optional sign, unlike inf or nan. Do not reparse the output.
    auto digits = token;
    if (digits.starts_with('-'))
      digits.remove_prefix(1);
    if (digits.empty() || digits.front() < '0' || digits.front() > '9')
      return std::unexpected(number_error());
    auto written = Base::put_double(token);
    if (!written)
      return std::unexpected(std::move(written.error()));
    // The engine parser treats decimal-free tokens as int64. A finite float
    // such as 1e19 must not be reloaded through its permissive integer parser.
    if (token.find_first_of(".eE") == std::string_view::npos)
      return Base::put_double(".0");
    return {};
  }
  std::expected<void, StarException> put_integer(std::string_view token) override { return Base::put_double(token); }
  std::expected<void, StarException> put_boolean(bool value) override {
    return Base::put_double(value ? "true" : "false");
  }
  std::expected<void, StarException> put_null() override { return Base::put_double("null"); }

private:
  static std::expected<void, StarException> depth_error() {
    return std::unexpected(StarException("Mod storage JSON exceeds 256 container levels"));
  }
  unsigned m_depth = 1;
};

// The ordinary engine parser intentionally preserves permissive numeric
// conversion. Storage opts into strict conversion without changing that API.
class StorageBuilder final : public JsonBuilderStream {
public:
  std::expected<void, StarException> begin_object() override {
    auto valid = descend();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return JsonBuilderStream::begin_object();
  }
  std::expected<void, StarException> end_object() override {
    --m_depth;
    return JsonBuilderStream::end_object();
  }
  std::expected<void, StarException> begin_array() override {
    auto valid = descend();
    if (!valid)
      return std::unexpected(std::move(valid.error()));
    return JsonBuilderStream::begin_array();
  }
  std::expected<void, StarException> end_array() override {
    --m_depth;
    return JsonBuilderStream::end_array();
  }
  std::expected<void, StarException> put_double(std::string_view token) override {
    double value = 0;
    auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    constexpr std::uint64_t exponent_mask = 0x7ff0000000000000ULL;
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()
        || (std::bit_cast<std::uint64_t>(value) & exponent_mask) == exponent_mask)
      return std::unexpected(number_error());
    push_double(value);
    return {};
  }
  std::expected<void, StarException> put_integer(std::string_view token) override {
    std::int64_t value;
    auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
      return std::unexpected(StarException("Mod storage JSON integer is outside int64 range"));
    push_integer(value);
    return {};
  }

private:
  std::expected<void, StarException> descend() {
    if (m_depth == max_storage_depth)
      return std::unexpected(StarException("Mod storage JSON exceeds 256 container levels"));
    ++m_depth;
    return {};
  }
  unsigned m_depth = 0;
};

std::expected<std::string, StarException> serialize_value(Json const& value, std::size_t limit) {
  std::string text;
  StorageWriter writer{StorageOutput(text, limit)};
  auto written = JsonStreamer<Json>::to_json_stream(value, writer, false);
  if (!written)
    return std::unexpected(std::move(written.error()));
  return text;
}

std::expected<std::string, StarException> serialize_key(String const& key, std::size_t limit) {
  std::string text;
  StorageWriter writer{StorageOutput(text, limit)};
  auto written = writer.put_string_utf8(key.utf8());
  if (!written)
    return std::unexpected(std::move(written.error()));
  return text;
}

std::expected<bool, StarException> storage_file_exists(String const& filename) {
  auto path = File::filesystem_path(filename);
  if (!path)
    return std::unexpected(std::move(path.error()));
  std::error_code error;
  auto status = std::filesystem::status(*path, error);
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

std::expected<std::string, StarException> read_bounded(File& file) {
  auto size = file.size();
  if (!size)
    return std::unexpected(std::move(size.error()));
  if (*size < 0 || *size > static_cast<StreamOffset>(ModStorage::max_mod_data_size))
    return std::unexpected(size_error());
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
    if (*count > ModStorage::max_mod_data_size - text.size())
      return std::unexpected(size_error());
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
  auto thread = Thread::invoke("ModStorage", [&storage = *result] { return storage.run(); });
  if (!thread)
    return std::unexpected(std::move(thread.error()));
  result->m_thread = std::move(*thread);
  return result;
}

ModStorage::~ModStorage() {
  {
    std::lock_guard lock(m_mutex);
    m_stop = true;
  }
  m_condition.notify_one();
  auto joined = m_thread.finish();
  if (!joined)
    EmergencyLog::error("Mod storage worker shutdown failed: {}", joined.error().what());
  if (!m_thread.is_finished())
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
  auto exists = storage_file_exists(data.filename);
  if (!exists)
    return std::unexpected(std::move(exists.error()));
  if (*exists) {
    File file(data.filename);
    auto opened = file.open(IOMode::Read);
    if (!opened)
      return std::unexpected(std::move(opened.error()));
    auto contents = read_bounded(file);
    auto closed = file.close();
    if (!closed) {
      if (!contents)
        return std::unexpected(contents.error().with_related(closed.error(), "Closing mod storage also failed"));
      return std::unexpected(std::move(closed.error()));
    }
    if (!contents)
      return std::unexpected(std::move(contents.error()));
    auto parsed = input_utf8_json<std::string::const_iterator, StorageBuilder>(contents->cbegin(), contents->cend(),
                                                                               JsonParseType::Top);
    if (!parsed)
      return std::unexpected(parsed.error().with_context(Star::format("Loading mod storage '{}'", data.filename)));
    auto object = parsed->object_ptr();
    if (!object)
      return std::unexpected(std::move(object.error()));
    data.entries.reserve((*object)->size());
    for (auto const& [key, value] : **object) {
      auto inserted = insert_new_entry(data, key, value);
      if (!inserted)
        return std::unexpected(std::move(inserted.error()));
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

std::expected<void, StarException> ModStorage::insert_new_entry(ModData& data, String const& key, Json const& value) {
  auto remaining = max_mod_data_size - data.bytes;
  auto key_json = serialize_key(key, remaining);
  if (!key_json)
    return std::unexpected(std::move(key_json.error()));
  auto overhead = key_json->size() + 1 + (data.entries.empty() ? 0 : 1);
  if (overhead > remaining)
    return std::unexpected(size_error());
  auto value_json = serialize_value(value, remaining - overhead);
  if (!value_json)
    return std::unexpected(std::move(value_json.error()));

  auto added_bytes = overhead + value_json->size();
  data.entries.emplace(key, Entry{value, std::move(*key_json), std::move(*value_json)});
  data.bytes += added_bytes;
  return {};
}

void ModStorage::mark_dirty(ModData& data) {
  data.dirty = true;
  if (!m_next_write)
    set_write_deadline(std::chrono::steady_clock::now() + write_delay);
}

void ModStorage::set_write_deadline(std::optional<std::chrono::steady_clock::time_point> deadline) {
  if (m_next_write == deadline)
    return;
  m_next_write = deadline;
  m_condition.notify_one();
}

std::expected<void, StarException> ModStorage::set(String const& mod, String const& key, Json const& value) {
  std::lock_guard lock(m_mutex);
  auto loaded = load(mod);
  if (!loaded)
    return std::unexpected(std::move(loaded.error()));
  auto& data = loaded->get();
  auto found = data.entries.find(key);
  if (value.is_null()) {
    if (found != data.entries.end()) {
      data.bytes -= found->second.size() + (data.entries.size() > 1 ? 1 : 0);
      data.entries.erase(found);
      mark_dirty(data);
    }
    return {};
  }
  if (found == data.entries.end()) {
    auto inserted = insert_new_entry(data, key, value);
    if (!inserted)
      return std::unexpected(std::move(inserted.error()));
  } else {
    auto& entry = found->second;
    if (entry.value == value)
      return {};
    auto fixed_bytes = data.bytes - entry.value_json.size();
    auto value_json = serialize_value(value, max_mod_data_size - fixed_bytes);
    if (!value_json)
      return std::unexpected(std::move(value_json.error()));
    data.bytes = fixed_bytes + value_json->size();
    entry.value = value;
    entry.value_json = std::move(*value_json);
  }
  mark_dirty(data);
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
    mark_dirty(data);
  }
  return {};
}

std::expected<void, StarException> ModStorage::flush_locked() {
  if (!m_next_write)
    return {};

  std::optional<StarException> failure;
  std::string contents;
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
      contents.append(entry.key_json);
      contents.push_back(':');
      contents.append(entry.value_json);
    }
    contents.push_back('}');
    auto written = File::overwrite_file_with_rename(std::span<char const>(contents), data.filename);
    if (written)
      data.dirty = false;
    else if (!failure)
      failure = written.error().with_context(Star::format("Saving mod storage '{}'", data.filename));
  }
  if (failure) {
    set_write_deadline(std::chrono::steady_clock::now() + write_delay);
    return std::unexpected(std::move(*failure));
  }
  set_write_deadline(std::nullopt);
  return {};
}

std::expected<void, StarException> ModStorage::flush() {
  std::lock_guard lock(m_mutex);
  return flush_locked();
}

std::expected<void, StarException> ModStorage::run() {
  std::unique_lock lock(m_mutex);
  while (!m_stop) {
    if (!m_next_write) {
      m_condition.wait(lock, [&storage = *this] { return storage.m_stop || storage.m_next_write.has_value(); });
      continue;
    }
    auto deadline = *m_next_write;
    if (m_condition.wait_until(lock, deadline, [&storage = *this, deadline] {
          return storage.m_stop || storage.m_next_write != deadline;
        }))
      continue;
    if (auto written = flush_locked(); !written) {
      // Logger sinks are user callbacks; never invoke them under the storage
      // mutex. A sink may itself call one of the storage APIs.
      lock.unlock();
      auto logged = m_logger.error_exception("Mod storage background flush failed", ExceptionDiagnostic(written.error()));
      if (!logged)
        EmergencyLog::error("Mod storage background flush diagnostic failed: {}", logged.error().what());
      lock.lock();
    }
  }
  return {};
}

}
