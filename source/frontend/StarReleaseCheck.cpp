module;
import std;
import star.format;
import star.http_client;
import star.json;
import star.map;
import star.string;
import star.version;
import star.worker_pool;

module star.release_check;

namespace Star {
namespace {

using Version = std::array<std::uint16_t, 4>;

// Keep the same component domain as cmake/AsterboundVersion.cmake.
constexpr std::optional<Version> parse_version(std::string_view text) {
  Version result{};
  for (std::size_t i = 0; i != result.size(); ++i) {
    if (i != 0) {
      if (!text.starts_with('.'))
        return {};
      text.remove_prefix(1);
    }
    if (text.empty())
      return {};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result[i]);
    if (error != std::errc{} || (text.front() == '0' && end != text.data() + 1))
      return {};
    text.remove_prefix(end - text.data());
  }
  return text.empty() ? std::optional(result) : std::nullopt;
}

constexpr std::string_view version_from_tag(std::string_view tag) {
  return tag.starts_with('v') ? tag.substr(1) : tag;
}

static_assert(*parse_version(version_from_tag("v0.10.0.1")) > *parse_version("0.9.65535.65535"));
static_assert(*parse_version("1.2.3.10") > *parse_version("1.2.3.9"));
static_assert(!parse_version("1.2.3") && !parse_version("1.2.3.4.5") && !parse_version("1..3.4"));
static_assert(!parse_version("0.01.0.0") && !parse_version("1.2.3.65536") && !parse_version("1.2.3.+4"));
static_assert(!parse_version(" 1.2.3.4") && !parse_version("1.2.3.4-rc1") && !parse_version("v1.2.3.4"));

ReleaseCheck::State classify(HttpResponse const& response, String& latest) {
  // The release document includes asset descriptions. Keep title-thread JSON
  // work bounded even if an unexpected response is returned by the service.
  if (response.status_code != 200 || response.body.utf8().size() > 1024 * 1024)
    return ReleaseCheck::State::Failed;
  auto release = Json::parse(response.body);
  if (!release || release->get_bool("draft").value_or(true) || release->get_bool("prerelease").value_or(true))
    return ReleaseCheck::State::Failed;
  auto tag = release->get("tag_name").and_then([](Json const& value) { return value.string_ptr(); });
  if (!tag)
    return ReleaseCheck::State::Failed;
  auto digits = version_from_tag((*tag)->utf8());
  auto remote = parse_version(digits);
  auto installed = parse_version(product_version);
  if (!remote || !installed)
    return ReleaseCheck::State::Failed;
  latest = String(digits);
  return *remote > *installed ? ReleaseCheck::State::Available : ReleaseCheck::State::Current;
}

}

void ReleaseCheck::start() {
  if (state_ == State::Checking)
    return;
  latest_.clear();
  state_ = State::Checking;
  HttpRequest request;
  request.url = "https://api.github.com/repos/ErodeesFleurs/Asterbound/releases/latest";
  request.headers = {{"Accept", "application/vnd.github+json"}, {"User-Agent", "Asterbound"}};
  request.timeout = 8;
  auto submitted = client_.emplace().request(std::move(request));
  if (submitted)
    pending_ = std::move(*submitted);
}

bool ReleaseCheck::poll() {
  if (state_ != State::Checking || (pending_ && !pending_->done()))
    return false;
  state_ = State::Failed;
  if (pending_) {
    auto response = pending_->get();
    if (response)
      state_ = classify(response->get(), latest_);
    pending_.reset();
  }
  // Don't leave dedicated HTTP workers alive between checks.
  client_.reset();
  return true;
}

}
