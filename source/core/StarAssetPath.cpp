import std;
import star.memory;
import star.format;
import star.string;
import star.hash;
import star.optional;
import star.json;
import star.list;
import star.image_processing;
import star.data_stream;
import star.string_view;
import star.thread;
import star.directives;
import star.asset_path;
import star.lexical_cast;

namespace Star {

// The filename is everything after the last slash (excluding directives) and
// up to the first directive marker.
static std::optional<std::pair<std::size_t, std::size_t>> findFilenameRange(std::string const& pathUtf8) {
  std::size_t firstDirectiveOrSubPath = pathUtf8.find_first_of(":?");
  std::size_t filenameStart = 0;
  while (true) {
    std::size_t find = pathUtf8.find('/', filenameStart);
    if (find >= firstDirectiveOrSubPath)
      break;
    filenameStart = find + 1;
  }

  if (filenameStart == NPos) {
    return {};
  } else if (firstDirectiveOrSubPath == NPos) {
    return {{filenameStart, pathUtf8.size()}};
  } else {
    return {{filenameStart, firstDirectiveOrSubPath}};
  }
}

AssetPath AssetPath::split(String const& path) {
  AssetPath components;

  std::string const& str = path.utf8();

  //base paths cannot have any ':' or '?' characters, stop at the first one.
  std::size_t end = str.find_first_of(":?");
  components.basePath = str.substr(0, end);

  if (end == NPos)
    return components;

  // Sub-paths must immediately follow base paths and must start with a ':',
  // after this point any further ':' characters are not special.
  if (str[end] == ':') {
    std::size_t beg = end + 1;
    if (beg != str.size()) {
      end = str.find_first_of('?', beg);
      if (end == NPos && beg + 1 != str.size())
        components.subPath.emplace(str.substr(beg));
      else if (std::size_t len = end - beg)
        components.subPath.emplace(str.substr(beg, len));
    }
  }

  if (end == NPos)
    return components;

  // Directives must follow the base path and optional sub-path, and each
  // directive is separated by one or more '?' characters.
  if (str[end] == '?')
    components.directives = String(str.substr(end));

  return components;
}

String AssetPath::join(AssetPath const& components) {
  String result = components.basePath;
  if (components.subPath) {
    result.append(":");
    result.append(*components.subPath);
  }
  components.directives.forEach([&](Directives::Entry const& entry, Directives const& directives) {
    result.append("?");
    result += entry.string(*directives.m_shared);
  });
  return result;
}

String AssetPath::setSubPath(String const& path, String const& subPath) {
  auto components = split(path);
  components.subPath = subPath;
  return join(components);
}

String AssetPath::removeSubPath(String const& path) {
  auto components = split(path);
  components.subPath.reset();
  return join(components);
}

String AssetPath::getDirectives(String const& path) {
  std::size_t firstDirective = path.utf8().find('?');
  if (firstDirective == NPos)
    return {};
  return String(path.utf8().substr(firstDirective + 1));
}

String AssetPath::addDirectives(String const& path, String const& directives) {
  if (path.empty())
    return directives;
  if (directives.empty())
    return path;
  bool const leftJoined = path.utf8().back() == '?';
  bool const rightJoined = directives.utf8().front() == '?';
  String result = path;
  if (leftJoined && rightJoined) {
    result.append(std::string_view(directives.utf8()).substr(1));
  } else {
    if (!leftJoined && !rightJoined)
      result.append("?");
    result.append(directives);
  }
  return result;
}

String AssetPath::removeDirectives(String const& path) {
  std::size_t firstDirective = path.utf8().find('?');
  if (firstDirective == NPos)
    return path;
  return String(path.utf8().substr(0, firstDirective));
}

String AssetPath::directory(String const& path) {
  if (auto p = findFilenameRange(path.utf8())) {
    return String(path.utf8().substr(0, p->first));
  } else {
    return String();
  }
}

String AssetPath::filename(String const& path) {
  if (auto p = findFilenameRange(path.utf8())) {
    return String(path.utf8().substr(p->first, p->second));
  } else {
    return String();
  }
}

String AssetPath::extension(String const& path) {
  auto file = filename(path);
  auto lastDot = file.utf8().find_last_of('.');
  if (lastDot == NPos)
    return "";

  return String(file.utf8().substr(lastDot + 1));
}

String AssetPath::relative_to(String const& sourcePath, String const& givenPath) {
  if (!givenPath.empty() && givenPath.utf8()[0] == '/')
    return givenPath;

  auto path = directory(sourcePath);
  path.append(givenPath);
  return path;
}

bool AssetPath::operator==(AssetPath const& rhs) const {
  return std::tie(basePath, subPath, directives) == std::tie(rhs.basePath, rhs.subPath, rhs.directives);
}

AssetPath::AssetPath(std::string_view path) {
  *this = AssetPath::split(String(path));
}


AssetPath::AssetPath(String const& path) {
  *this = AssetPath::split(path);
}

AssetPath::AssetPath(AssetPath&& path)
  : basePath(std::move(path.basePath)),
    subPath(std::move(path.subPath)),
    directives((path.subPath.reset(), std::move(path.directives))) {}

AssetPath& AssetPath::operator=(AssetPath&& path) {
  if (this != &path) {
    basePath = std::move(path.basePath);
    if (path.subPath) {
      subPath.emplace(std::move(*path.subPath));
      path.subPath.reset();
    } else {
      subPath.reset();
    }
    directives = std::move(path.directives);
  }
  return *this;
}

AssetPath::AssetPath(String&& basePath, std::optional<String>&& subPath, DirectivesGroup&& directives) {
  this->basePath = std::move(basePath);
  if (subPath) {
    this->subPath.emplace(std::move(*subPath));
    subPath.reset();
  }
  this->directives = std::move(directives);
}

AssetPath::AssetPath(String const& basePath, std::optional<String> const& subPath, DirectivesGroup const& directives) {
  this->basePath = basePath;
  this->subPath = subPath;
  this->directives = directives;
}

std::ostream& operator<<(std::ostream& os, AssetPath const& rhs) {
  os << rhs.basePath;
  if (rhs.subPath) {
    os << ":";
    os << *rhs.subPath;
  }

  rhs.directives.forEach([&](Directives::Entry const& entry, Directives const& directives) {
    os << "?";
    os << entry.string(*directives.m_shared);
  });

  return os;
}

std::size_t hash<AssetPath>::operator()(AssetPath const& s) const {
  return hashOf(s.basePath, s.subPath, s.directives);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator>>(DataStream& ds, AssetPath& path) {
  String string;
  auto read = ds.read(string);
  if (!read)
    return std::unexpected(std::move(read.error()));
  path = std::move(string);
  return std::ref(ds);
}

std::expected<std::reference_wrapper<DataStream>, StarException> operator<<(DataStream& ds, AssetPath const& path) {
  auto written = ds.write(AssetPath::join(path));
  if (!written)
    return std::unexpected(std::move(written.error()));
  return std::ref(ds);
}

}
