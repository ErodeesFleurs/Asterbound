---@meta

--- sb API
---@class sb
sb = {}

--- Returns the engine build's version and architecture strings.
---@return {version: string, architecture: string}
function sb.buildInfo() end

--- Returns a randomized value with a normal distribution using the specified standard deviation (default is 1.0) and mean (default is 0). ---
---@param standardDeviation? number
---@param mean? number
---@return number
function sb.nrand(standardDeviation, mean) end

--- Returns a `String` representation of a new, randomly-created `Uuid`. ---
---@return string
function sb.makeUuid() end

--- Logs the specified formatted string, optionally using the formatted replacement values, to the log file and console with the Info log level. ---
---@param formatString string
---@param ... LuaValue
function sb.logInfo(formatString, ...) end

--- Logs the specified formatted string, optionally using the formatted replacement values, to the log file and console with the Warn log level. ---
---@param formatString string
---@param ... LuaValue
function sb.logWarn(formatString, ...) end

--- Logs the specified formatted string, optionally using the formatted replacement values, to the log file and console with the Error log level. ---
---@param formatString string
---@param ... LuaValue
function sb.logError(formatString, ...) end

--- Sets an entry in the debug log map (visible while in debug mode) using the specified format string and optional formatted replacement values. ---
---@param key string
---@param formatString string
---@param ... LuaValue
function sb.setLogMap(key, formatString, ...) end

--- Returns a human-readable string representation of the specified JSON value. If pretty is `true`, objects and arrays will have whitespace added for readability. ---
---@param value Json
---@param pretty? integer
---@return string
function sb.printJson(value, pretty) end

--- Returns a human-readable string representation of the specified `LuaValue`. ---
---@param value LuaValue
---@return string
function sb.print(value) end

--- Returns an interpolated `Vec2F` or `double` between the two specified values using a sin ease function. ---
---@param offset number
---@param value1 number
---@param value2 number
---@overload fun(offset: number, value1: Vec2F, value2: Vec2F): Vec2F
---@return number
function sb.interpolateSinEase(offset, value1, value2) end

--- Replaces all tags in the specified string with the specified tag replacement values. ---
---@param string string
---@param tags table<string, string>
---@return string
function sb.replaceTags(string, tags) end

--- Strict native <name> substitution; << escapes <. Unknown/empty/unterminated tags raise errors.
--- Replacement bytes are copied verbatim, including NULs; braces and % are literal.
---@param text string
---@param tags table<string, string>
---@return string
function sb.formatTags(text, tags) end

--- Returns the result of merging the contents of b on top of a. ---
---@param a Json
---@param b Json
---@return Json
function sb.jsonMerge(a, b) end

--- Attempts to extract the value in the specified content at the specified path, and returns the found value or the specified default if no such value exists. ---
---@param content Json
---@param path string
---@param default Json
---@return Json
function sb.jsonQuery(content, path, default) end

--- Returns a statically randomized 32-bit signed integer based on the given list of seed values. ---
---@param ... LuaValue
---@return number
function sb.staticRandomI32(...) end

--- Returns a statically randomized 32-bit signed integer within the specified range based on the given list of seed values. ---
---@param min number
---@param max number
---@param ... LuaValue
---@return number
function sb.staticRandomI32Range(min, max, ...) end

--- Returns a statically randomized `double` based on the given list of seed values. ---
---@param ... LuaValue
---@return number
function sb.staticRandomDouble(...) end

--- Returns a statically randomized `double` within the specified range based on the given list of seed values. ---
---@param min number
---@param max number
---@param ... LuaValue
---@return number
function sb.staticRandomDoubleRange(min, max, ...) end

--- Creates and returns a Lua UserData value which can be used as a random source, initialized with the specified seed. The `RandomSource` has the following methods:
---@param seed? unsigned
---@return RandomSource
function sb.makeRandomSource(seed) end

--- Creates and returns a Lua UserData value which can be used as a Perlin noise source. The configuration for the `PerlinSource` should be a JSON object and can include the following keys: * `unsigned` __seed__ - Seed value used to initialize the source. * `String` __type__ - Type of noise to use. Valid types are "perlin", "billow" or "ridgedMulti". * `int` __octaves__ - Number of octaves of noise to use. Defaults to 1. * `double` __frequency__ - Defaults to 1.0. * `double` __amplitude__ - Defaults to 1.0. * `double` __bias__ - Defaults to 0.0. * `double` __alpha__ - Defaults to 2.0. * `double` __beta__ - Defaults to 2.0. * `double` __offset__ - Defaults to 1.0. * `double` __gain__ - Defaults to 2.0. The `PerlinSource` has only one method:
---@param config Json
---@return PerlinSource
function sb.makePerlinSource(config) end


--- Parses a JSON document; malformed input raises an error.
---@param text string
---@return Json
function sb.parseJson(text) end

--- Equivalent to sb.parseJson.
---@param text string
---@return Json
function sb.jsonFromString(text) end

--- Parses a sequence of JSON values into an array.
---@param text string
---@return JsonArray
function sb.parseJsonSequence(text) end

--- Compares JSON values for equality.
---@param a Json
---@param b Json
---@return boolean
function sb.jsonEqual(a, b) end

--- Removes text formatting escape codes.
---@param text string
---@return string
function sb.stripEscapeCodes(text) end

---@return integer
function sb.millisecondsSinceEpoch() end

--- Returns monotonic milliseconds for elapsed-time animations, not a wall-clock date.
---@return integer
function sb.monotonicMilliseconds() end

--- Precise native monotonic seconds (not rounded milliseconds or wall-clock time); compare only within this run.
---@return number
function sb.monotonicTime() end

--- A promise fulfiller returned by sb.makePromise.
---@class RpcPromiseKeeper<T>
---@field chain fun(self: RpcPromiseKeeper<T>, promise: RpcPromise<T>)
---@field fulfill fun(self: RpcPromiseKeeper<T>, value: T)
---@field fail fun(self: RpcPromiseKeeper<T>, reason: string)
---@field cancel fun(self: RpcPromiseKeeper<T>):boolean Whether producer cancellation won.

--- Returns one {promise, keeper} pair table; fulfilling nil is successful JSON null.
---@return {[1]: RpcPromise<Json>, [2]: RpcPromiseKeeper<Json>}
function sb.makePromise() end
