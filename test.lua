local socket = require 'socket'

package.cpath = package.cpath .. ';./bin/?.so'
local lua_pa = require 'lua_pa'

-- TEST getting all sinks
local all_sinks = lua_pa.get_all_sinks()
if not all_sinks then
	print('lua_pa.get_all_sinks ERROR')
	return false
end
print('lua_pa.get_all_sinks OK')

-- TEST getting all sources
local all_sources = lua_pa.get_all_sources()
if not all_sources then
	print('lua_pa.get_all_sources ERROR')
	return false
end
print('lua_pa.get_all_sources OK')

-- Test getting default sink
local default_sink = lua_pa.get_default_sink()
if not default_sink then
	print('lua_pa.get_default_sink ERROR')
	return false
end
print('lua_pa.get_default_sink OK')

-- Test getting default source
local default_source = lua_pa.get_default_source()
if not default_source then
	print('lua_pa.get_default_source ERROR')
	return false
end
print('lua_pa.get_default_source OK')

-- Test signals
local signal_processed = {
	sink_change = false,
	sink_new = false,
	sink_remove = false,
	source_change = false,
	source_new = false,
	source_remove = false,
}

lua_pa.connect_signal('pulseaudio::sink_change', function(description, name, id, volume, muted)
	print('Sink Changed:', description, name, id, volume, muted)
	signal_processed.sink_change = true
end)

lua_pa.connect_signal('pulseaudio::sink_new', function(sink)
	print('Sink Added:', sink.name)
	signal_processed.sink_new = true
end)

lua_pa.connect_signal('pulseaudio::sink_remove', function(name)
	print('Sink Removed:', name)
	signal_processed.sink_remove = true
end)

lua_pa.connect_signal('pulseaudio::source_change', function(description, name, id, volume, muted)
	print('Source Changed:', description, name, id, volume, muted)
	signal_processed.source_change = true
end)

lua_pa.connect_signal('pulseaudio::source_new', function(source)
	print('Source Added:', source.name)
	signal_processed.source_new = true
end)

lua_pa.connect_signal('pulseaudio::source_remove', function(name)
	print('Source Removed:', name)
	signal_processed.source_remove = true
end)

local loop = true
while loop do
	socket.select(nil, nil, 1)
	local all_signals_processed = true
	for _, processed in pairs(signal_processed) do
		if not processed then
			all_signals_processed = false
			break
		end
	end

	if all_signals_processed then
		print('Testing signals all OK')
		loop = false
	end
end

return 0
