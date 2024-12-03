#include "lua_pa.h"

static lua_pa_state* pa_state = NULL;

static signal_handler_t* signal_handlers = NULL;

static size_t num_signal_handlers = 0;

static active_sink_sources_t* active_sinks = NULL;
static size_t num_sinks = 0;
static active_sink_sources_t* active_sources = NULL;
static size_t num_sources = 0;

static pa_sink_info* deep_copy_sink_info(const pa_sink_info* info) {
	pa_sink_info* info_copy = malloc(sizeof(pa_sink_info));
	if (!info_copy) {
		fprintf(stderr, "ERROR: Memory allocation failed for sink info copy.\n");
		return NULL;
	}
	*info_copy = *info;

	if (info->name) {
		info_copy->name = strdup(info->name);
		if (!info_copy->name) {
			fprintf(stderr, "ERROR: Memory allocation failed for sink name copy.\n");
			free(info_copy);
			return NULL;
		}
	} else {
		info_copy->name = NULL;
	}
	return info_copy;
}

static pa_source_info* deep_copy_source_info(const pa_source_info* info) {
	pa_source_info* info_copy = malloc(sizeof(pa_source_info));
	if (!info_copy) {
		fprintf(stderr, "ERROR: Memory allocation failed for source info copy.\n");
		return NULL;
	}
	*info_copy = *info;

	if (info->name) {
		info_copy->name = strdup(info->name);
		if (!info_copy->name) {
			fprintf(stderr, "ERROR: Memory allocation failed for source name copy.\n");
			free(info_copy);
			return NULL;
		}
	} else {
		info_copy->name = NULL;
	}
	return info_copy;
}

static int lua_sink_factory(lua_State* L, const pa_sink_info* info) {
	if (!L || !info || !info->name) return 1;

	lua_newtable(L);

	lua_pushstring(L, "description");
	lua_pushstring(L, info->description);
	lua_settable(L, -3);
	lua_pushstring(L, "name");
	lua_pushstring(L, info->name);
	lua_settable(L, -3);

	lua_pushstring(L, "index");
	lua_pushinteger(L, info->index);
	lua_settable(L, -3);

	double dB = pa_sw_volume_to_dB(pa_cvolume_avg(&info->volume));
	int v = (int)round(100 * pow(10, dB / 60));
	lua_pushstring(L, "volume");
	lua_pushinteger(L, v);
	lua_settable(L, -3);

	lua_pushstring(L, "mute");
	lua_pushboolean(L, info->mute);
	lua_settable(L, -3);

	lua_pushstring(L, "set_volume");
	lua_pushcfunction(L, lua_pa_set_volume_sink);
	lua_settable(L, -3);

	lua_pushstring(L, "set_mute");
	lua_pushcfunction(L, lua_pa_set_mute_sink);
	lua_settable(L, -3);

	lua_pushstring(L, "set_default");
	lua_pushcfunction(L, lua_pa_set_default_sink);
	lua_settable(L, -3);

	return 0;
}

static int lua_source_factory(lua_State* L, const pa_source_info* info) {
	if (!L || !info || !info->name) return 1;

	lua_newtable(L);

	lua_pushstring(L, "description");
	lua_pushstring(L, info->description);
	lua_settable(L, -3);

	lua_pushstring(L, "name");
	lua_pushstring(L, info->name);
	lua_settable(L, -3);

	lua_pushstring(L, "index");
	lua_pushinteger(L, info->index);
	lua_settable(L, -3);

	double dB = pa_sw_volume_to_dB(pa_cvolume_avg(&info->volume));
	int v = (int)round(100 * pow(10, dB / 60));

	lua_pushstring(L, "volume");
	lua_pushinteger(L, v);
	lua_settable(L, -3);

	lua_pushstring(L, "mute");
	lua_pushboolean(L, info->mute);
	lua_settable(L, -3);

	lua_pushstring(L, "set_volume");
	lua_pushcfunction(L, lua_pa_set_volume_source);
	lua_settable(L, -3);

	lua_pushstring(L, "set_mute");
	lua_pushcfunction(L, lua_pa_set_mute_source);
	lua_settable(L, -3);

	lua_pushstring(L, "set_default");
	lua_pushcfunction(L, lua_pa_set_default_source);
	lua_settable(L, -3);

	return 0;
}

