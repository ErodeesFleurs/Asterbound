struct lua_State;

void pushImguiBindings(lua_State* lState);

// Main-thread application render scope, not a simulation tick or worker script.
void setImguiPresentationActive(bool active);