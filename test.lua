local inspect = require('pl.import_into')().pretty.write

package.cpath = package.cpath .. ';./bin/?.so'
local lua_pa = require 'lua_pa'

print(inspect(lua_pa.get_all_sinks()))
print(inspect(lua_pa.get_all_sources()))

print(inspect(lua_pa.get_default_sink()));
print(inspect(lua_pa.get_default_source()));

-- Connect a signal that will trigger when something happens
lua_pa.connect_signal('pulseaudio::sink_change', function(description, name, id, volume, muted)
	print('Sink Changed:', description, name, id, volume, muted)
end)

lua_pa.connect_signal('pulseaudio::sink_new', function(sink)
	print('Sink Added:', sink.name)
	print(inspect(lua_pa.get_all_sinks()))
end)

lua_pa.connect_signal('pulseaudio::sink_remove', function(name)
	print('Sink Removed:', name)
	print(inspect(lua_pa.get_all_sinks()))
end)

lua_pa.connect_signal('pulseaudio::source_change', function(description, name, id, volume, muted)
	print('Source Changed:', description, name, id, volume, muted)
end)

lua_pa.connect_signal('pulseaudio::source_new', function(source)
	print('Source Added:', source.name)
end)

lua_pa.connect_signal('pulseaudio::source_remove', function(name)
	print('Source Removed:', name)
end)

local socket = require('socket')

function sleep(seconds)
	socket.select(nil, nil, seconds)
end

while true do
	sleep(1)
end