static int lua_pa_set_volume_sink(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* sink_name = NULL;
	int volume = 0;

	if (nargs == 2 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		sink_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);

		volume = luaL_checkinteger(L, 2);
	} else if (nargs == 2) {
		sink_name = luaL_checkstring(L, 1);
		volume = luaL_checkinteger(L, 2);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_volume(device, volume) or object.set_volume(volume)");
		lua_error(L);
	}

	if (volume < 0) volume = 0;
	if (volume > 100) volume = 100;

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_volume_t pa_volume = pa_sw_volume_from_dB(60 * log10(volume / 100.0));
	pa_cvolume cvolume;
	pa_cvolume_set(&cvolume, 1, pa_volume);

	pa_operation* op = pa_context_set_sink_volume_by_name(pa_state->ctx, sink_name, &cvolume, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);
	return 1;
}

static int lua_pa_set_volume_source(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* source_name = NULL;
	int volume = 0;

	if (nargs == 2 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		source_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);

		volume = luaL_checkinteger(L, 2);
	} else if (nargs == 2) {
		source_name = luaL_checkstring(L, 1);
		volume = luaL_checkinteger(L, 2);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_volume(device, volume) or object.set_volume(volume)");
		lua_error(L);
	}

	if (volume < 0) volume = 0;
	else if (volume > 100) volume = 100;

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_volume_t pa_volume = pa_sw_volume_from_dB(60 * log10(volume / 100.0));
	pa_cvolume cvolume;
	pa_cvolume_set(&cvolume, 1, pa_volume);

	pa_operation* op = pa_context_set_source_volume_by_name(pa_state->ctx, source_name, &cvolume, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);
	return 1;
}

static int lua_pa_set_mute_sink(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* sink_name = NULL;
	int mute = 0;

	if (nargs == 2 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		sink_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);

		mute = lua_toboolean(L, 2);
	} else if (nargs == 2) {
		sink_name = luaL_checkstring(L, 1);
		mute = lua_toboolean(L, 2);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_volume(device, volume) or object.set_volume(volume)");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_set_sink_mute_by_name(pa_state->ctx, sink_name, mute, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);
	return 1;
}

static int lua_pa_set_mute_source(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* sink_name = NULL;
	int mute = 0;

	if (nargs == 2 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		sink_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);

		mute = lua_toboolean(L, 2);
	} else if (nargs == 2) {
		sink_name = luaL_checkstring(L, 1);
		mute = lua_toboolean(L, 2);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_volume(device, volume) or object.set_volume(volume)");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_set_sink_mute_by_name(pa_state->ctx, sink_name, mute, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);
	return 1;
}

static int lua_pa_set_default_sink(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* sink_name = NULL;

	if (nargs == 1 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		sink_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);
	} else if (nargs == 1) {
		sink_name = luaL_checkstring(L, 1);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_default_sink(device) or object.set_default_sink()");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_set_default_sink(pa_state->ctx, sink_name, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);

	return 1;
}

