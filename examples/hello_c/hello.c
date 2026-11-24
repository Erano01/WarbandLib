/* Minimal plugin in plain C: logging, a menu window, screen text, a shared
 * interface, a custom event and a hook on Agent_SetHitPoints shared with hp_editor. */
#include <stdio.h>

#include "warbandlib/warbandlib.h"

#define AGENT_SET_HIT_POINTS_VA 0x0049EE40u

typedef struct hello_api_v1 {
	uint32_t struct_size;
	uint32_t(WBL_CALL* greet_count)(void);
} hello_api_v1;

static const wbl_host_api* g_host;
static wbl_plugin g_self;
static wbl_bool g_show_banner = WBL_TRUE;
static uint32_t g_greets;
static uint32_t g_hp_calls;
static wbl_event_id g_greet_event;

/* __thiscall(Agent* this, float* hp): this in ECX, callee pops the argument; __fastcall matches with EDX unused. */
typedef void(__fastcall* set_hit_points_fn)(void* agent, void* edx, float* hp);
static set_hit_points_fn g_original_set_hit_points;

static void __fastcall set_hit_points_detour(void* agent, void* edx, float* hp) {
	++g_hp_calls;
	g_original_set_hit_points(agent, edx, hp);
}

static uint32_t WBL_CALL greet_count(void) { return g_greets; }
static const hello_api_v1 g_api = {sizeof(hello_api_v1), greet_count};

static wbl_event_reply WBL_CALL on_draw(const wbl_event* event, void* user) {
	const wbl_frame_event* frame = (const wbl_frame_event*)event->data;
	(void)user;
	if (g_show_banner) {
		g_host->draw->text(12.0f, (float)frame->height - 24.0f, WBL_RGBA(255, 220, 120, 255), "WarbandLib: hello_c plugin");
	}
	return WBL_EVENT_CONTINUE;
}

static wbl_event_reply WBL_CALL on_ui(const wbl_event* event, void* user) {
	char line[96];
	wbl_bool cancelled = WBL_FALSE;
	(void)event;
	(void)user;
	if (g_host->ui->begin_window("hello_c", NULL)) {
		g_host->ui->checkbox("Show banner", &g_show_banner);
		if (g_host->ui->button("Greet")) {
			++g_greets;
			g_host->events->post(g_greet_event, &g_greets, &cancelled);
			g_host->core->log(g_self, WBL_LOG_INFO, "greeted");
		}
		snprintf(line, sizeof(line), "greets: %u", (unsigned)g_greets);
		g_host->ui->text(line);
		snprintf(line, sizeof(line), "Agent_SetHitPoints calls seen: %u", (unsigned)g_hp_calls);
		g_host->ui->text(line);
	}
	g_host->ui->end_window();
	return WBL_EVENT_CONTINUE;
}

WBL_PLUGIN_QUERY {
	static const wbl_plugin_info info = {
	    sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 0, "example.hello_c", "Hello (C)", "1.0.0", "WarbandLib",
	    "Smallest possible plugin, written in C.", 0, NULL,
	};
	return &info;
}

WBL_PLUGIN_LOAD(host, self) {
	void* target;
	wbl_result r;
	char line[128];
	g_host = host;
	g_self = self;

	host->events->subscribe(self, WBL_EVENT_DRAW, WBL_PRIORITY_NORMAL, on_draw, NULL, NULL);
	host->events->subscribe(self, WBL_EVENT_UI, WBL_PRIORITY_NORMAL, on_ui, NULL, NULL);
	host->events->register_event(self, "example.hello_c.greeted", 0, &g_greet_event);
	host->core->register_interface(self, "example.hello_c.api", 1, &g_api);

	target = (void*)host->core->rebase(AGENT_SET_HIT_POINTS_VA);
	r = host->hooks->create(self, target, (void*)set_hit_points_detour, WBL_PRIORITY_HIGH,
	                        (void**)&g_original_set_hit_points, NULL);
	snprintf(line, sizeof(line), "loaded, Agent_SetHitPoints hook: %d", (int)r);
	host->core->log(self, WBL_LOG_INFO, line);
	return WBL_OK;
}

WBL_PLUGIN_UNLOAD { g_host->core->log(g_self, WBL_LOG_INFO, "unloading"); }
