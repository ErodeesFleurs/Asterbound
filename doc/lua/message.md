# message

[Lua API](README.md#base-api) · [Modding guide](../modding.md)

The message table contains a single function, setHandler, which allows entities to receive messages sent using world.sendEntityMessage. Entities which can receive messages include:

* monster
* NPC
* object
* vehicle
* stagehand
* projectile

Additionally, messages can be handled by a variety of script contexts that run on the player:

* activeitem
* quest
* playercompanions
* status

---

#### `void` message.setHandler(`String` messageName, `LuaFunction` handler)

Messages of the specified message type received by this script context will call the specified function. The first two arguments passed to the handler function will be the `String` messageName and a `bool` indicating whether the message is from a local entity, followed by any arguments sent with the message.

Returning a `RpcPromise<Json>` defers the reply until that promise settles. A successful JSON `null` remains successful; use `succeeded()` or `status()`, not `result() ~= nil`, to check success. See [promise methods](openstarbound/utility.md#pairrpcpromisejsonrpcpromisekeeperjson-sbmakepromise).

Closing or reloading the receiving session cancels its pending deferred replies. Cancelling the sender's observation does not undo delivery or cancel a shared producer. Across the network, cancellation uses the existing failed-response representation; there is no separate cancellation packet.