static int lua_pa_set_default_source(lua_State* L) {
	int nargs = lua_gettop(L);

	const char* source_name = NULL;

	if (nargs == 1 && lua_istable(L, 1)) {
		lua_getfield(L, 1, "name");
		source_name = luaL_checkstring(L, -1);
		lua_pop(L, 1);
	} else if (nargs == 1) {
		source_name = luaL_checkstring(L, 1);
	} else {
		lua_pushstring(L, "Invalid arguments. Usage: set_default_source(device) or object.set_default_source()");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_set_default_source(pa_state->ctx, source_name, lua_pa_successful_callback, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	lua_pushboolean(L, 1);

	return 1;
}

static int lua_pa_get_all_sinks(lua_State* L) {
	if (!pa_state) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	lua_newtable(L);

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_sink_info_list(pa_state->ctx, sink_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static int lua_pa_get_all_sources(lua_State* L) {
	if (!pa_state) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	lua_newtable(L);

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_source_info_list(pa_state->ctx, source_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static int lua_pa_get_default_sink(lua_State* L) {
	if (!pa_state) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_server_info(pa_state->ctx, sink_server_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	const char* default_sink_name = lua_tostring(L, -1);
	lua_pop(L, 1);

	pa_threaded_mainloop_lock(pa_state->mainloop);

	op = pa_context_get_sink_info_by_name(pa_state->ctx, default_sink_name, default_sink_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static int lua_pa_get_default_source(lua_State* L) {
	if (!pa_state) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_server_info(pa_state->ctx, source_server_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	const char* default_sink_name = lua_tostring(L, -1);
	lua_pop(L, 1);

	pa_threaded_mainloop_lock(pa_state->mainloop);

	op = pa_context_get_source_info_by_name(pa_state->ctx, default_sink_name, default_source_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static int lua_pa_get_sink_by_name(lua_State* L) {
	if (!pa_state || !pa_state->mainloop) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	const char* name = luaL_checkstring(L, 1);

	if (name == NULL)
		return 0;

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_sink_info_by_name(pa_state->ctx, name, sink_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static int lua_pa_get_source_by_name(lua_State* L) {
	if (!pa_state || !pa_state->mainloop) {
		lua_pushstring(L, "PulseAudio not initialized.");
		lua_error(L);
	}

	const char* name = luaL_checkstring(L, 1);

	if (name == NULL)
		return 0;

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_source_info_by_name(pa_state->ctx, name, source_info_cb, L);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 1;
}

static void context_state_cb(pa_context* c, void* userdata __attribute__((unused))) {
	if (!pa_state || !pa_state->mainloop) return;

	pa_context_state_t state = pa_context_get_state(c);

	switch (state) {
	case PA_CONTEXT_READY:
		pa_threaded_mainloop_signal(pa_state->mainloop, 0);
		break;
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED:
		pa_threaded_mainloop_signal(pa_state->mainloop, 0);
		pa_init( );
		break;
	default:
		break;
	}
}

static void lua_pa_successful_callback(pa_context* c __attribute__((unused)), int success __attribute__((unused)), void* userdata __attribute__((unused))) {
	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void sink_info_cb(pa_context* c __attribute__((unused)), const pa_sink_info* info, int eol, void* userdata) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	if (!eol)
		if (lua_sink_factory(L, info) == 0)
			lua_rawseti(L, -2, luaL_len(L, -2) + 1);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void* trigger_signal_7_sink(void* userdata) {
	arg_list* al = (arg_list*)userdata;
	pa_sink_info* info = (pa_sink_info*)al->info;
	lua_pa_trigger_signal(
		al->signal_name,
		al->types,
		info->description,
		info->name,
		info->index,
		(int)round(100 * pow(10, pa_sw_volume_to_dB(pa_cvolume_avg(&info->volume)) / 60)),
		info->mute
	);

	free(info);
	free(al);

	pthread_mutex_unlock(&pa_state->mutex);

	return NULL;
}

static void* trigger_signal_7_source(void* userdata) {
	arg_list* al = (arg_list*)userdata;
	pa_source_info* info = (pa_source_info*)al->info;
	lua_pa_trigger_signal(
		al->signal_name,
		al->types,
		info->description,
		info->name,
		info->index,
		(int)round(100 * pow(10, pa_sw_volume_to_dB(pa_cvolume_avg(&info->volume)) / 60)),
		info->mute
	);

	free(info);
	free(al);

	pthread_mutex_unlock(&pa_state->mutex);

	return NULL;
}

static void* trigger_signal_3(void* userdata) {
	arg_list* al = (arg_list*)userdata;

	lua_pa_trigger_signal(
		al->signal_name,
		al->types,
		al->info
	);

	free(al);

	pthread_mutex_unlock(&pa_state->mutex);

	return NULL;
}

static void* trigger_signal_3_source(void* userdata) {
	arg_list* al = (arg_list*)userdata;
	pa_source_info* info = (pa_source_info*)al->info;

	lua_pa_trigger_signal(
		al->signal_name,
		al->types,
		al->info
	);

	free(info);
	free(al);

	pthread_mutex_unlock(&pa_state->mutex);

	return NULL;
}

static void* trigger_signal_3_sink(void* userdata) {
	arg_list* al = (arg_list*)userdata;
	pa_sink_info* info = (pa_sink_info*)al->info;

	lua_pa_trigger_signal(
		al->signal_name,
		al->types,
		al->info
	);

	free(info);
	free(al);

	pthread_mutex_unlock(&pa_state->mutex);

	return NULL;
}

static void signal_sink_info_cb(pa_context* c __attribute__((unused)), const pa_sink_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		arg_list* al = malloc(sizeof(arg_list));

		al->signal_name = "pulseaudio::sink_change";
		al->types = "ssiib";
		al->info = deep_copy_sink_info(info);
		pthread_mutex_lock(&pa_state->mutex);
		pthread_create(&pa_state->thread, NULL, trigger_signal_7_sink, al);
		pthread_detach(pa_state->thread);
	}

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void signal_sink_new_cb(pa_context* c __attribute__((unused)), const pa_sink_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		active_sinks = realloc(active_sinks, (num_sinks + 1) * sizeof(active_sink_sources_t));

		active_sinks[num_sinks].index = info->index;
		active_sinks[num_sinks].name = strdup(info->name);
		num_sinks++;

		arg_list* al = malloc(sizeof(arg_list));

		al->signal_name = "pulseaudio::sink_new";
		al->types = "u";
		al->info = deep_copy_sink_info(info);

		pthread_mutex_lock(&pa_state->mutex);
		pthread_create(&pa_state->thread, NULL, trigger_signal_3_sink, al);
		pthread_detach(pa_state->thread);

	}
	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void signal_source_info_cb(pa_context* c __attribute__((unused)), const pa_source_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		arg_list* al = malloc(sizeof(arg_list));

		al->signal_name = "pulseaudio::source_change";
		al->types = "ssiib";
		al->info = deep_copy_source_info(info);
		pthread_mutex_lock(&pa_state->mutex);
		pthread_create(&pa_state->thread, NULL, trigger_signal_7_source, al);
		pthread_detach(pa_state->thread);
	}

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void signal_source_new_cb(pa_context* c __attribute__((unused)), const pa_source_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		active_sources = realloc(active_sources, (num_sources + 1) * sizeof(active_sink_sources_t));

		active_sources[num_sources].index = info->index;
		active_sources[num_sources].name = strdup(info->name);
		num_sources++;

		arg_list* al = malloc(sizeof(arg_list));

		al->signal_name = "pulseaudio::source_new";
		al->types = "o";
		al->info = deep_copy_source_info(info);

		pthread_mutex_lock(&pa_state->mutex);
		pthread_create(&pa_state->thread, NULL, trigger_signal_3_source, al);
		pthread_detach(pa_state->thread);
	}
	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void source_info_cb(pa_context* c __attribute__((unused)), const pa_source_info* info, int eol, void* userdata) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	if (!eol)
		if (lua_source_factory(L, info) == 0)
			lua_rawseti(L, -2, luaL_len(L, -2) + 1);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void sink_server_info_cb(pa_context* c __attribute__((unused)), const pa_server_info* info, void* userdata) {
	if (!pa_state || !info || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	lua_pushstring(L, info->default_sink_name);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void source_server_info_cb(pa_context* c __attribute__((unused)), const pa_server_info* info, void* userdata) {
	if (!pa_state || !info || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	lua_pushstring(L, info->default_source_name);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void default_sink_info_cb(pa_context* c __attribute__((unused)), const pa_sink_info* info, int eol, void* userdata) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	if (!eol)
		lua_sink_factory(L, info);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void default_source_info_cb(pa_context* c __attribute__((unused)), const pa_source_info* info, int eol, void* userdata) {
	if (eol < 0 || !info || !pa_state || !pa_state->mainloop) return;

	lua_State* L = (lua_State*)userdata;

	if (!eol)
		lua_source_factory(L, info);

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static int lua_pa_connect_signal(lua_State* L) {
	const char* signal_name = luaL_checkstring(L, 1);
	if (!lua_isfunction(L, 2)) {
		lua_pushstring(L, "Signal handler must be a function.");
		lua_error(L);
	}

	signal_handlers = realloc(signal_handlers, sizeof(signal_handler_t) * (num_signal_handlers + 1));
	signal_handlers[num_signal_handlers].signal_name = strdup(signal_name);
	signal_handlers[num_signal_handlers].L = L;
	signal_handlers[num_signal_handlers].ref = luaL_ref(L, LUA_REGISTRYINDEX);
	num_signal_handlers++;

	return 0;
}

static void lua_pa_trigger_signal(const char* signal_name, const char* types, ...) {
	va_list args;
	va_start(args, types);
	int argc = 0;

	for (size_t i = 0; i < num_signal_handlers; i++) {
		if (strcmp(signal_handlers[i].signal_name, signal_name) == 0) {
			lua_State* L = signal_handlers[i].L;
			lua_rawgeti(L, LUA_REGISTRYINDEX, signal_handlers[i].ref);

			for (size_t j = 0; types[j] != '\0'; j++) {
				switch (types[j]) {
				case 's':
					const char* str = va_arg(args, const char*);
					lua_pushstring(L, str);
					break;
				case 'i':
					int i_val = va_arg(args, int);
					lua_pushinteger(L, i_val);
					break;
				case 'b':
					int b = va_arg(args, int);
					lua_pushboolean(L, b);
					break;
				case 'u': {
					const pa_sink_info* info = va_arg(args, const pa_sink_info*);
					if (info)
						lua_sink_factory(L, info);
					else
						lua_pushnil(L);
					break;
				}
				case 'o': {
					const pa_source_info* info = va_arg(args, const pa_source_info*);
					if (info)
						lua_source_factory(L, info);
					else
						lua_pushnil(L);
					break;
				}
				default:
					break;
				}
				argc++;
			}

			if (lua_pcall(L, argc, 0, 0) != 0) {
				lua_pushstring(L, "Error in signal handler \n");
				lua_error(L);
			}
		}
	}

	va_end(args);
}

static void lua_pa_subscribe_cb(pa_context* c, pa_subscription_event_type_t type, uint32_t index, void* userdata) {
	if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK) {
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
			pa_operation* op = pa_context_get_sink_info_by_index(c, index, signal_sink_info_cb, userdata);
			pa_operation_unref(op);
		}
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
			pa_operation* op = pa_context_get_sink_info_by_index(c, index, signal_sink_new_cb, userdata);
			pa_operation_unref(op);
		}
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
			for (size_t i = 0; i < num_sinks; i++) {
				if (active_sinks[i].index == index) {
					char* sink_name_copy = NULL;
					if (active_sinks[i].name != NULL) {
						sink_name_copy = strdup(active_sinks[i].name);
					}

					arg_list* al = malloc(sizeof(arg_list));

					al->signal_name = "pulseaudio::sink_remove";
					al->types = "s";
					al->info = sink_name_copy;

					pthread_mutex_lock(&pa_state->mutex);
					pthread_create(&pa_state->thread, NULL, trigger_signal_3, al);
					pthread_detach(pa_state->thread);

					if (active_sinks[i].name != NULL)
						active_sinks[i].name = NULL;


					for (size_t j = i; j < num_sinks - 1; ++j)
						active_sinks[j] = active_sinks[j + 1];

					num_sinks--;

					if (num_sinks > 0) {
						active_sinks = realloc(active_sinks, num_sinks * sizeof(active_sink_sources_t));
						if (active_sinks == NULL) {
							perror("realloc failed");
							exit(EXIT_FAILURE);
						}
					} else {
						free(active_sinks);
						active_sinks = NULL;
					}

					break;
				}
			}
		}
	}
	if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE) {
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
			pa_operation* op = pa_context_get_source_info_by_index(c, index, signal_source_info_cb, userdata);
			pa_operation_unref(op);
		}
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
			pa_operation* op = pa_context_get_source_info_by_index(c, index, signal_source_new_cb, userdata);
			pa_operation_unref(op);
		}
		if ((type & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
			for (size_t i = 0; i < num_sources; i++) {
				if (active_sources[i].index == index) {
					char* source_name_copy = NULL;
					if (active_sources[i].name != NULL) {
						source_name_copy = strdup(active_sources[i].name);
					}

					arg_list* al = malloc(sizeof(arg_list));

					al->signal_name = "pulseaudio::source_remove";
					al->types = "s";
					al->info = source_name_copy;

					pthread_mutex_lock(&pa_state->mutex);
					pthread_create(&pa_state->thread, NULL, trigger_signal_3, al);
					pthread_detach(pa_state->thread);

					if (active_sources[i].name != NULL)
						active_sources[i].name = NULL;

					for (size_t j = i; j < num_sources - 1; ++j)
						active_sources[j] = active_sources[j + 1];

					num_sources--;

					if (num_sources > 0) {
						active_sources = realloc(active_sources, num_sources * sizeof(active_sink_sources_t));
						if (active_sources == NULL) {
							perror("realloc failed");
							exit(EXIT_FAILURE);
						}
					} else {
						free(active_sources);
						active_sources = NULL;
					}

					break;
				}
			}
		}
	} else if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SERVER) {
		printf("SERVER\n");
	} else if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_CLIENT) {
		printf("CLIENT\n");
	}
}

static int pa_init( ) {
	pa_state = (lua_pa_state*)malloc(sizeof(lua_pa_state));

	pa_state->mainloop = pa_threaded_mainloop_new( );
	if (!pa_state->mainloop) {
		free(pa_state);
		pa_state = NULL;
		return -1;
	}

	pa_state->ctx = pa_context_new(pa_threaded_mainloop_get_api(pa_state->mainloop), "Lua Pulseaudio");
	if (!pa_state->ctx) {
		pa_threaded_mainloop_free(pa_state->mainloop);
		free(pa_state);
		pa_state = NULL;
		return -1;
	}

	pa_context_set_state_callback(pa_state->ctx, context_state_cb, NULL);

	if (pa_context_connect(pa_state->ctx, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
		pa_context_unref(pa_state->ctx);
		pa_threaded_mainloop_free(pa_state->mainloop);
		free(pa_state);
		pa_state = NULL;
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);
	pa_threaded_mainloop_start(pa_state->mainloop);

	pa_threaded_mainloop_wait(pa_state->mainloop);

	if (pa_context_get_state(pa_state->ctx) != PA_CONTEXT_READY)	return 0;

	pa_threaded_mainloop_unlock(pa_state->mainloop);

	pa_threaded_mainloop_lock(pa_state->mainloop);
	pa_operation* op = pa_context_subscribe(pa_state->ctx, PA_SUBSCRIPTION_MASK_ALL, lua_pa_successful_callback, NULL);
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_context_set_subscribe_callback(pa_state->ctx, lua_pa_subscribe_cb, NULL);
	pa_threaded_mainloop_unlock(pa_state->mainloop);

	return 0;
}

static int lua_pa_cleanup(lua_State* L) {
	if (pa_state) {
		if (pa_state->ctx) {
			pa_context_disconnect(pa_state->ctx);
			pa_context_unref(pa_state->ctx);
		}
		pa_threaded_mainloop_stop(pa_state->mainloop);
		if (pa_state->mainloop)
			pa_threaded_mainloop_free(pa_state->mainloop);
		free(pa_state);
		pa_state = NULL;
	}

	lua_pushboolean(L, 1);
	return 1;
}

static void fill_active_sinks(pa_context* c __attribute__((unused)), const pa_sink_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		active_sinks = realloc(active_sinks, (num_sinks + 1) * sizeof(active_sink_sources_t));

		active_sinks[num_sinks].name = strdup(info->name);

		active_sinks[num_sinks].index = info->index;
		num_sinks++;
	}

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static void fill_active_sources(pa_context* c __attribute__((unused)), const pa_source_info* info, int eol, void* userdata __attribute__((unused))) {
	if (eol < 0 || !pa_state || !pa_state->mainloop) return;

	if (!eol) {
		active_sources = realloc(active_sources, (num_sources + 1) * sizeof(active_sink_sources_t));

		active_sources[num_sources].name = strdup(info->name);
		active_sources[num_sources].index = info->index;
		num_sources++;
	}

	pa_threaded_mainloop_signal(pa_state->mainloop, 0);
}

static int lua_quit(lua_State* L) {
	lua_pa_cleanup(L);
	puts("Lua exited, exiting now...\n");
	return 0;
}

static const struct luaL_Reg lua_pa_funcs[] = {
	{"cleanup", lua_pa_cleanup},
	{"get_all_sinks", lua_pa_get_all_sinks},
	{"get_all_sources", lua_pa_get_all_sources},
	{"get_default_sink", lua_pa_get_default_sink},
	{"get_default_source", lua_pa_get_default_source},
	{"set_volume_sink", lua_pa_set_volume_sink},
	{"set_volume_source", lua_pa_set_volume_source},
	{"set_default_sink", lua_pa_set_default_sink},
	{"set_default_source", lua_pa_set_default_source},
	{"set_mute_sink", lua_pa_set_mute_sink},
	{"set_mute_source", lua_pa_set_mute_source},
	{"get_sink_by_name", lua_pa_get_sink_by_name},
	{"get_source_by_name", lua_pa_get_source_by_name},
	{"connect_signal", lua_pa_connect_signal},
	{ NULL, NULL },
};

int luaopen_lua_pa(lua_State* L) {
	luaL_newlib(L, lua_pa_funcs);

	void* ud __attribute__((unused)) = lua_newuserdata(L, 0);

	luaL_newmetatable(L, "lua_quit");
	lua_pushcfunction(L, lua_quit);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);

	lua_setfield(L, -2, "__finalizer");

	if (pa_init( ) != 0) {
		luaL_error(L, "Error initializing pulseaudio\n");
		return -1;
	}

	pa_threaded_mainloop_lock(pa_state->mainloop);

	pa_operation* op = pa_context_get_sink_info_list(pa_state->ctx, fill_active_sinks, NULL);
	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);

	op = pa_context_get_source_info_list(pa_state->ctx, fill_active_sources, NULL);

	while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pa_state->mainloop);
	pa_operation_unref(op);

	pa_threaded_mainloop_unlock(pa_state->mainloop);
	return 1;
}
