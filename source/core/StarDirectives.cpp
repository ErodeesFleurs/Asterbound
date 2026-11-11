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

Directives::Entry::Entry(ImageOperation&& newOperation, std::size_t strBegin, std::size_t strLength) {
  operation = std::move(newOperation);
  begin = strBegin;
  length = strLength;
}

Directives::Entry::Entry(ImageOperation const& newOperation, std::size_t strBegin, std::size_t strLength) {
  operation = newOperation;
  begin = strBegin;
  length = strLength;
}

Directives::Entry::Entry(Entry const& other) {
  operation = other.operation;
  begin = other.begin;
  length = other.length;
}

ImageOperation const& Directives::Entry::loadOperation(Shared const& parent) const {
  if (std::holds_alternative<NullImageOperation>(operation)) {
    operation = image_operation_from_string(string(parent));
  }
  return operation;
}

StringView Directives::Entry::string(Shared const& parent) const {
  StringView result = parent.string;
  result = result.utf8().substr(begin, length);
  return result;
}

bool Directives::Shared::empty() const {
  return entries.empty();
}

Directives::Shared::Shared() = default;

Directives::Shared::Shared(List<Entry>&& givenEntries, String&& givenString) {
  entries = std::move(givenEntries);
  string = std::move(givenString);
  hash = string.empty() ? 0 : xx_hash3(std::span<char const>(string.utf8()));
}

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

Directives::Directives(Directives&& directives) noexcept {
  *this = std::move(directives);
}

Directives::Directives(Directives const& directives) {
  *this = directives;
}

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

Directives& Directives::operator=(Directives&& other) noexcept {
  m_shared = std::move(other.m_shared);
  return *this;
}

Directives& Directives::operator=(Directives const& other) = default;

void Directives::loadOperations() const {
  if (!m_shared)
    return;

  std::unique_lock locker(m_shared->mutex, std::defer_lock);
  if (m_shared.use_count() != 1)
    locker.lock();
  for (auto& entry : m_shared->entries)
    entry.loadOperation(*m_shared);
}

void Directives::parse(String&& directives) {
  if (directives.empty()) {
    m_shared.reset();
    return;
  }

  List<Entry> entries;
  StringView view(directives);
  auto bytes = view.utf8();
  for (std::size_t begin = 0; begin < bytes.size();) {
    auto end = bytes.find('?', begin);
    if (end == std::string_view::npos)
      end = bytes.size();
    if (end != begin) {
      ImageOperation operation = begin == 0 ? image_operation_from_string(bytes.substr(begin, end - begin))
                                            : ImageOperation(NullImageOperation{});
      entries.emplace_back(std::move(operation), begin, end - begin);
    }
    begin = end + 1;
  }

  if (entries.empty()) {
    m_shared.reset();
    return;
  }

  m_shared = std::make_shared<Shared const>(std::move(entries), std::move(directives));
  if (view.utf8().size() < 1000) { // Pre-load short enough directives
    for (auto& entry : m_shared->entries)
      entry.loadOperation(*m_shared);
  }
}

StringView Directives::prefix() const {
  if (!m_shared)
    return "";
  else if (m_shared->empty())
    return m_shared->string;
  else if (m_shared->string.utf8()[0] == '?')
    return "";
  else
    return m_shared->entries.front().string(*m_shared);
}

String Directives::string() const {
  if (!m_shared)
    return "";
  else
    return m_shared->string;
}

std::optional<std::reference_wrapper<String const>> Directives::maybeString() const {
  if (!m_shared)
    return std::nullopt;
  else
    return std::cref(m_shared->string);
}


String Directives::buildString() const {
  if (m_shared) {
    String built;
    for (auto& entry : m_shared->entries) {
      if (entry.begin > 0)
        built += "?";
      built += entry.string(*m_shared);
    }

    return built;
  }

  return String();
}

String& Directives::addToString(String& out) const {
  if (!empty())
    out += m_shared->string;
  return out;
}

std::size_t Directives::hash() const {
  return m_shared ? m_shared->hash : 0;
}

std::size_t Directives::size() const {
  return m_shared ? m_shared->entries.size() : 0;
}

bool Directives::empty() const {
  return !m_shared || m_shared->empty();
}

Directives::operator bool() const {
  return !empty();
}

bool Directives::equals(Directives const& other) const {
  return hash() == other.hash();
}

