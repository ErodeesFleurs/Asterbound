# Utility

A few additional utiliy callbacks are added by OpenStarbound.

---

#### `Json` sb.buildInfo()

Returns the engine build's version and architecture strings as
`{version = String, architecture = String}`. These are build identifiers, not
runtime configuration values.

---

#### `Json` sb.parseJson(`String` json)

Parses a JSON string.
Equivalent to StarExtensions' `sb.jsonFromString`, which is also present for backwards compatibility.

---

#### `Json` sb.parseJsonSequence(`String` json)

Parses a space-separated sequence of JSON values in a string into an array of values. Useful for parsing command arguments.
Similar to `chat.parseArguments`.

---

#### `String` sb.formatTags(`String` text, `Map<String, String>` tags)

Strict native named-tag formatting. Replaces `<name>` with its supplied value;
`<<` produces a literal `<`. Values are copied verbatim, including embedded
NULs and tag-like text; they are not recursively expanded. Braces and percent
signs have no special meaning.

Unknown, empty and unterminated tags raise an error. Unlike `sb.replaceTags`,
this does not preserve unknown placeholders. It is not standard-format or
printf formatting.

---

#### `String` sb.stripEscapeCodes(`String` text)

Removes escape codes from the string.
If given `^blue;fish^reset;`, outputs `fish`.

---

#### `bool` sb.jsonEqual(`Json` a, `Json` b)

Checks if the two json values match.

---

#### `int` sb.millisecondsSinceEpoch()

Returns how many milliseconds have passed since the Unix epoch. (1970-01-01)
This can be used like other time values, but note that it is in milliseconds.
It is also useful for working with sound setClockStart and setClockStop, which have time values in milliseconds.

---

#### `int` sb.monotonicMilliseconds()

Returns monotonic milliseconds for elapsed-time animations. Unlike
`millisecondsSinceEpoch()`, this clock is not a wall-clock timestamp and must
not be stored as a portable date or compared across application runs.

---
#### `double` sb.monotonicTime()

Returns precise native steady-clock time in **seconds**, without the
millisecond rounding of `sb.monotonicMilliseconds()`. Use differences for
elapsed-time animation. It is not wall-clock time or a portable timestamp;
do not persist it for comparisons across application runs.

---


#### `pair<RpcPromise<Json>,RpcPromiseKeeper<Json>>` sb.makePromise()

Returns one pair table: `{promise, keeper}`. A message handler may return the promise to defer its reply. Fulfilling with `nil` is a successful JSON `null`, not a pending result.

Promises provide these methods:

| Method | Result |
| --- | --- |
| `status()` | `"pending"`, `"succeeded"`, `"failed"` or `"cancelled"` |
| `finished()` | Whether a terminal outcome has been published |
| `succeeded()` | Whether the outcome is successful |
| `failed()` | Whether the outcome is failed **or cancelled** |
| `cancelled()` | Whether the outcome is cancelled |
| `result()` | Successful value, otherwise `nil` |
| `error()` | Failure or cancellation reason, otherwise `nil` |
| `cancel()` | Cancels a pending observation; returns whether cancellation won |

Use `status()` or `succeeded()` to distinguish successful `null` from an unfinished result. A published terminal outcome cannot be overwritten by cancellation or a late producer.

Pending observations created or received during script-session execution belong to that session. Shutdown or reload cancels them and its pending deferred replies, without cancelling other sessions' observations of a shared native producer. Native hosts that create standalone Lua contexts must supply a lifetime to opt into this isolation; otherwise promises retain shared-reference semantics.

Cancellation does not undo a sent message or interrupt an already running native operation. It stops waiting for and publishing that observation's result.

The `RpcPromiseKeeper` has the following methods:

##### `void` fulfill(`Json` result)

Fulfills the promise as succeeded with the provided result.

##### `void` fail(`String` error)

Fails the promise with the provided error.

##### `void` chain(`RpcPromise<Json>` promise)

Delegates the outcome to the provided promise, including cancellation. Chaining assigns the keeper immediately, even while the delegated promise is pending.

##### `bool` cancel()

Cancels an unassigned producer; returns whether cancellation won. To cancel a pending observation after its keeper has chained another promise, use the promise's `cancel()` instead. Late producer decisions after cancellation are discarded; other duplicate producer decisions remain errors.

