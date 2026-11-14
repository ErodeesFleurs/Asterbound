-- Small helper to organize code for the same context into different Lua scripts without having to "hook" previously defined.

modules = setmetatable({}, {__call = function(_this, path, names)
	for _, name in pairs(names) do
		require(path .. name .. ".lua")
	end
end})

local modules, type = modules, type
local function call(func, ...)
	if type(func) == "function" then
		return func(...)
	end
end

function init(...)
	script.setUpdateDelta(1)
	for _, module in pairs(modules) do
		call(module.init, ...)
	end
end

function update(...)
	for _, module in pairs(modules) do
		call(module.update, ...)
	end
end

function uninit(...)
	for _, module in pairs(modules) do
		call(module.uninit, ...)
	end
end