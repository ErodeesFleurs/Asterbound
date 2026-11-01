
# Threads

The new `threads` table is accessible from every script and allows creating, communicating with, and destroying scriptable threads.
Scriptable threads are stopped and joined when their parent script session shuts down or reloads. A lifecycle change requested inside a callback is deferred until the outermost callback returns, so its bindings remain valid during that call.

---

#### `String` threads.create(`Json` parameters)

Creates a thread using the given parameters, and returns the thread's name.

Here's an example that uses all available parameters:
```lua
threads.create({
    name="example", -- This is the thread name you'll use to index the thread. If unspecified, uses a random UUID as the thread name.
    scripts={
        main={"/scripts/examplethread.lua"}, -- A list of scripts for each context, similarly to how other scripts work.
        other={"/scripts/examplesecondthreadscript.lua"}, -- Threads can have multiple contexts.
    },
    instructionLimit=100000000, -- Optional, threads are allowed to change their own instruction limit (as they have nothing else to block if stuck).
    tickRate=60, -- Optional, how many ticks per second the thread runs at, defaults to 60 but can be any number.
    updateMeasureWindow=0.5, -- Optional, defaults to 0.5, changing this is unnecessary unless you really care about an accurate tickrate for some reason.
    logMapped=true, -- Optional, defaults to true. If false, thread's lua memory and update rate won't be included in debug log map.
    someParameter="scungus" -- Parameters for the scripts, all parameters are accessible using config.getParameter in the script contexts.
}),
```

---

#### `void` threads.setPause(`String` name, `bool` paused)

Pauses or unpauses a thread.

---

#### `void` threads.stop(`String` name)

Stops and joins the thread, then removes it. Its script contexts finish shutdown and queued message requests are cancelled before it can be reclaimed.

---

#### `RpcPromise<Json>` threads.sendMessage(`String` threadName, `String` messageName, [`LuaValue` args...])

Sends a message to the given thread. The promise resolves to the handler's JSON result, or chains a returned `RpcPromise<Json>`. Startup failures fail queued requests; shutdown cancels unhandled requests and pending deferred replies. A late producer cannot overwrite cancellation. Successful JSON `null` is still success; see [promise methods](utility.md#pairrpcpromisejsonrpcpromisekeeperjson-sbmakepromise).

---

The following callback is only accessible on updateable scripts.
This means everything **except** the following:
- Command processor scripts, both server and client
- Item aging scripts
- Augment application scripts
- Fireable item scripts

---

#### `void` threads.setMessageHandler(`String` messageName, `LuaFunction` handler)

Messages of the specified message type sent by any thread will call the specified function.
The function is passed the following arguments:
    (`String` threadName, `String` message, `Json` args...)

Pass `nil` as the handler to remove it. A running handler may replace or remove itself; the current call finishes using the handler it started with.

---

Threads have simple updateable scripts with access to only a few tables.
They include:
 - the basic tables all scripts have access to (including `threads`)
 - `updateablescript` bindings
 - `message`, for handling thread messages
 - `config`
 - `thread`

They may include additional bindings depending on what script context created them.
- `universe` on server universe/world scripts
- `celestial` on server universe/world scripts
 
---

The `thread` table is accessible only in scriptable threads.

#### `void` thread.stop()

Requests termination after the current callback. The parent reclaims the worker only after its contexts have shut down and queued requests have been settled.

---

#### `RpcPromise<Json>` thread.sendParentMessage(`String` messageName, [`LuaValue` args...])

Sends a message to the parent context.
