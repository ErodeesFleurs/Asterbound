---@meta

--- HTTP API for asynchronous HTTP requests
---@class http
http = {}

---@class HttpOptions
---@field headers table<string, string>|nil Optional. HTTP headers to include in the request
---@field body string|table|nil Optional. Request body; tables are JSON-encoded
---@field timeout integer|nil Optional. Request timeout in seconds (default 30; 0 disables the timeout)

---@class HttpResponse
---@field status number HTTP status code (e.g., 200, 404, 500)
---@field body string Response body content

---@class HttpPromise
---@field status fun(self: HttpPromise): "pending"|"succeeded"|"failed"|"cancelled" Returns the standard promise state
---@field finished fun(self: HttpPromise): boolean Returns `true` after success, failure, or cancellation
---@field succeeded fun(self: HttpPromise): boolean Returns `true` for a completed HTTP response, including HTTP 4xx/5xx
---@field failed fun(self: HttpPromise): boolean Returns `true` after failure, denial, or cancellation
---@field cancelled fun(self: HttpPromise): boolean Returns `true` if this request was cancelled
---@field cancel fun(self: HttpPromise): boolean Cancels a pending request without affecting other requests; a running native operation may still finish
---@field result fun(self: HttpPromise): HttpResponse|nil Returns the successful response, preserving empty and literal "null" body strings
---@field error fun(self: HttpPromise): string|nil Returns the failure or cancellation reason

--- Owning Lua-session closure cancels outstanding requests, including discarded promises.
--- Late approval cannot submit a cancelled trust request. Approval persists domain trust only when remembered.

--- Returns `true` if HTTP functionality is enabled in the configuration, `false` otherwise. ---
---@return boolean
function http.available() end

--- Creates and executes an asynchronous HTTP GET request to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.get(url, options) end

--- Creates and executes an asynchronous HTTP POST request to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.post(url, options) end

--- Creates and executes an asynchronous HTTP PUT request to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.put(url, options) end

--- Creates and executes an asynchronous HTTP DELETE request to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.delete(url, options) end

--- Creates and executes an asynchronous HTTP PATCH request to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.patch(url, options) end

--- Creates and executes an asynchronous HTTP request with a custom method to the specified URL. Returns an `HttpPromise` object that can be checked for completion. ---
---@param method string HTTP method (e.g., "GET", "POST", "HEAD", "OPTIONS")
---@param url string Target URL
---@param options HttpOptions|nil Optional request options (headers, body, timeout)
---@return HttpPromise
function http.createRequest(method, url, options) end

--- Returns `true` if the specified domain is in the trusted domains list, `false` otherwise. ---
---@param domain string Domain to check (e.g., "example.com")
---@return boolean
function http.isTrusted(domain) end

