# Chat

[Lua API](../README.md#openstarbound-compatible-extensions) · [Modding guide](../../modding.md)

The `chat` table is accessible from almost every client-side script. Network messages, command execution and logging are native services; the bundled chat window is a Lua-managed `"chat"` instance registered under `/interface.config:panes`, with layout and scripts in `/interface/asterbound/chat/`. There is no native Chat window fallback.

---

#### `void` chat.send(`String` message, [`String` modeName], [`bool` speak], [`Json` data])

Sends a new message in the chat. Optionally specifies the modeName:

- `"Broadcast"`: Global chat. Default value.
- `"Local"`: Chat within the current planet.
- `"Party"`: Chat within the current party.

If `speak` is false, the chat bubble will not appear above the player.
If `data` is provided, it will be sent as a JSON object with the message. This can be used to send custom data with the message.

---

#### `String[]` chat.command(`String` command)

Executes the specified command and returns its synchronous result strings; it does not automatically display those strings. Asynchronous command results retain their normal chat delivery. Script calls cannot authorize clipboard access by supplying additional arguments. Clipboard authorization is scoped to genuine native user submission/repeat handling and the existing clipboard policy.

---

#### `void` chat.addMessage(`String` text, [`Json` config])

Adds the specified message to the chat log. The following keys are available in the `config` JSON object:

- `String` __mode__ - The mode of the message. Can be one of the followgin: 
	- `"Broadcast"`
	- `"Local"`
	- `"Party"`
	- `"Whisper"`
	- `"CommandResult"`
	- `"RadioMessage"`
	- `"World"`
- `String` __channelName__ - The name of the channel to send the message to.
- `String` __fromNick__ - The name of the sender of the message.
- `String` __portrait__ - message portrait.
- `bool` __showPane__ - If false, the chat pane will not be triggered.
- `unsigned` __fromConnection__ - Optional local origin metadata; defaults to
  the client's connection ID. `0` represents the server origin in local radio
  records. This function only adds a local display/log message: it does not
  send a packet, authorize a command or authenticate the claimed sender.

---

#### `String` chat.input()

Returns the current chat input text.

---

#### `bool` chat.setInput(`String` text, [`bool` moveCursor])

Sets the current chat input text. If `moveCursor` is true, the cursor will be moved to the end of the text. Returns true if the input was set successfully, false otherwise.

---

#### `void` chat.clear([`unsigned`] count)

Clears received messages, not input text. If `count` is provided, removes the newest `count` messages; otherwise clears all received messages. Use `chat.setInput("")` to clear input.

---

#### `LuaVariadic<Json>` chat.parseArguments()

Parses a space-separated sequence of JSON values in a string into multiple values. Useful for parsing command arguments.
Similar to `sb.parseJsonSequence`.

## Presentation and storage

Lua owns slash-command detection, post-send input behavior, channel selection,
filtering, portraits, scrolling, input history, expansion and fade. Messages
retain their raw fields; text layout is a separate cache. `showPane = false`
does not wake the display. Native logging does not depend on rendering.

Input drafts and messages use the pane's standard `storage`, surviving script
reload while that instance exists. Input history retains the existing
PlayerStorage `"chatHistory"` metadata through the generic
`client.storage("chatHistory")` service. Party availability uses
`player.teamMembers()`, not a chat-specific native callback.
