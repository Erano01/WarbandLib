/* C plugin: listens to the game's own events (ABI 1.1) and shows what it saw. No addresses. */
#include <stdio.h>

#include "warbandlib/warbandlib.h"

static const wbl_host_api* g_host;
static wbl_plugin g_self;

/* Logs the first event of each kind, so a run shows in the log what reached the plugin. */
static void LogFirst(volatile int32_t* seen, const char* text) {
	char line[200];
	if (*seen) return;
	*seen = 1;
	snprintf(line, sizeof line, "first %s (game thread: %s)", text, g_host->core->is_game_thread() ? "yes" : "NO");
	g_host->core->log(g_self, WBL_LOG_INFO, line);
}
static volatile int32_t g_seen_trigger, g_seen_script, g_seen_block, g_seen_hit;

/* Written on the game thread, read by the menu on the render thread: plain 32-bit values. */
static volatile int32_t g_triggers[5];   /* by wbl_trigger_source */
static volatile int32_t g_blocks[6];     /* by wbl_block_kind */
static volatile int32_t g_script_calls;
static volatile const char* g_last_script = "";
static volatile int32_t g_hits;
static volatile int32_t g_last_victim = -1, g_last_attacker = -1, g_last_damage;
static wbl_bool g_no_damage = WBL_FALSE;
static wbl_bool g_hide_dialogs = WBL_FALSE;

static wbl_event_reply WBL_CALL OnTrigger(const wbl_event* e, void* user) {
	const wbl_trigger_event* t = (const wbl_trigger_event*)e->data;
	(void)user;
	char text[96];
	if (t->source < 5) ++g_triggers[t->source];
	snprintf(text, sizeof text, "module trigger: source %u, context %d", (unsigned)t->source, (int)t->context);
	LogFirst(&g_seen_trigger, text);
	return WBL_EVENT_CONTINUE;
}

static wbl_event_reply WBL_CALL OnScriptCall(const wbl_event* e, void* user) {
	const wbl_script_call_event* s = (const wbl_script_call_event*)e->data;
	(void)user;
	char text[128];
	++g_script_calls;
	g_last_script = s->name;
	snprintf(text, sizeof text, "script call: %s (%u params, defined %d, result %lld)", s->name,
	         (unsigned)s->param_count, (int)s->defined, (long long)s->result);
	LogFirst(&g_seen_script, text);
	return WBL_EVENT_CONTINUE;
}

static wbl_event_reply WBL_CALL OnBlock(const wbl_event* e, void* user) {
	wbl_block_event* b = (wbl_block_event*)e->data;
	(void)user;
	char text[96];
	if (b->kind < 6) ++g_blocks[b->kind];
	snprintf(text, sizeof text, "operation block: kind %u, context %d, passed %d", (unsigned)b->kind, (int)b->context,
	         (int)b->passed);
	LogFirst(&g_seen_block, text);
	/* POST: the outcome is writable. Hiding every dialog line shows how conditions can be overridden. */
	if (g_hide_dialogs && b->kind == WBL_BLOCK_DIALOG_CONDITION) b->passed = WBL_FALSE;
	return WBL_EVENT_CONTINUE;
}

static wbl_event_reply WBL_CALL OnAgentHit(const wbl_event* e, void* user) {
	wbl_agent_hit_event* h = (wbl_agent_hit_event*)e->data;
	(void)user;
	++g_hits;
	g_last_victim = h->victim_agent;
	g_last_attacker = h->attacker_agent;
	g_last_damage = h->damage;
	{
		char text[128];
		snprintf(text, sizeof text, "agent hit: agent %d hit by %d for %d (bone %d, missile %d)", (int)h->victim_agent,
		         (int)h->attacker_agent, (int)h->damage, (int)h->hit_bone, (int)h->missile_item);
		LogFirst(&g_seen_hit, text);
	}
	if (g_no_damage) h->damage = 0;
	return WBL_EVENT_CONTINUE;
}

static wbl_event_reply WBL_CALL OnUi(const wbl_event* e, void* user) {
	const wbl_ui_api* ui = g_host->ui;
	char line[160];
	(void)e;
	(void)user;
	if (ui->begin_window("game_events", NULL)) {
		snprintf(line, sizeof line, "triggers  mission %d  campaign %d  game %d  objects %d", (int)g_triggers[1],
		         (int)g_triggers[2], (int)g_triggers[3], (int)g_triggers[4]);
		ui->text(line);
		snprintf(line, sizeof line, "script calls %d, last %s", (int)g_script_calls, (const char*)g_last_script);
		ui->text(line);
		snprintf(line, sizeof line, "dialog cond %d / cons %d   menu opened %d, option cond %d / cons %d",
		         (int)g_blocks[1], (int)g_blocks[2], (int)g_blocks[3], (int)g_blocks[4], (int)g_blocks[5]);
		ui->text(line);
		snprintf(line, sizeof line, "agent hits %d, last: agent %d hit by %d for %d", (int)g_hits, (int)g_last_victim,
		         (int)g_last_attacker, (int)g_last_damage);
		ui->text(line);
		ui->checkbox("No damage to anyone", &g_no_damage);
		ui->checkbox("Hide every dialog line", &g_hide_dialogs);
	}
	ui->end_window();
	return WBL_EVENT_CONTINUE;
}

WBL_PLUGIN_QUERY {
	static const wbl_plugin_info info = {
	    sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 1, "example.game_events", "Game events", "1.0.0", "WarbandLib",
	    "Counts module triggers, engine script calls, dialogs, menus and agent hits.", 0, NULL,
	};
	return &info;
}

WBL_PLUGIN_LOAD(host, self) {
	g_host = host;
	g_self = self;
	if (host->abi_minor < 1) {
		host->core->log(self, WBL_LOG_ERROR, "needs WarbandLib ABI 1.1 (game events)");
		return WBL_ERR_VERSION_MISMATCH;
	}
	host->events->subscribe(self, WBL_POST(WBL_EVENT_MODULE_TRIGGER), WBL_PRIORITY_NORMAL, OnTrigger, NULL, NULL);
	host->events->subscribe(self, WBL_POST(WBL_EVENT_SCRIPT_CALL), WBL_PRIORITY_NORMAL, OnScriptCall, NULL, NULL);
	host->events->subscribe(self, WBL_POST(WBL_EVENT_OPERATION_BLOCK), WBL_PRIORITY_NORMAL, OnBlock, NULL, NULL);
	host->events->subscribe(self, WBL_POST(WBL_EVENT_AGENT_HIT), WBL_PRIORITY_NORMAL, OnAgentHit, NULL, NULL);
	host->events->subscribe(self, WBL_EVENT_UI, WBL_PRIORITY_NORMAL, OnUi, NULL, NULL);
	return WBL_OK;
}

WBL_PLUGIN_UNLOAD {
	char line[256];
	snprintf(line, sizeof line,
	         "seen: triggers %d/%d/%d/%d, script calls %d, dialog %d/%d, menu %d/%d/%d, agent hits %d",
	         (int)g_triggers[1], (int)g_triggers[2], (int)g_triggers[3], (int)g_triggers[4], (int)g_script_calls,
	         (int)g_blocks[1], (int)g_blocks[2], (int)g_blocks[3], (int)g_blocks[4], (int)g_blocks[5], (int)g_hits);
	g_host->core->log(g_self, WBL_LOG_INFO, line);
}
