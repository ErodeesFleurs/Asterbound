module;
import std;
import star.string;
import star.map;
import star.worker_pool;
import star.format;

export module star.http_client;

export namespace Star {

enum class HttpMethod { Get, Post, Put, Delete, Patch };

struct HttpUrl {
  String url;
  String host;
};

[[nodiscard]] std::expected<HttpMethod, StarException> parse_http_method(String const& method);
[[nodiscard]] std::expected<HttpUrl, StarException> parse_http_url(String const& url);
[[nodiscard]] std::expected<String, StarException> normalize_http_host(String const& host);
[[nodiscard]] std::expected<void, StarException> validate_http_timeout(int timeout);
[[nodiscard]] std::expected<void, StarException> validate_http_headers(StringMap<String> const& headers);

struct HttpRequest {
  HttpMethod method = HttpMethod::Get;
  String url;
  StringMap<String> headers;
  String body;
  int timeout = 30; // Seconds; zero disables the transfer deadline.
};

struct HttpResponse {
  int status_code;
  String body;
};

class HttpClient {
public:
  HttpClient() = default;
  ~HttpClient();

  HttpClient(HttpClient const&) = delete;
  HttpClient& operator=(HttpClient const&) = delete;
  HttpClient(HttpClient&&) = delete;
  HttpClient& operator=(HttpClient&&) = delete;

  // Stops submissions, aborts active transfers and joins the dedicated workers.
  [[nodiscard]] std::expected<void, StarException> stop();

  [[nodiscard]] std::expected<WorkerPoolPromise<HttpResponse>, StarException> request(HttpRequest request);

private:
  std::mutex m_mutex;
  std::atomic<bool> m_stopped{false};
  // Destroyed before the stop flag borrowed by worker progress callbacks.
  std::optional<WorkerPool> m_pool;
};

}
