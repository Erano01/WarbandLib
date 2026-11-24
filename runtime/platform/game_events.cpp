#include "runtime/platform/game_events.h"

#include <windows.h>

#include <intrin.h>

#include <atomic>
#include <climits>
#include <cstdint>
#include <cstring>

#include "runtime/host/game_event_flow.h"
#include "runtime/host/task_queue.h"
#include "runtime/platform/memory_access.h"
#include "runtime/platform/overlay.h"
#include "runtime/platform/runtime_state.h"

// Every address comes from the game data (internal catalogue); none is written here.
// The game's member functions are __thiscall; seen through __fastcall, `this` is ECX and EDX is unused.

namespace warbandlib::runtime::game_events {

namespace {

constexpr int kScriptSlots = 55;
constexpr int kScriptNameMax = 63;
constexpr std::int32_t kAgentHitContext = -28;

std::atomic<bool> g_active{false};
std::atomic<int> g_in_flight{0};
std::atomic<DWORD> g_game_thread{0};
host::TaskQueue g_tasks;

std::uintptr_t g_trigger_params = 0; // int64[8]
std::uintptr_t g_trigger_result = 0; // int64
std::uintptr_t g_game_ptr = 0;       // address of the game object pointer
std::uint32_t g_campaign_triggers = 0, g_game_simple_triggers = 0; // offsets inside the game object
std::uintptr_t g_mapped_scripts = 0; // int32[55]
std::uintptr_t g_script_names = 0;   // const char*[55]
char g_names[kScriptSlots][kScriptNameMax + 1] = {};

struct BlockSite {
	std::uintptr_t return_address;
	wbl_block_kind kind;
};
BlockSite g_block_sites[8] = {};
int g_block_site_count = 0;

host::EventBus& Bus() { return State().events; }

// Marks a callback window: Quiesce waits for these to end before plugins are unloaded.
struct Flight {
	Flight() {
		g_in_flight.fetch_add(1, std::memory_order_acq_rel);
		const DWORD self = GetCurrentThreadId();
		DWORD none = 0;
		g_game_thread.compare_exchange_strong(none, self);
	}
	~Flight() { g_in_flight.fetch_sub(1, std::memory_order_acq_rel); }
};

std::int64_t ReadI64(std::uintptr_t address) {
	std::int64_t v = 0;
	SafeRead(address, &v, sizeof(v));
	return v;
}
void WriteI64(std::uintptr_t address, std::int64_t v) { SafeWrite(address, &v, sizeof(v)); }

std::uintptr_t GameObject() {
	std::uint32_t game = 0;
	SafeRead(g_game_ptr, &game, sizeof(game));
	return game;
}

// ------------------------------------------------------------------ triggers

using TriggerManagerFn = void(__fastcall*)(void* self, void* edx, int context);
TriggerManagerFn o_trigger_manager = nullptr;
TriggerManagerFn o_simple_trigger_manager = nullptr;

void SnapshotParams(wbl_trigger_event& e) {
	if (e.context < 0) {
		SafeRead(g_trigger_params, e.params, sizeof(e.params));
	}
	e.result = ReadI64(g_trigger_result);
}

void WriteBackParams(const wbl_trigger_event& before, const wbl_trigger_event& now) {
	if (now.context < 0 && std::memcmp(before.params, now.params, sizeof(now.params)) != 0) {
		SafeWrite(g_trigger_params, now.params, sizeof(now.params));
	}
}

void AgentHitFromTrigger(const wbl_trigger_event& t, wbl_agent_hit_event& h) {
	h.struct_size = sizeof(h);
	h.victim_agent = static_cast<std::int32_t>(t.params[0]);
	h.attacker_agent = static_cast<std::int32_t>(t.params[1]);
	h.damage = t.result >= 0 ? static_cast<std::int32_t>(t.result) : static_cast<std::int32_t>(t.params[2]);
	h.hit_bone = static_cast<std::int32_t>(t.params[3]);
	h.missile_item = static_cast<std::int32_t>(t.params[4]);
}

// One dispatch of a trigger manager: typed hit (if any) around the generic trigger event around the module.
void DispatchTrigger(wbl_trigger_source source, void* self, void* edx, int context, TriggerManagerFn original) {
	const bool hit = source == WBL_TRIGGER_SOURCE_MISSION && context == kAgentHitContext &&
	                 host::AnySubscribers(Bus(), WBL_EVENT_AGENT_HIT);
	if (!hit && !host::AnySubscribers(Bus(), WBL_EVENT_MODULE_TRIGGER)) {
		original(self, edx, context);
		return;
	}
	Flight flight;
	wbl_trigger_event t{};
	t.struct_size = sizeof(t);
	t.source = source;
	t.context = context < 0 ? context : 0;
	SnapshotParams(t);
	const wbl_trigger_event entry = t;

	wbl_agent_hit_event h{};
	if (hit) {
		AgentHitFromTrigger(t, h);
		const std::int32_t engine_damage = h.damage;
		const bool cancelled = Bus().Post(WBL_PRE(WBL_EVENT_AGENT_HIT), &h);
		if (h.damage != engine_damage) t.result = h.damage;
		if (cancelled) { // module triggers skipped; the damage chosen in PRE still applies
			if (t.result != entry.result) WriteI64(g_trigger_result, t.result);
			return;
		}
	}

	const bool ran = host::RunPhased(Bus(), WBL_EVENT_MODULE_TRIGGER, &t, [&] {
		WriteBackParams(entry, t);
		if (t.result != entry.result) WriteI64(g_trigger_result, t.result);
		original(self, edx, context);
		const std::int64_t module_result = ReadI64(g_trigger_result);
		SnapshotParams(t);
		t.result = module_result;
	});
	if (!ran) {
		WriteBackParams(entry, t);
		if (t.result != entry.result) WriteI64(g_trigger_result, t.result);
	}
	const std::int64_t after_post = t.result;
	if (ran && after_post != ReadI64(g_trigger_result)) WriteI64(g_trigger_result, after_post);

	if (hit) {
		AgentHitFromTrigger(t, h);
		const std::int32_t applied = h.damage;
		Bus().Post(WBL_POST(WBL_EVENT_AGENT_HIT), &h);
		if (h.damage != applied) WriteI64(g_trigger_result, h.damage);
	}
}

void __fastcall TriggerManagerDetour(void* self, void* edx, int context) {
	if (!g_active.load(std::memory_order_acquire)) {
		o_trigger_manager(self, edx, context);
		return;
	}
	const std::uintptr_t game = GameObject();
	const bool campaign = game != 0 && reinterpret_cast<std::uintptr_t>(self) == game + g_campaign_triggers;
	DispatchTrigger(campaign ? WBL_TRIGGER_SOURCE_CAMPAIGN : WBL_TRIGGER_SOURCE_MISSION, self, edx, context,
	                o_trigger_manager);
}

void __fastcall SimpleTriggerManagerDetour(void* self, void* edx, int context) {
	if (!g_active.load(std::memory_order_acquire)) {
		o_simple_trigger_manager(self, edx, context);
		return;
	}
	const std::uintptr_t game = GameObject();
	const bool game_simple = game != 0 && reinterpret_cast<std::uintptr_t>(self) == game + g_game_simple_triggers;
	DispatchTrigger(game_simple ? WBL_TRIGGER_SOURCE_GAME_SIMPLE : WBL_TRIGGER_SOURCE_OBJECT, self, edx, context,
	                o_simple_trigger_manager);
}

// ------------------------------------------------------------- script calls

const char* ScriptName(int slot) {
	if (slot < 0 || slot >= kScriptSlots) return "";
	if (g_names[slot][0] == '\0' && g_script_names != 0) {
		std::uint32_t text = 0;
		if (SafeRead(g_script_names + static_cast<std::uintptr_t>(slot) * 4, &text, 4) == WBL_OK && text != 0) {
			char buffer[kScriptNameMax + 1] = {};
			SafeRead(text, buffer, kScriptNameMax);
			std::memcpy(g_names[slot], buffer, sizeof(buffer));
		}
	}
	return g_names[slot];
}

bool ScriptDefined(int slot) {
	std::int32_t mapped = -1;
	if (slot < 0 || slot >= kScriptSlots) return false;
	SafeRead(g_mapped_scripts + static_cast<std::uintptr_t>(slot) * 4, &mapped, 4);
	return mapped >= 0;
}

// Runs one engine script call with its PRE/POST events; `call` receives the (possibly changed) params.
template <typename Call>
char DispatchScript(int slot, std::uint32_t count, const int* params, Call&& call) {
	if (!host::AnySubscribers(Bus(), WBL_EVENT_SCRIPT_CALL)) {
		return call(params);
	}
	Flight flight;
	wbl_script_call_event e{};
	e.struct_size = sizeof(e);
	e.script = static_cast<std::uint32_t>(slot);
	e.name = ScriptName(slot);
	e.defined = ScriptDefined(slot) ? WBL_TRUE : WBL_FALSE;
	e.param_count = count;
	std::memcpy(e.params, params, count * sizeof(int));
	e.result = ReadI64(g_trigger_result);
	const std::int64_t entry_result = e.result;
	char returned = 0;
	host::RunPhased(Bus(), WBL_EVENT_SCRIPT_CALL, &e, [&] {
		if (e.result != entry_result) WriteI64(g_trigger_result, e.result);
		returned = call(e.params);
		e.result = ReadI64(g_trigger_result);
	});
	if (e.result != ReadI64(g_trigger_result)) WriteI64(g_trigger_result, e.result);
	return returned;
}

using Script2Fn = char(__fastcall*)(void*, void*, int, int, int);
using Script3Fn = char(__fastcall*)(void*, void*, int, int, int, int);
using Script4Fn = char(__fastcall*)(void*, void*, int, int, int, int, int);
using Script6Fn = char(__fastcall*)(void*, void*, int, int, int, int, int, int, int);
Script2Fn o_script2 = nullptr;
Script3Fn o_script3 = nullptr;
Script4Fn o_script4 = nullptr;
Script6Fn o_script6 = nullptr;

char __fastcall Script2Detour(void* self, void* edx, int slot, int a, int b) {
	if (!g_active.load(std::memory_order_acquire)) return o_script2(self, edx, slot, a, b);
	const int p[] = {a, b};
	return DispatchScript(slot, 2, p, [&](const int* q) { return o_script2(self, edx, slot, q[0], q[1]); });
}
char __fastcall Script3Detour(void* self, void* edx, int slot, int a, int b, int c) {
	if (!g_active.load(std::memory_order_acquire)) return o_script3(self, edx, slot, a, b, c);
	const int p[] = {a, b, c};
	return DispatchScript(slot, 3, p, [&](const int* q) { return o_script3(self, edx, slot, q[0], q[1], q[2]); });
}
char __fastcall Script4Detour(void* self, void* edx, int slot, int a, int b, int c, int d) {
	if (!g_active.load(std::memory_order_acquire)) return o_script4(self, edx, slot, a, b, c, d);
	const int p[] = {a, b, c, d};
	return DispatchScript(slot, 4, p,
	                      [&](const int* q) { return o_script4(self, edx, slot, q[0], q[1], q[2], q[3]); });
}
char __fastcall Script6Detour(void* self, void* edx, int slot, int a, int b, int c, int d, int e, int f) {
	if (!g_active.load(std::memory_order_acquire)) return o_script6(self, edx, slot, a, b, c, d, e, f);
	const int p[] = {a, b, c, d, e, f};
	return DispatchScript(slot, 6, p, [&](const int* q) {
		return o_script6(self, edx, slot, q[0], q[1], q[2], q[3], q[4], q[5]);
	});
}

// ---------------------------------------------------------- operation blocks

using ExecuteBlockFn = char(__fastcall*)(void* self, void* edx, int context);
ExecuteBlockFn o_execute_block = nullptr;

char __fastcall ExecuteBlockDetour(void* self, void* edx, int context) {
	const auto from = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
	if (!g_active.load(std::memory_order_acquire) || !host::AnySubscribers(Bus(), WBL_EVENT_OPERATION_BLOCK)) {
		return o_execute_block(self, edx, context);
	}
	wbl_block_kind kind = 0;
	for (int i = 0; i < g_block_site_count; ++i) {
		if (g_block_sites[i].return_address == from) kind = g_block_sites[i].kind;
	}
	if (kind == 0) return o_execute_block(self, edx, context);

	Flight flight;
	wbl_block_event e{};
	e.struct_size = sizeof(e);
	e.kind = kind;
	e.context = context;
	e.passed = WBL_FALSE;
	host::RunPhased(Bus(), WBL_EVENT_OPERATION_BLOCK, &e,
	                [&] { e.passed = o_execute_block(self, edx, context) != 0 ? WBL_TRUE : WBL_FALSE; });
	return e.passed ? 1 : 0;
}

// -------------------------------------------------------------------- setup

std::uintptr_t Live(const host::SymbolTable& s, const char* name) {
	const auto v = s.Get(name);
	return v ? State().game_info.image_base + (*v - 0x00400000u) : 0;
}

bool HookAt(const host::SymbolTable& s, const char* name, void* detour, void** original) {
	const std::uintptr_t target = Live(s, name);
	if (target == 0) return false;
	// Outermost in every chain, so _ReturnAddress() is the game's call site and plugins' raw hooks see PRE-adjusted input.
	const wbl_result r = State().hooks.Create(&g_active, reinterpret_cast<void*>(target), detour, INT32_MAX, original, nullptr);
	if (r != WBL_OK) Logf(WBL_LOG_ERROR, "game-events", "hook %s failed (%d)", name, r);
	return r == WBL_OK;
}

} // namespace

int Install(const host::SymbolTable* symbols) {
	if (symbols == nullptr) {
		Log(WBL_LOG_WARN, "game-events", "no game data for this exe: game events stay silent");
		return 0;
	}
	const host::SymbolTable& s = *symbols;
	g_trigger_params = Live(s, "events.trigger_params");
	g_trigger_result = Live(s, "events.trigger_result");
	g_game_ptr = Live(s, "events.game");
	g_campaign_triggers = s.Get("events.game.campaign_triggers").value_or(0);
	g_game_simple_triggers = s.Get("events.game.simple_triggers").value_or(0);
	g_mapped_scripts = Live(s, "events.mapped_scripts");
	g_script_names = Live(s, "events.script_names");
	const bool globals = g_trigger_params && g_trigger_result && g_game_ptr && g_campaign_triggers &&
	                     g_game_simple_triggers && g_mapped_scripts && g_script_names;
	if (!globals) {
		Log(WBL_LOG_WARN, "game-events", "game data lacks the event globals: game events stay silent");
		return 0;
	}

	const struct {
		const char* name;
		wbl_block_kind kind;
	} sites[] = {
	    {"events.block.dialog_condition", WBL_BLOCK_DIALOG_CONDITION},
	    {"events.block.dialog_consequence", WBL_BLOCK_DIALOG_CONSEQUENCE},
	    {"events.block.menu_opened", WBL_BLOCK_MENU_OPENED},
	    {"events.block.menu_option_condition", WBL_BLOCK_MENU_OPTION_CONDITION},
	    {"events.block.menu_option_consequence", WBL_BLOCK_MENU_OPTION_CONSEQUENCE},
	    {"events.block.menu_option_consequence_in_mission", WBL_BLOCK_MENU_OPTION_CONSEQUENCE},
	};
	for (const auto& site : sites) {
		const std::uintptr_t call = Live(s, site.name);
		if (call != 0) g_block_sites[g_block_site_count++] = BlockSite{call + 5, site.kind}; // after `call rel32`
	}

	int live = 0;
	live += HookAt(s, "events.trigger_manager_execute", reinterpret_cast<void*>(&TriggerManagerDetour),
	               reinterpret_cast<void**>(&o_trigger_manager));
	live += HookAt(s, "events.simple_trigger_manager_execute", reinterpret_cast<void*>(&SimpleTriggerManagerDetour),
	               reinterpret_cast<void**>(&o_simple_trigger_manager));
	live += HookAt(s, "events.script_call_2", reinterpret_cast<void*>(&Script2Detour), reinterpret_cast<void**>(&o_script2));
	live += HookAt(s, "events.script_call_3", reinterpret_cast<void*>(&Script3Detour), reinterpret_cast<void**>(&o_script3));
	live += HookAt(s, "events.script_call_4", reinterpret_cast<void*>(&Script4Detour), reinterpret_cast<void**>(&o_script4));
	live += HookAt(s, "events.script_call_6", reinterpret_cast<void*>(&Script6Detour), reinterpret_cast<void**>(&o_script6));
	if (g_block_site_count > 0) {
		live += HookAt(s, "events.execute_block", reinterpret_cast<void*>(&ExecuteBlockDetour),
		               reinterpret_cast<void**>(&o_execute_block));
	}
	g_active.store(live > 0, std::memory_order_release);
	Logf(WBL_LOG_INFO, "game-events", "%d dispatch point(s) live, %d block site(s)", live, g_block_site_count);
	return live;
}

void Quiesce() {
	g_active.store(false, std::memory_order_release);
	for (int i = 0; i < 200 && g_in_flight.load(std::memory_order_acquire) != 0; ++i) Sleep(5);
	if (g_in_flight.load() != 0) Log(WBL_LOG_WARN, "game-events", "a game event callback is still running");
}

bool IsGameThread() {
	const DWORD id = g_game_thread.load(std::memory_order_acquire);
	return id != 0 ? id == GetCurrentThreadId() : GetCurrentThreadId() == overlay::GameWindowThread();
}

void RunGameThreadTasks() {
	DWORD none = 0;
	g_game_thread.compare_exchange_strong(none, GetCurrentThreadId());
	Flight flight;
	g_tasks.Drain();
}

wbl_result QueueGameThreadTask(const void* owner, wbl_task_fn fn, void* user_data) {
	if (fn == nullptr) return WBL_ERR_INVALID_ARGUMENT;
	g_tasks.Push(owner, fn, user_data);
	if (!overlay::WakeGameThread()) {
		Log(WBL_LOG_DEBUG, "game-events", "game window not known yet, task waits for it");
	}
	return WBL_OK;
}

void RemoveOwnerTasks(const void* owner) { g_tasks.RemoveOwner(owner); }

} // namespace warbandlib::runtime::game_events
