# CommandProcessor

[Lua API](README.md#base-api) · [Modding guide](../modding.md)

The command processor has a single binding for performing admin checks, available on the *CommandProcessor* table.

---

#### `String` CommandProcessor.adminCheck(`ConnectionId` connectionId, `String` actionDescription)

Checks whether the specified connection ID is authorized to perform admin actions and returns `nil` if authorization is successful. If unauthorized, returns a `String` error message to display to the client requesting the action, which may include the specified action description, such as "Insufficient privileges to do the time warp again."
