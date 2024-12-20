#ifndef LUA_PA_H
#define LUA_PA_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <pulse/pulseaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
	pa_threaded_mainloop* mainloop;
	pa_context* ctx;
	pthread_mutex_t mutex;
	pthread_t thread;
}lua_pa_state;

typedef struct {
	const char* signal_name;
	lua_State* L;
	int ref;
} signal_handler_t;

typedef struct {
	const char* name;
	uint32_t index;
} active_sink_sources_t;

typedef struct {
	const char* signal_name;
	const char* types;
	const char* description;
	const char* name;
	uint32_t index;
	int volume;
	int mute;
	const void* info;
}arg_list;

static int lua_pa_set_volume_sink(lua_State* L);
static int lua_pa_set_volume_source(lua_State* L);
static int lua_pa_set_mute_sink(lua_State* L);
static int lua_pa_set_mute_source(lua_State* L);
static int lua_pa_set_default_sink(lua_State* L);
static int lua_pa_set_default_source(lua_State* L);

static int lua_pa_get_all_sinks(lua_State* L);
static int lua_pa_get_all_sources(lua_State* L);

static int lua_pa_get_default_sink(lua_State* L);
static int lua_pa_get_default_source(lua_State* L);

static void context_state_cb(pa_context* c, void* userdata);
static void lua_pa_successful_callback(pa_context* c, int success, void* userdata);
static void sink_info_cb(pa_context* c, const pa_sink_info* info, int eol, void* userdata);
static void source_info_cb(pa_context* c, const pa_source_info* info, int eol, void* userdata);
static void sink_server_info_cb(pa_context* c, const pa_server_info* info, void* userdata);
static void default_sink_info_cb(pa_context* c, const pa_sink_info* info, int eol, void* userdata);
static void source_server_info_cb(pa_context* c, const pa_server_info* info, void* userdata);
static void default_source_info_cb(pa_context* c, const pa_source_info* info, int eol, void* userdata);

static void lua_pa_trigger_signal(const char* signal_name, const char* types, ...);

static int pa_init( );

#endif // LUA_PA_H
