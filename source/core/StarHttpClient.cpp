module;
import std;
#include <cpr/session.h>
#include <curl/curl.h>
import star.format;
import star.map;
import star.string;
import star.worker_pool;

module star.http_client;

namespace Star {
namespace {

using CurlUrl = std::unique_ptr<CURLU, decltype([](CURLU* handle) { curl_url_cleanup(handle); })>;
using CurlText = std::unique_ptr<char[], decltype([](char* text) { curl_free(text); })>;

// URL inspection can precede the first CPR session (and even the first
// worker). Initialize libcurl, including Winsock, before either use.
struct CurlRuntime {
  CURLcode status = curl_global_init(CURL_GLOBAL_DEFAULT);
  ~CurlRuntime() {
    if (status == CURLE_OK)
      curl_global_cleanup();
  }
};

std::expected<void, StarException> initialize_curl() {
  static CurlRuntime const runtime;
  if (runtime.status != CURLE_OK)
    return std::unexpected(StarException::format("Could not initialize libcurl (error {})", static_cast<int>(runtime.status)));
  return {};
}

char ascii_lower(char byte) {
  return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte + ('a' - 'A')) : byte;
}

bool ascii_alphanumeric(char byte) {
  return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9');
}

// Curl also accepts guessed schemes, nonstandard slash counts and scoped IPv6
// (whose zone is a separate CURLU part, not part of the host trust key).
// Require an unambiguous authority even for direct native submissions.
std::expected<void, StarException> check_url_spelling(std::string_view url) {
  if (std::ranges::any_of(url, [](unsigned char byte) { return byte <= ' ' || byte == 0x7f; }))
    return std::unexpected(StarException("HTTP URL contains whitespace or a control character"));
  auto separator = url.find("://");
  auto scheme = url.substr(0, separator);
  if (separator == url.npos || (!std::ranges::equal(scheme, std::string_view("http"), {}, ascii_lower)
                                && !std::ranges::equal(scheme, std::string_view("https"), {}, ascii_lower)))
    return std::unexpected(StarException("HTTP URL requires an explicit http:// or https:// scheme"));
  auto rest = url.substr(separator + 3);
  auto authority = rest.substr(0, rest.find_first_of("/?#"));
  if (authority.empty() || authority.find_first_of("@\\%") != authority.npos)
    return std::unexpected(StarException("HTTP URL requires a host without user information, escapes or a zone ID"));
  return {};
}

std::expected<CurlUrl, StarException> read_native_url(String const& url) {
  if (auto valid = check_url_spelling(url.utf8()); !valid)
    return std::unexpected(std::move(valid.error()));
  if (auto initialized = initialize_curl(); !initialized)
    return std::unexpected(std::move(initialized.error()));
  CurlUrl handle(curl_url());
  if (!handle)
    return std::unexpected(StarException("Could not allocate the HTTP URL parser"));
  if (auto code = curl_url_set(handle.get(), CURLUPART_URL, url.utf8_data(), CURLU_DISALLOW_USER); code != CURLUE_OK)
    return std::unexpected(StarException::format("Invalid HTTP URL: {}", curl_url_strerror(code)));
  return handle;
}

std::expected<CurlText, StarException> get_url_part(CurlUrl const& url, CURLUPart part, unsigned flags) {
  CurlText value;
  if (auto code = curl_url_get(url.get(), part, std::out_ptr(value), flags); code != CURLUE_OK)
    return std::unexpected(StarException::format("Invalid HTTP URL: {}", curl_url_strerror(code)));
  return value;
}

std::expected<String, StarException> canonical_host(CurlUrl const& url) {
  auto host = get_url_part(url, CURLUPART_HOST, CURLU_PUNYCODE);
  if (!host)
    return std::unexpected(std::move(host.error()));
  std::string name(host->get());
  // The key is also shown in styled game text. Only DNS/IDNA labels and
  // curl-validated bracketed IP literals may reach that display.
  for (char& byte : name) {
    if (!ascii_alphanumeric(byte) && !std::string_view(".-_[]:").contains(byte))
      return std::unexpected(StarException("Unsafe character in HTTP host name"));
    byte = ascii_lower(byte);
  }
  return String(std::move(name));
}

std::expected<HttpResponse, StarException> take_response(cpr::Response response) {
  if (response.error.code != cpr::ErrorCode::OK)
    return std::unexpected(StarException::format("HTTP error: {}", response.error.message));
  return HttpResponse{static_cast<int>(response.status_code), String(std::move(response.text))};
}

std::expected<HttpResponse, StarException> perform_request(HttpRequest& request, std::atomic<bool> const& stopped) {
  if (stopped.load(std::memory_order_relaxed))
    return std::unexpected(StarException("HTTP client stopped"));
  cpr::Session session;
  session.SetUrl(cpr::Url{std::move(request.url).utf8()});
  session.SetRedirect(cpr::Redirect{false});
  session.SetTimeout(cpr::Timeout{std::chrono::seconds(request.timeout)});
  session.SetProgressCallback(cpr::ProgressCallback{[&stopped](cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t,
      cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, std::intptr_t) { return !stopped.load(std::memory_order_relaxed); }});
  // Our result has no header map. Don't accumulate and parse unused headers.
  session.SetHeaderCallback(cpr::HeaderCallback{[](std::string_view, std::intptr_t) { return true; }});
  for (auto&& [name, value] : request.headers)
    session.GetHeader().insert_or_assign(name.utf8(), std::move(value).utf8());
  if (request.method == HttpMethod::Post || request.method == HttpMethod::Put || request.method == HttpMethod::Patch)
    session.SetBodyView(cpr::BodyView{std::string_view(request.body.utf8())});
  switch (request.method) {
    case HttpMethod::Get: return take_response(session.Get());
    case HttpMethod::Delete: return take_response(session.Delete());
    case HttpMethod::Post: return take_response(session.Post());
    case HttpMethod::Put: return take_response(session.Put());
    case HttpMethod::Patch: return take_response(session.Patch());
  }
  return std::unexpected(StarException("Unsupported HTTP method"));
}

}

