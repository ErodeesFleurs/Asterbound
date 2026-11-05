struct lua_State;

void pushImguiBindings(lua_State* lState);

// Application-owner render scope, not a simulation tick or another worker script.
void setImguiPresentationActive(bool active);