module;
import std;
import star.string;
import star.map;
import star.thread;
import star.worker_pool;

import star.format;
#include <cpr/cpr.h>

export module star.http_client;

export namespace Star {

struct HttpRequest {
  String method;
  String url;
  StringMap<String> headers;
  String body;

  int timeout = 30; // 0 - no timeout. NOT RECOMENDED YOU HEAR ME??
};

struct HttpResponse {
  int statusCode = 0;
  StringMap<String> headers;
  String body;
  String error;
};

class HttpClient {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<HttpClient>, StarException> create();
  ~HttpClient();

  HttpClient(HttpClient const&) = delete;
  HttpClient& operator=(HttpClient const&) = delete;
  HttpClient(HttpClient&&) = delete;
  HttpClient& operator=(HttpClient&&) = delete;

  // Cancels queued requests and joins in-flight blocking requests.
  [[nodiscard]] std::expected<void, StarException> stop();

  WorkerPoolPromise<HttpResponse> requestAsync(HttpRequest const& request);

  WorkerPoolPromise<HttpResponse> getAsync(String const& url, StringMap<String> const& headers = {});
  WorkerPoolPromise<HttpResponse> postAsync(String const& url, String const& body, StringMap<String> const& headers = {});
  WorkerPoolPromise<HttpResponse> putAsync(String const& url, String const& body, StringMap<String> const& headers = {});
  WorkerPoolPromise<HttpResponse> deleteAsync(String const& url, StringMap<String> const& headers = {});
  WorkerPoolPromise<HttpResponse> patchAsync(String const& url, String const& body, StringMap<String> const& headers = {});

private:
  explicit HttpClient(std::unique_ptr<WorkerPool> workerPool);

  // Blocking network I/O must not occupy the CPU/render executor.
  std::unique_ptr<WorkerPool> m_workerPool;
};

}

namespace Star {

[[nodiscard]] std::expected<std::unique_ptr<HttpClient>, StarException> HttpClient::create() {
  auto workerPool = WorkerPool::create("HttpClient", 4);
  if (!workerPool)
    return std::unexpected(std::move(workerPool.error()));
  return std::unique_ptr<HttpClient>(new HttpClient(std::move(*workerPool)));
}

HttpClient::HttpClient(std::unique_ptr<WorkerPool> workerPool)
  : m_workerPool(std::move(workerPool)) {}

HttpClient::~HttpClient() = default;

[[nodiscard]] std::expected<void, StarException> HttpClient::stop() {
  return m_workerPool->stop();
}

[[nodiscard]] static std::expected<HttpResponse, StarException> performRequest(HttpRequest const& req) {
  HttpResponse response;

  // CPR reports libcurl transport failures in Response::error, independently
  // of HTTP status codes (including 4xx/5xx responses).
  cpr::Header cprHeaders;
  for (const auto& [fst, snd] : req.headers)
    cprHeaders[fst.utf8()] = snd.utf8();

  auto milliseconds = static_cast<std::int64_t>(req.timeout) * 1000;
  if (milliseconds < 0 || milliseconds > std::numeric_limits<int>::max()) {
    response.error = "HTTP error: Request timeout is outside the supported range";
    return response;
  }
  cpr::Timeout timeout{std::chrono::milliseconds(milliseconds)};

  cpr::Response r;
  if (req.method == "GET") {
    r = cpr::Get(
      cpr::Url{req.url.utf8()},
      cprHeaders,
      timeout
    );
  } else if (req.method == "POST") {
    r = cpr::Post(
      cpr::Url{req.url.utf8()},
      cprHeaders,
      cpr::Body{req.body.utf8()},
      timeout
    );
  } else if (req.method == "PUT") {
    r = cpr::Put(
      cpr::Url{req.url.utf8()},
      cprHeaders,
      cpr::Body{req.body.utf8()},
      timeout
    );
  } else if (req.method == "DELETE") {
    r = cpr::Delete(
      cpr::Url{req.url.utf8()},
      cprHeaders,
      timeout
    );
  } else if (req.method == "PATCH") {
    r = cpr::Patch(
      cpr::Url{req.url.utf8()},
      cprHeaders,
      cpr::Body{req.body.utf8()},
      timeout
    );
  } else {
    response.error = strf("Unsupported HTTP method: {}", req.method);
    return response;
  }

  if (r.error.code != cpr::ErrorCode::OK) {
    response.error = strf("HTTP error: {}", r.error.message);
    return response;
  }

  response.statusCode = static_cast<int>(r.status_code);
  response.body = String(r.text);

  for (auto const& pair : r.header)
    response.headers[String(pair.first)] = String(pair.second);

  return response;
}

WorkerPoolPromise<HttpResponse> HttpClient::requestAsync(HttpRequest const& request) {
  return m_workerPool->addProducer<HttpResponse>([request]() -> std::expected<HttpResponse, StarException> {
    return performRequest(request);
  });
}

WorkerPoolPromise<HttpResponse> HttpClient::getAsync(String const& url, StringMap<String> const& headers) {
  return requestAsync({.method = "GET", .url = url, .headers = headers, .body = {}});
}

WorkerPoolPromise<HttpResponse> HttpClient::postAsync(String const& url, String const& body, StringMap<String> const& headers) {
  return requestAsync({.method = "POST", .url = url, .headers = headers, .body = body});
}

WorkerPoolPromise<HttpResponse> HttpClient::putAsync(String const& url, String const& body, StringMap<String> const& headers) {
  return requestAsync({.method = "PUT", .url = url, .headers = headers, .body = body});
}

WorkerPoolPromise<HttpResponse> HttpClient::deleteAsync(String const& url, StringMap<String> const& headers) {
  return requestAsync({.method = "DELETE", .url = url, .headers = headers, .body = {}});
}

WorkerPoolPromise<HttpResponse> HttpClient::patchAsync(String const& url, String const& body, StringMap<String> const& headers) {
  return requestAsync({.method = "PATCH", .url = url, .headers = headers, .body = body});
}

}