std::expected<HttpMethod, StarException> parse_http_method(String const& method) {
  if (method == "GET") return HttpMethod::Get;
  if (method == "POST") return HttpMethod::Post;
  if (method == "PUT") return HttpMethod::Put;
  if (method == "DELETE") return HttpMethod::Delete;
  if (method == "PATCH") return HttpMethod::Patch;
  return std::unexpected(StarException::format("Unsupported HTTP method: {}", method.utf8()));
}

std::expected<HttpUrl, StarException> parse_http_url(String const& url) {
  auto handle = read_native_url(url);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  auto host = canonical_host(*handle);
  if (!host)
    return std::unexpected(std::move(host.error()));
  // Use the same canonical host spelling in the approved URL and in the key.
  if (auto code = curl_url_set(handle->get(), CURLUPART_HOST, host->utf8_data(), 0); code != CURLUE_OK)
    return std::unexpected(StarException::format("Invalid HTTP host: {}", curl_url_strerror(code)));
  auto target = get_url_part(*handle, CURLUPART_URL, CURLU_GET_EMPTY);
  if (!target)
    return std::unexpected(std::move(target.error()));
  return HttpUrl{String(std::string_view(target->get())), std::move(*host)};
}

std::expected<String, StarException> normalize_http_host(String const& host) {
  std::string_view name = host.utf8();
  if (name.find_first_of("/?#") != name.npos || (name.contains(':') && !(name.starts_with('[') && name.ends_with(']'))))
    return std::unexpected(StarException("HTTP trust requires a bare host without a port"));
  auto handle = read_native_url("http://" + host);
  if (!handle)
    return std::unexpected(std::move(handle.error()));
  return canonical_host(*handle);
}

std::expected<void, StarException> validate_http_timeout(int timeout) {
  if (timeout < 0 || timeout > std::numeric_limits<int>::max() / 1000)
    return std::unexpected(StarException("HTTP error: Request timeout is outside the supported range"));
  return {};
}

std::expected<void, StarException> validate_http_headers(StringMap<String> const& headers) {
  for (auto const& [name, value] : headers) {
    if (name.empty() || !std::ranges::all_of(name.utf8(), [](char byte) {
          return ascii_alphanumeric(byte) || std::string_view("!#$%&'*+-.^_`|~").contains(byte);
        }))
      return std::unexpected(StarException("Invalid HTTP header name"));
    if (std::ranges::equal(name.utf8(), std::string_view("host"), {}, ascii_lower))
      return std::unexpected(StarException("Cannot override the HTTP Host header"));
    // HTTP field values permit horizontal tabs, but never line breaks or
    // embedded NULs that could change the field seen by libcurl.
    if (std::ranges::any_of(value.utf8(), [](unsigned char byte) {
          return (byte < ' ' && byte != '\t') || byte == 0x7f;
        }))
      return std::unexpected(StarException("Invalid control character in HTTP header value"));
  }
  return {};
}

HttpClient::~HttpClient() {
  m_stopped.store(true, std::memory_order_relaxed);
  // WorkerPool's destructor joins before m_stopped is destroyed.
}

std::expected<void, StarException> HttpClient::stop() {
  // Only submitters and other stoppers need this lock, never a progress callback.
  std::lock_guard lock(m_mutex);
  m_stopped.store(true, std::memory_order_relaxed);
  if (m_pool) {
    if (auto joined = m_pool->stop(); !joined)
      return joined;
    m_pool.reset();
  }
  return {};
}

std::expected<WorkerPoolPromise<HttpResponse>, StarException> HttpClient::request(HttpRequest request) {
  if (auto valid = validate_http_timeout(request.timeout); !valid)
    return std::unexpected(std::move(valid.error()));
  if (auto valid = check_url_spelling(request.url.utf8()); !valid)
    return std::unexpected(std::move(valid.error()));
  if (auto valid = validate_http_headers(request.headers); !valid)
    return std::unexpected(std::move(valid.error()));
  std::lock_guard lock(m_mutex);
  if (m_stopped.load(std::memory_order_relaxed))
    return std::unexpected(StarException("HTTP client stopped"));
  if (!m_pool) {
    if (auto initialized = initialize_curl(); !initialized)
      return std::unexpected(std::move(initialized.error()));
    m_pool.emplace("HttpClient");
    if (auto started = m_pool->start(4); !started) {
      m_pool.reset();
      return std::unexpected(std::move(started.error()));
    }
  }
  return m_pool->add_producer<HttpResponse>([request = std::move(request), &stopped = m_stopped]() mutable {
    return perform_request(request, stopped);
  });
}

}