bool Directives::equals(String const& string) const {
  auto directiveString = maybeString();
  return directiveString ? string == directiveString->get() : string.empty();
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

bool operator==(Directives const& d1, Directives const& d2) {
  return d1.equals(d2);
}

bool operator==(String const& string, Directives const& directives) {
  return directives.equals(string);
}

bool operator==(Directives const& directives, String const& string) {
  return directives.equals(string);
}

DirectivesGroup::DirectivesGroup() = default;
DirectivesGroup::DirectivesGroup(String const& directives) {
  if (directives.empty())
    return;

  Directives parsed(directives);
  if (parsed) {
    m_directives.emplace_back(std::move(parsed));
    m_count = m_directives.back().size();
  }
}
DirectivesGroup::DirectivesGroup(String&& directives) {
  if (directives.empty()) {
    directives.clear();
    return;
  }

  Directives parsed(std::move(directives));
  if (parsed) {
    m_directives.emplace_back(std::move(parsed));
    m_count = m_directives.back().size();
  }
}

DirectivesGroup::~DirectivesGroup() = default;

bool DirectivesGroup::empty() const {
  return m_count == 0;
}

DirectivesGroup::operator bool() const {
  return empty();
}

bool DirectivesGroup::compare(DirectivesGroup const& other) const {
  if (m_count != other.m_count)
    return false;

  if (empty())
    return true;

  return hash() == other.hash();
}

void DirectivesGroup::append(Directives const& directives) {
  m_directives.emplace_back(directives);
  m_count += m_directives.back().size();
}

void DirectivesGroup::clear() {
  m_directives.clear();
  m_count = 0;
}

DirectivesGroup& DirectivesGroup::operator+=(Directives const& other) {
  append(other);
  return *this;
}

String DirectivesGroup::toString() const {
  String string;
  addToString(string);
  return string;
}

void DirectivesGroup::addToString(String& string) const {
  for (auto& entry : m_directives) {
    if (entry && !entry.m_shared->string.empty()) {
      if (!string.empty() && string.utf8().back() != '?' && entry.m_shared->string.utf8()[0] != '?')
        string.append("?");
      string += entry.m_shared->string;
    }
  }
}

static std::expected<void, StarException>
collect_image_operations(DirectivesGroup const& group, List<std::reference_wrapper<ImageOperation const>>& operations) {
  bool first = true;
  return group.forEach([&](Directives::Entry const& entry,
                           Directives const& directives) -> std::expected<void, StarException> {
    ImageOperation const& operation = entry.loadOperation(*directives.m_shared);
    if (std::holds_alternative<ErrorImageOperation>(operation))
      return std::unexpected(std::get<ErrorImageOperation>(operation).cause);
    if (!first && entry.begin != 0 && std::holds_alternative<NullImageOperation>(operation))
      return std::unexpected(DirectivesException::format("Invalid image operation: {}", entry.string(*directives.m_shared)));
    // Accepted group prefixes only promote 24-bit input, already done by any
    // preceding executable operation. Omit redundant prefixes across groups.
    if (!std::holds_alternative<NullImageOperation>(operation) || operations.empty())
      operations.push_back(std::cref(operation));
    first = false;
    return {};
  });
}

std::expected<Image, StarException> DirectivesGroup::apply_new_image(Image const& image,
                                                                     ImageReferenceCallback refCallback) const {
  List<std::reference_wrapper<ImageOperation const>> operations;
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
          [[maybe_unused]] auto operationCallback = refCallback;
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
                                          *result, std::move(refCallback));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  if (!collected)
    return std::unexpected(std::move(collected.error()));
  return std::move(*result);
}

std::expected<void, StarException> DirectivesGroup::apply_existing_image(Image& image,
                                                                         ImageReferenceCallback refCallback) const {
  List<std::reference_wrapper<ImageOperation const>> operations;
  auto collected = collect_image_operations(*this, operations);
  // A later parse error must not hide an earlier execution error or discard
  // the successfully applied prefix of an existing-image operation.
  auto applied = process_image_operations(std::span<std::reference_wrapper<ImageOperation const> const>(operations),
                                          image, std::move(refCallback));
  if (!applied)
    return std::unexpected(std::move(applied.error()));
  return collected;
}

std::size_t DirectivesGroup::hash() const {
  XXHash3 hasher;
  for (auto& directives : m_directives) {
    std::size_t hash = directives.hash();
    hasher.push({reinterpret_cast<char const*>(&hash), sizeof(hash)});
  }

  return hasher.digest();
}

const List<Directives>& DirectivesGroup::list() const {
  return m_directives;
}

bool operator==(DirectivesGroup const& a, DirectivesGroup const& b) {
  return a.compare(b);
}

bool operator!=(DirectivesGroup const& a, DirectivesGroup const& b) {
  return !a.compare(b);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, DirectivesGroup& directivesGroup) {
  String string;
  auto read = ds.read(string);
  if (!read)
    return std::unexpected(std::move(read.error()));
  directivesGroup = DirectivesGroup(std::move(string));
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, DirectivesGroup const& directivesGroup) {
  auto written = ds.write(directivesGroup.toString());
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(ds);
}

std::size_t hash<DirectivesGroup>::operator()(DirectivesGroup const& s) const {
  return s.hash();
}

}