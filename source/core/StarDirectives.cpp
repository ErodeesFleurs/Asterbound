import std;
import star.string;
import star.format;
import star.hash;
import star.json;
import star.xx_hash;
import star.list;
import star.image_processing;
import star.data_stream;
import star.string_view;
import star.directives;
import star.image;
import star.logging;
import star.vector;
import star.rect;

namespace Star {

struct Directives::Shared {
  Shared(List<Entry>&& entries, String&& string)
    : entries(std::move(entries)), string(std::move(string)),
      hash(xx_hash3(std::span<char const>(this->string.utf8()))) {}

  void load_operations() const {
    std::call_once(loaded, [&] {
      List<ImageOperation> parsed;
      parsed.reserve(entries.size());
      bool has_errors = false;
      for (auto const& entry : entries) {
        auto operation = image_operation_from_string(std::string_view(string.utf8()).substr(entry.begin, entry.length));
        has_errors |= std::holds_alternative<ErrorImageOperation>(operation);
        parsed.push_back(std::move(operation));
      }
      if (has_errors)
        reported = std::make_unique<std::atomic<bool>[]>(entries.size());
      operations = std::move(parsed);
    });
  }

  List<Entry> entries;
  String string;
  std::size_t hash;
  mutable std::once_flag loaded;
  mutable List<ImageOperation> operations;
  mutable std::unique_ptr<std::atomic<bool>[]> reported;
};

Directives::Directives() = default;

Directives::Directives(String const& directives) {
  parse(String(directives));
}

Directives::Directives(String&& directives) {
  parse(std::move(directives));
}

Directives::Directives(std::string_view directives) {
  parse(String(directives));
}

Directives::Directives(Directives&& directives) noexcept = default;

Directives::Directives(Directives const& directives) = default;

Directives::~Directives() = default;

Directives& Directives::operator=(String const& s) {
  if (m_shared && m_shared->string == s)
    return *this;

  parse(String(s));
  return *this;
}

Directives& Directives::operator=(String&& s) {
  if (m_shared && m_shared->string == s) {
    s.clear();
    return *this;
  }

  parse(std::move(s));
  return *this;
}

Directives& Directives::operator=(std::string_view s) {
  if (m_shared && m_shared->string.utf8().compare(s) == 0)
    return *this;

  parse(String(s));
  return *this;
}

Directives& Directives::operator=(Directives&& other) noexcept = default;

Directives& Directives::operator=(Directives const& other) = default;

void Directives::load_operations() const {
  if (m_shared)
    m_shared->load_operations();
}

void Directives::parse(String&& directives) {
  if (directives.empty()) {
    m_shared.reset();
    return;
  }

  List<Entry> entries;
  auto bytes = std::string_view(directives.utf8());
  for (std::size_t begin = 0; begin < bytes.size();) {
    auto end = bytes.find('?', begin);
    if (end == std::string_view::npos)
      end = bytes.size();
    if (end != begin)
      entries.push_back(Entry{begin, end - begin, entries.size()});
    begin = end + 1;
  }

  if (entries.empty()) {
    m_shared.reset();
    return;
  }

  bool preload = bytes.size() < 1000;
  m_shared = std::make_shared<Shared const>(std::move(entries), std::move(directives));
  if (preload)
    load_operations();
}

std::span<Directives::Entry const> Directives::entries() const & {
  return m_shared ? std::span<Entry const>(m_shared->entries) : std::span<Entry const>{};
}

std::span<ImageOperation const> Directives::operations() const & {
  load_operations();
  return m_shared ? std::span<ImageOperation const>(m_shared->operations) : std::span<ImageOperation const>{};
}

StringView Directives::entry_string(Entry const& entry) const & {
  return std::string_view(m_shared->string.utf8()).substr(entry.begin, entry.length);
}

ImageOperation const& Directives::entry_operation(Entry const& entry) const & {
  load_operations();
  return m_shared->operations[entry.index];
}

bool Directives::error_reported(Entry const& entry) const {
  if (!std::holds_alternative<ErrorImageOperation>(entry_operation(entry)))
    return true;
  return m_shared->reported[entry.index].load(std::memory_order_relaxed);
}

void Directives::mark_error_reported(Entry const& entry) const {
  m_shared->reported[entry.index].store(true, std::memory_order_relaxed);
}

StringView Directives::prefix() const & {
  if (!m_shared || m_shared->string.utf8()[0] == '?')
    return "";
  return entry_string(m_shared->entries.front());
}

String Directives::string() const {
  if (!m_shared)
    return "";
  else
    return m_shared->string;
}

std::optional<std::reference_wrapper<String const>> Directives::maybe_string() const & {
  if (!m_shared)
    return std::nullopt;
  else
    return std::cref(m_shared->string);
}


String Directives::build_string() const {
  String built;
  if (m_shared) {
    built.reserve(m_shared->string.utf8().size());
    for (auto const& entry : m_shared->entries) {
      if (entry.begin > 0)
        built += "?";
      built += entry_string(entry);
    }
  }
  return built;
}

String& Directives::append_to(String& out) const {
  if (m_shared) {
    out.reserve(out.utf8().size() + m_shared->string.utf8().size());
    out += m_shared->string;
  }
  return out;
}

std::size_t Directives::hash() const {
  return m_shared ? m_shared->hash : 0;
}

std::size_t Directives::size() const {
  return m_shared ? m_shared->entries.size() : 0;
}

bool Directives::empty() const {
  return !m_shared;
}

Directives::operator bool() const {
  return !empty();
}

bool Directives::equals(Directives const& other) const {
  if (m_shared == other.m_shared)
    return true;
  return hash() == other.hash() && m_shared && other.m_shared && m_shared->string == other.m_shared->string;
}

bool Directives::equals(String const& string) const {
  auto directive_string = maybe_string();
  return directive_string ? string == directive_string->get() : string.empty();
}

bool Directives::operator==(Directives const& other) const {
  return equals(other);
}

bool Directives::operator==(String const& string) const {
  return equals(string);
}

bool Directives::operator!=(Directives const& other) const {
  return !equals(other);
}

bool Directives::operator!=(String const& string) const {
  return !equals(string);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, Directives& directives) {
  String string;
  auto read = ds.read(string);
  if (!read)
    return std::unexpected(std::move(read.error()));
  directives.parse(std::move(string));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream & ds, Directives const& directives) {
  auto written = ds.write(directives.string());
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(ds);
}

DirectivesGroup::DirectivesGroup() = default;

DirectivesGroup::DirectivesGroup(String const& directives) {
  append(Directives(directives));
}

DirectivesGroup::DirectivesGroup(String&& directives) {
  append(Directives(std::move(directives)));
}

DirectivesGroup::DirectivesGroup(DirectivesGroup const& directives)
  : m_directives(directives.m_directives), m_count(directives.m_count),
    m_hash(directives.m_hash.load(std::memory_order_relaxed)) {}

DirectivesGroup::DirectivesGroup(DirectivesGroup&& directives) noexcept
  : m_directives(std::move(directives.m_directives)), m_count(std::exchange(directives.m_count, 0)),
    m_hash(directives.m_hash.exchange(0, std::memory_order_relaxed)) {
  directives.m_directives.clear();
}

DirectivesGroup& DirectivesGroup::operator=(DirectivesGroup const& directives) {
  if (this != &directives) {
    m_directives = directives.m_directives;
    m_count = directives.m_count;
    m_hash.store(directives.m_hash.load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  return *this;
}

DirectivesGroup& DirectivesGroup::operator=(DirectivesGroup&& directives) noexcept {
  if (this != &directives) {
    m_directives = std::move(directives.m_directives);
    m_count = std::exchange(directives.m_count, 0);
    m_hash.store(directives.m_hash.exchange(0, std::memory_order_relaxed), std::memory_order_relaxed);
    directives.m_directives.clear();
  }
  return *this;
}

DirectivesGroup::~DirectivesGroup() = default;

bool DirectivesGroup::empty() const {
  return m_count == 0;
}

DirectivesGroup::operator bool() const {
  return !empty();
}

bool DirectivesGroup::compare(DirectivesGroup const& other) const {
  if (m_count != other.m_count || m_directives.size() != other.m_directives.size() || hash() != other.hash())
    return false;
  return std::equal(m_directives.begin(), m_directives.end(), other.m_directives.begin());
}

void DirectivesGroup::append(Directives directives) {
  if (directives.empty())
    return;
  m_count += directives.size();
  m_directives.emplace_back(std::move(directives));
  m_hash.store(0, std::memory_order_relaxed);
}

void DirectivesGroup::clear() {
  m_directives.clear();
  m_count = 0;
  m_hash.store(0, std::memory_order_relaxed);
}

DirectivesGroup& DirectivesGroup::operator+=(Directives other) {
  append(std::move(other));
  return *this;
}

String DirectivesGroup::to_string() const {
  String string;
  append_to(string);
  return string;
}

void DirectivesGroup::append_to(String& string) const {
  std::size_t capacity = string.utf8().size();
  for (auto const& directives : m_directives)
    capacity += directives.maybe_string()->get().utf8().size() + 1;
  string.reserve(capacity);
  for (auto const& directives : m_directives) {
    auto const& text = directives.maybe_string()->get();
    if (!string.empty() && string.utf8().back() != '?' && text.utf8()[0] != '?')
      string.append("?");
    string += text;
  }
}
static std::expected<void, StarException>
collect_image_operations(DirectivesGroup const& group, List<std::reference_wrapper<ImageOperation const>>& operations) {
  bool first = true;
  for (auto const& directives : group.list()) {
    auto parsed = directives.operations();
    for (auto const& entry : directives.entries()) {
      ImageOperation const& operation = parsed[entry.index];
      if (std::holds_alternative<ErrorImageOperation>(operation))
        return std::unexpected(std::get<ErrorImageOperation>(operation).cause);
      if (!first && entry.begin != 0 && std::holds_alternative<NullImageOperation>(operation))
        return std::unexpected(DirectivesException::format("Invalid image operation: {}", directives.entry_string(entry)));
      // Accepted group prefixes only promote 24-bit input, already done by any
      // preceding executable operation. Omit redundant prefixes across groups.
      if (!std::holds_alternative<NullImageOperation>(operation) || operations.empty())
        operations.push_back(std::cref(operation));
      first = false;
    }
  }
  return {};
}

std::expected<Image, StarException> DirectivesGroup::apply_new_image(Image const& image,
                                                                     ImageReferenceCallback ref_callback) const {
  List<std::reference_wrapper<ImageOperation const>> operations;
  operations.reserve(m_count);
  auto collected = collect_image_operations(*this, operations);
  std::optional<Image> result;
  if (image.bytes_per_pixel() == 3 || image.bytes_per_pixel() == 4) {
    for (std::size_t i = 0; i < operations.size(); ++i) {
      auto const& operation = operations[i].get();
      if (std::holds_alternative<CropImageOperation>(operation)) {
        auto const& crop = std::get<CropImageOperation>(operation);
        auto min = crop.subset.min(), max = crop.subset.max();
        if (min[0] >= 0 && min[1] >= 0 && max[0] >= min[0] && max[1] >= min[1] && std::uint64_t(max[0]) <= image.width()
            && std::uint64_t(max[1]) <= image.height()
            && std::uint64_t(max[0] - min[0]) * (max[1] - min[1]) < std::uint64_t(image.width()) * image.height()) {
          // Exactly the in-place optimizer's safe reorder, seeded from the
          // original source instead of first copying all of its pixels.
          [[maybe_unused]] auto operation_callback = ref_callback;
          auto cropped = image.sub_image(Vec2U(min), Vec2U(crop.subset.size()));
          if (!cropped)
            return std::unexpected(std::move(cropped.error()));
          result.emplace(std::move(*cropped));
          if (result->bytes_per_pixel() == 3)
            *result =
              result->convert(result->pixel_format() == PixelFormat::BGR24 ? PixelFormat::BGRA32 : PixelFormat::RGBA32);
          operations.erase(operations.begin() + i);
        }
        break;
      }
      // Scanlines depend on absolute y, and references can alias the original.
      if (!std::holds_alternative<NullImageOperation>(operation)
          && !std::holds_alternative<HueShiftImageOperation>(operation)
          && !std::holds_alternative<SaturationShiftImageOperation>(operation)
          && !std::holds_alternative<BrightnessMultiplyImageOperation>(operation)
          && !std::holds_alternative<FadeToColorImageOperation>(operation)
          && !std::holds_alternative<SetColorImageOperation>(operation)
          && !std::holds_alternative<ColorReplaceImageOperation>(operation)
          && !std::holds_alternative<MultiplyImageOperation>(operation))
        break;
    }
  }
  if (!result)
    result.emplace(image);
  auto applied = process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const>(operations),
                                          *result, std::move(ref_callback));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  if (!collected)
    return std::unexpected(std::move(collected.error()));
  return std::move(*result);
}

std::expected<void, StarException> DirectivesGroup::apply_existing_image(Image& image,
                                                                         ImageReferenceCallback ref_callback) const {
  List<std::reference_wrapper<ImageOperation const>> operations;
  operations.reserve(m_count);
  auto collected = collect_image_operations(*this, operations);
  // A later parse error must not hide an earlier execution error or discard
  // the successfully applied prefix of an existing-image operation.
  auto applied = process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const>(operations),
                                          image, std::move(ref_callback));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  return collected;
}

std::size_t DirectivesGroup::hash() const {
  auto cached = m_hash.load(std::memory_order_relaxed);
  if (cached != 0)
    return cached;
  XXHash3 hasher;
  for (auto const& directives : m_directives) {
    auto bytes = std::bit_cast<std::array<char, sizeof(std::size_t)>>(directives.hash());
    hasher.push(bytes);
  }

  auto result = hasher.digest();
  m_hash.store(result, std::memory_order_relaxed);
  return result;
}

List<Directives> const& DirectivesGroup::list() const & {
  return m_directives;
}

bool operator==(DirectivesGroup const& a, DirectivesGroup const& b) {
  return a.compare(b);
}

bool operator!=(DirectivesGroup const& a, DirectivesGroup const& b) {
  return !a.compare(b);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directives_group) {
  String string;
  auto read = ds.read(string);
  if (!read)
    return std::unexpected(std::move(read.error()));
  directives_group = DirectivesGroup(std::move(string));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directives_group) {
  // The historical flattened string codec preserves ordinary bytes, but not
  // arbitrary child boundaries or prefixes used by structural group identity.
  auto written = ds.write(directives_group.to_string());
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(ds);
}

std::size_t ValueHash<DirectivesGroup>::operator()(DirectivesGroup const& s) const {
  return s.hash();
}

}