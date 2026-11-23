import std;
import star.text_formatter;
import star.format;
import star.string;
import star.hash;
import star.data_stream;
import star.string_view;
import star.directives;
import star.asset_path;

namespace Star {
namespace {
struct PathParts {
  std::string_view base_path;
  std::string_view sub_path;
  std::string_view directives;
};

PathParts split_path(std::string_view path) {
  auto boundary = path.find_first_of(":?");
  PathParts parts{path.substr(0, boundary), {}, {}};
  if (boundary == std::string_view::npos)
    return parts;
  if (path[boundary] == ':') {
    auto begin = boundary + 1;
    auto end = path.find('?', begin);
    parts.sub_path = path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
    boundary = end;
  }
  if (boundary != std::string_view::npos)
    parts.directives = path.substr(boundary);
  return parts;
}

std::string_view base_path(std::string_view path) { return path.substr(0, path.find_first_of(":?")); }

std::string_view filename_view(std::string_view path) {
  auto base = base_path(path);
  auto slash = base.rfind('/');
  return slash == std::string_view::npos ? base : base.substr(slash + 1);
}

std::string_view directory_view(std::string_view path) {
  auto base = base_path(path);
  auto slash = base.rfind('/');
  return slash == std::string_view::npos ? std::string_view{} : base.substr(0, slash + 1);
}

template <typename Output>
concept asset_path_output = requires(Output& output, std::string_view bytes) {
  { output.append(bytes) } -> std::same_as<void>;
};

template <asset_path_output Output> void append_suffix(Output& output, AssetPath const& path) {
  if (path.sub_path) {
    output.append(":");
    output.append(path.sub_path->utf8());
  }
  path.directives.for_each([&](Directives::Entry const& entry, Directives const& directives) {
    output.append("?");
    output.append(directives.entry_string(entry).utf8());
  });
}

template <typename Visitor>
concept asset_directive_visitor =
  std::invocable<Visitor&, std::string_view> && std::same_as<std::invoke_result_t<Visitor&, std::string_view>, void>;

template <asset_directive_visitor Visitor> void for_each_directive(std::string_view directives, Visitor&& visitor) {
  while (!directives.empty()) {
    auto end = directives.find('?');
    auto entry = directives.substr(0, end);
    if (!entry.empty())
      std::invoke(visitor, entry);
    if (end == std::string_view::npos)
      break;
    directives.remove_prefix(end + 1);
  }
}

void append_normalized_directives(String& output, std::string_view directives) {
  for_each_directive(directives, [&](std::string_view entry) {
    output.append("?");
    output.append(entry);
  });
}

String with_sub_path(std::string_view path, std::optional<std::string_view> sub_path) {
  auto parts = split_path(path);
  String result;
  auto size = parts.base_path.size() + (sub_path ? 1 + sub_path->size() : 0);
  for_each_directive(parts.directives, [&](std::string_view entry) { size += 1 + entry.size(); });
  result.reserve(size);
  result.append(parts.base_path);
  if (sub_path) {
    result.append(":");
    result.append(*sub_path);
  }
  append_normalized_directives(result, parts.directives);
  return result;
}
}// namespace

AssetPath AssetPath::split(String const& path) { return AssetPath(std::string_view(path.utf8())); }

String AssetPath::join(AssetPath const& path) {
  String result = path.base_path;
  append_suffix(result, path);
  return result;
}

bool AssetPath::is_embedded(String const& path) { return path.utf8().starts_with("/opensb_data;"); }

bool AssetPath::is_remote(String const& path) { return is_embedded(path); }

String AssetPath::set_sub_path(String const& path, String const& sub_path) {
  return with_sub_path(path.utf8(), std::string_view(sub_path.utf8()));
}

String AssetPath::remove_sub_path(String const& path) { return with_sub_path(path.utf8(), std::nullopt); }

String AssetPath::get_directives(String const& path) {
  auto bytes = std::string_view(path.utf8());
  auto boundary = bytes.find('?');
  return boundary == std::string_view::npos ? String{} : String(bytes.substr(boundary + 1));
}

String AssetPath::add_directives(String const& path, String const& directives) {
  if (path.empty())
    return directives;
  if (directives.empty())
    return path;
  bool left_joined = path.utf8().back() == '?';
  bool right_joined = directives.utf8().front() == '?';
  String result = path;
  if (left_joined && right_joined) {
    result.append(std::string_view(directives.utf8()).substr(1));
  } else {
    if (!left_joined && !right_joined)
      result.append("?");
    result.append(directives);
  }
  return result;
}

String AssetPath::remove_directives(String const& path) {
  auto bytes = std::string_view(path.utf8());
  return String(bytes.substr(0, bytes.find('?')));
}

String AssetPath::directory(String const& path) { return String(directory_view(path.utf8())); }

String AssetPath::filename(String const& path) { return String(filename_view(path.utf8())); }

String AssetPath::extension(String const& path) {
  auto file = filename_view(path.utf8());
  auto dot = file.rfind('.');
  return dot == std::string_view::npos ? String{} : String(file.substr(dot + 1));
}

String AssetPath::relative_to(String const& source_path, String const& given_path) {
  auto given = std::string_view(given_path.utf8());
  if (given.starts_with('/'))
    return given_path;
  auto directory = directory_view(source_path.utf8());
  String result;
  result.reserve(directory.size() + given.size());
  result.append(directory);
  result.append(given);
  return result;
}

bool AssetPath::operator==(AssetPath const& other) const {
  return std::tie(base_path, sub_path, directives)
    == std::tie(other.base_path, other.sub_path, other.directives);
}

AssetPath::AssetPath(std::string_view path) {
  auto parts = split_path(path);
  base_path = String(parts.base_path);
  if (!parts.sub_path.empty())
    sub_path.emplace(parts.sub_path);
  if (!parts.directives.empty())
    directives = String(parts.directives);
}

AssetPath::AssetPath(String const& path) : AssetPath(std::string_view(path.utf8())) {}

AssetPath::AssetPath(AssetPath&& path) noexcept
    : AssetPath(std::move(path.base_path), std::move(path.sub_path), std::move(path.directives)) {}

AssetPath& AssetPath::operator=(AssetPath&& path) noexcept {
  if (this != &path) {
    base_path = std::move(path.base_path);
    sub_path = std::exchange(path.sub_path, std::nullopt);
    directives = std::move(path.directives);
  }
  return *this;
}

AssetPath::AssetPath(String&& base_path, std::optional<String>&& sub_path, DirectivesGroup&& directives) noexcept
    : base_path(std::move(base_path)), sub_path(std::exchange(sub_path, std::nullopt)),
      directives(std::move(directives)) {}

AssetPath::AssetPath(String const& base_path, std::optional<String> const& sub_path, DirectivesGroup const& directives)
    : base_path(base_path), sub_path(sub_path), directives(directives) {}

void write_text(TextBuffer& buffer, AssetPath const& path) {
  buffer.append(path.base_path.utf8());
  append_suffix(buffer, path);
}

std::size_t ValueHash<AssetPath>::operator()(AssetPath const& path) const {
  return hash_of(path.base_path, path.sub_path, path.directives);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& stream, AssetPath& path) {
  String bytes;
  auto read = stream.read(bytes);
  if (!read)
    return std::unexpected(std::move(read.error()));
  path = AssetPath::split(bytes);
  return std::ref(stream);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& stream, AssetPath const& path) {
  auto written = stream.write(AssetPath::join(path));
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(stream);
}
}// namespace Star
