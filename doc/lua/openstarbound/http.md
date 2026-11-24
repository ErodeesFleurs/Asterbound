# http

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `http` table provides asynchronous HTTP request functionality for Lua scripts. All HTTP requests are executed
asynchronously and return promises that can be checked for completion.

Only absolute HTTP and HTTPS URLs are accepted. Trust applies to the canonical host, independent of port.
Redirects are not followed: a 3xx response belongs to the URL the script actually requested.

HTTP promises use the standard promise methods: `status()`, `finished()`, `succeeded()`, `failed()`,
`cancelled()`, `cancel()`, `result()`, and `error()`. `status()` returns `"pending"`, `"succeeded"`,
`"failed"`, or `"cancelled"`. A cancelled promise is finished and failed, with no result and a cancellation
reason returned by `error()`. `cancel()` returns `true` only when it cancels a pending promise.

Cancelling a request removes it from pending trust approval or active result tracking without cancelling
other requests. Closing the owning Lua session also cancels its outstanding requests, even when the
script discarded the promise. A late trust approval cannot submit a cancelled pending request. An already
running native HTTP operation may still finish; cancellation discards its result rather than promising
transport interruption or rolling back server-side effects. When the client HTTP session shuts down, it
also requests interruption of native transfers before joining its workers. This is not an instant deadline.

Successful responses contain `{status = <HTTP status code>, body = <response string>}`. HTTP error status
codes such as 404 or 503 are still successful HTTP responses; trust denial and transport errors fail the
promise. Empty bodies and the literal text `"null"` remain successful string results.

---

#### `bool` http.available()

Returns `true` if HTTP functionality is enabled in the configuration, `false` otherwise.

---

#### `HttpPromise` http.get(`String` url, [`Table` options])

Creates and executes an asynchronous HTTP GET request to the specified URL.

**Options table fields:**

* `headers` - `Table<String, String>` - Optional. Header names must be HTTP tokens and values must not
  contain NUL or line breaks. The transport sets `Host` from the approved URL; scripts cannot override it.
* `timeout` - `int` - Optional. Request timeout in seconds (default 30; 0 disables the timeout)
* `body` - `String` or `Table` - Optional. Sent with POST, PUT, and PATCH. GET and DELETE send no body.

Returns an `HttpPromise` object that can be checked for completion.

**Example:**

```lua
local promise = http.get("https://example.com/api/data", {
  headers = { ["User-Agent"] = "MyScript/1.0" },
  timeout = 50
})
```

---

#### `HttpPromise` http.post(`String` url, [`Table` options])

Creates and executes an asynchronous HTTP POST request to the specified URL.

Returns an `HttpPromise` object that can be checked for completion.

---

#### `HttpPromise` http.put(`String` url, [`Table` options])

Creates and executes an asynchronous HTTP PUT request to the specified URL.

Returns an `HttpPromise` object that can be checked for completion.

---

#### `HttpPromise` http.delete(`String` url, [`Table` options])

Creates and executes an asynchronous HTTP DELETE request to the specified URL.


Returns an `HttpPromise` object that can be checked for completion.

---

#### `HttpPromise` http.patch(`String` url, [`Table` options])

Creates and executes an asynchronous HTTP PATCH request to the specified URL.

Returns an `HttpPromise` object that can be checked for completion.

---

#### `HttpPromise` http.createRequest(`String` method, `String` url, [`Table` options])

Creates an asynchronous request. Supported methods are `GET`, `POST`, `PUT`, `DELETE`, and `PATCH`
(uppercase). An unsupported method or out-of-range timeout fails the promise without asking for trust.

Returns an `HttpPromise` object that can be checked for completion.

**Example:**

```lua
local promise = http.createRequest("PUT", "https://example.com/resource")
```

---

#### `bool` http.isTrusted(`String` domain)

Returns `true` if the specified domain is in the trusted domains list, `false` otherwise.

**Example:**

```lua
if http.isTrusted("example.com") then
  sb.logInfo("example.com is trusted")
end
```

---

## Security and Trust

HTTP requests require domain trust for security. When a script attempts to make a request to an untrusted domain:

1. If the trust dialog is unavailable or its layout cannot present the approval controls, the promise fails without contacting the host. The client then processes the next host.
2. Otherwise, the client shows one host at a time, in order. Requests waiting for the same host share a decision.
3. Allowing a host submits its waiting requests. “Remember” starts unchecked for each approval. If saving a requested trust decision fails, the affected requests fail rather than proceeding without it.
4. Denying or closing the dialog fails the waiting requests. Cancelling the last request for a host retires
   that approval; an old dialog cannot approve a later request, even for the same host.

Trusted domains are stored in the configuration under `safe.luaHttp.trustedSites`.

---

## Complete Example

### PromiseKeeper Example

```lua
require("/scripts/messageutil.lua")

function init()
    if not http.available() then
        sb.logError("HTTP is not enabled!")
        script.setUpdateDelta(0)
    else
        promises:add(http.get("https://api.example.com/data", {
            headers = {
                ["User-Agent"] = "Starbound-Script/1.0",
                ["Accept"] = "application/json"
            },
            timeout = 10 -- seconds
        }),
                handleSuccess,
                handleError
        )
    end
end

function handleSuccess(response)
    if response.status == 200 then
        sb.logInfo("Success! Body: %s", response.body)
        -- Process JSON or other data
    else
        sb.logWarn("Request returned status %s", response.status)
    end
end

function handleError(errorMessage)
    sb.logError("Request failed: %s", sb.print(errorMessage))
end

function update(...)
    promises:update()
end
```

