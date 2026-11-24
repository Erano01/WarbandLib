/*
 * WarbandLib plugin ABI.
 *
 * Plain C, x86, __cdecl. Every struct starts with uint32_t struct_size and
 * only ever grows at the end; every function table is append-only. A change
 * that breaks either rule bumps WBL_ABI_VERSION_MAJOR. Booleans are
 * wbl_bool (int32_t), enumerations are uint32_t constants, strings are
 * NUL-terminated UTF-8 owned by the caller (the host copies what it keeps).
 */
#ifndef WARBANDLIB_WARBANDLIB_H
#define WARBANDLIB_WARBANDLIB_H

#include <stddef.h>
#include <stdint.h>

#define WBL_ABI_VERSION_MAJOR 1u
#define WBL_ABI_VERSION_MINOR 1u

#if defined(_WIN32)
#define WBL_CALL __cdecl
#define WBL_PLUGIN_EXPORT __declspec(dllexport)
#else
#define WBL_CALL
#define WBL_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
#define WBL_EXTERN_C extern "C"
extern "C" {
#else
#define WBL_EXTERN_C
#endif

/* ------------------------------------------------------------------ basics */

typedef int32_t wbl_bool;
#define WBL_FALSE 0
#define WBL_TRUE 1

typedef int32_t wbl_result;
#define WBL_OK 0
#define WBL_ERR_INVALID_ARGUMENT (-1)
#define WBL_ERR_NOT_FOUND (-2)
#define WBL_ERR_ALREADY_EXISTS (-3)
#define WBL_ERR_UNSUPPORTED (-4)
#define WBL_ERR_ACCESS_VIOLATION (-5)
#define WBL_ERR_HOOK_FAILED (-6)
#define WBL_ERR_VERSION_MISMATCH (-7)
#define WBL_ERR_BUFFER_TOO_SMALL (-8)
#define WBL_ERR_WRONG_CONTEXT (-9)
#define WBL_ERR_INTERNAL (-100)

typedef struct wbl_plugin_s* wbl_plugin;
typedef struct wbl_subscription_s* wbl_subscription;
typedef struct wbl_hook_s* wbl_hook;

typedef uint32_t wbl_log_level;
#define WBL_LOG_TRACE 0u
#define WBL_LOG_DEBUG 1u
#define WBL_LOG_INFO 2u
#define WBL_LOG_WARN 3u
#define WBL_LOG_ERROR 4u

/* Colors are 0xAARRGGBB. */
typedef uint32_t wbl_color;
#define WBL_RGBA(r, g, b, a) \
	((wbl_color)((((uint32_t)(a) & 0xFFu) << 24) | (((uint32_t)(r) & 0xFFu) << 16) | \
	             (((uint32_t)(g) & 0xFFu) << 8) | ((uint32_t)(b) & 0xFFu)))

/* ----------------------------------------------------------------- plugins */

typedef uint32_t wbl_dependency_flags;
#define WBL_DEPENDENCY_REQUIRED 0u
#define WBL_DEPENDENCY_OPTIONAL 1u

typedef struct wbl_dependency {
	uint32_t struct_size;
	const char* id;
	const char* min_version; /* "major.minor.patch", NULL = any */
	wbl_dependency_flags flags;
} wbl_dependency;

typedef struct wbl_plugin_info {
	uint32_t struct_size;
	uint32_t abi_major;      /* WBL_ABI_VERSION_MAJOR the plugin was built against */
	uint32_t abi_minor;      /* lowest host minor the plugin needs */
	const char* id;          /* unique, [a-z0-9._-], 1..64 chars, e.g. "erano.coop" */
	const char* name;
	const char* version;     /* "major.minor.patch" */
	const char* author;
	const char* description;
	uint32_t dependency_count;
	const wbl_dependency* dependencies;
} wbl_plugin_info;

/* --------------------------------------------------------------- core api */

typedef struct wbl_game_info {
	uint32_t struct_size;
	uintptr_t image_base;
	uint32_t image_size;
	wbl_bool fingerprint_known; /* exe matches a build WarbandLib has data for */
	char sha256[65];            /* lowercase hex of mb_warband.exe on disk */
} wbl_game_info;

typedef void(WBL_CALL* wbl_task_fn)(void* user_data);

typedef struct wbl_core_api {
	uint32_t struct_size;
	void(WBL_CALL* log)(wbl_plugin self, wbl_log_level level, const char* message);
	const char*(WBL_CALL* runtime_version)(void);
	wbl_result(WBL_CALL* get_game_info)(wbl_game_info* out);
	/* static-analysis VA (image base 0x00400000) -> live VA */
	uintptr_t(WBL_CALL* rebase)(uintptr_t static_va);
	/* plugins/<id>/ with trailing separator, created on demand; out_len excludes NUL */
	wbl_result(WBL_CALL* get_data_directory)(wbl_plugin self, char* buffer, uint32_t buffer_size,
	                                         uint32_t* out_len);
	wbl_plugin(WBL_CALL* find_plugin)(const char* id);
	const wbl_plugin_info*(WBL_CALL* get_plugin_info)(wbl_plugin plugin);
	/* Services one plugin offers to others: a versioned table of function pointers. */
	wbl_result(WBL_CALL* register_interface)(wbl_plugin self, const char* name, uint32_t version,
	                                         const void* table);
	const void*(WBL_CALL* get_interface)(const char* name, uint32_t min_version, uint32_t* out_version);
	/* ABI 1.1 */
	/* Queues fn to run on the game thread between frames (in its window message loop), the only
	   safe place to change game state from render/ui callbacks. Runs once, in queue order. */
	wbl_result(WBL_CALL* run_on_game_thread)(wbl_plugin self, wbl_task_fn fn, void* user_data);
	/* True on the thread that runs the simulation, module code and every game event. */
	wbl_bool(WBL_CALL* is_game_thread)(void);
} wbl_core_api;

/* ------------------------------------------------------------- events api */

typedef uint32_t wbl_event_id;
/* data: wbl_frame_event. Raw D3D9 drawing, every frame, on the back buffer. */
#define WBL_EVENT_FRAME 1u
/* data: wbl_frame_event. Every frame inside the overlay frame; draw api only. */
#define WBL_EVENT_DRAW 2u
/* data: wbl_frame_event. While the overlay menu is open; ui + draw api. */
#define WBL_EVENT_UI 3u
/* data: wbl_device_event. */
#define WBL_EVENT_DEVICE_RESET_BEGIN 4u
#define WBL_EVENT_DEVICE_RESET_END 5u
/* data: NULL. After every plugin's load returned. */
#define WBL_EVENT_PLUGINS_LOADED 6u
/* data: NULL. Before any plugin is unloaded. */
#define WBL_EVENT_SHUTDOWN 7u
/* Ids from register_event start here. */
#define WBL_EVENT_FIRST_CUSTOM 0x10000u

/*
 * Game events (ABI 1.1). Raised from the game's own dispatch, on the game
 * thread, synchronously, while the game waits. Subscribe to one phase:
 * WBL_PRE(id) runs before module code and may change the payload or return
 * WBL_EVENT_CANCEL to skip module code (POST is then not raised);
 * WBL_POST(id) runs after module code and sees what the game will use.
 * event->data points to the payload; fields documented as writable may be
 * changed (cast away const). The payload is only valid during the callback.
 * Without game data for the running exe these events never fire.
 */
#define WBL_PHASE_PRE 0x1000u
#define WBL_PHASE_POST 0x2000u
#define WBL_PRE(id) ((wbl_event_id)((id) | WBL_PHASE_PRE))
#define WBL_POST(id) ((wbl_event_id)((id) | WBL_PHASE_POST))
#define WBL_EVENT_PHASE(id) ((id) & (WBL_PHASE_PRE | WBL_PHASE_POST))
#define WBL_EVENT_BASE(id) ((id) & ~(WBL_PHASE_PRE | WBL_PHASE_POST))

/* data: wbl_trigger_event. Module triggers run by the game's trigger managers. */
#define WBL_EVENT_MODULE_TRIGGER 0x100u
/* data: wbl_script_call_event. The engine calling one of its named game_* scripts. */
#define WBL_EVENT_SCRIPT_CALL 0x101u
/* data: wbl_block_event. Dialog lines and game menus run by the engine. */
#define WBL_EVENT_OPERATION_BLOCK 0x102u
/* data: wbl_agent_hit_event. An agent takes a hit in a mission. */
#define WBL_EVENT_AGENT_HIT 0x110u

typedef uint32_t wbl_event_flags;
#define WBL_EVENT_FLAG_CANCELLABLE 1u

typedef uint32_t wbl_event_reply;
#define WBL_EVENT_CONTINUE 0u
#define WBL_EVENT_CANCEL 1u

typedef struct wbl_event {
	uint32_t struct_size;
	wbl_event_id id;
	wbl_event_flags flags;
	const void* data;
} wbl_event;

typedef struct wbl_frame_event {
	uint32_t struct_size;
	void* d3d9_device; /* IDirect3DDevice9* */
	uint32_t width;
	uint32_t height;
	float delta_seconds;
	uint32_t reserved0; /* keeps frame_index 8-aligned on every compiler */
	uint64_t frame_index;
} wbl_frame_event;

typedef struct wbl_device_event {
	uint32_t struct_size;
	void* d3d9_device;
	void* present_parameters; /* D3DPRESENT_PARAMETERS* */
	int32_t hresult;          /* RESET_END only */
} wbl_device_event;

typedef uint32_t wbl_trigger_source;
#define WBL_TRIGGER_SOURCE_MISSION 1u     /* mission template triggers */
#define WBL_TRIGGER_SOURCE_CAMPAIGN 2u    /* campaign triggers (triggers.txt) */
#define WBL_TRIGGER_SOURCE_GAME_SIMPLE 3u /* game simple triggers (simple_triggers.txt) */
#define WBL_TRIGGER_SOURCE_OBJECT 4u      /* item, scene prop, presentation and map icon triggers */

typedef struct wbl_trigger_event {
	uint32_t struct_size;
	wbl_trigger_source source;
	int32_t context;    /* ti_* value of an event trigger (< 0); 0 for a timed pass */
	uint32_t reserved0;
	int64_t params[8];  /* trigger params 1..8 at entry, writable in PRE; all 0 for a timed pass */
	int64_t result;     /* trigger result, writable in PRE and POST */
} wbl_trigger_event;

typedef struct wbl_script_call_event {
	uint32_t struct_size;
	uint32_t script;      /* the engine's script slot */
	const char* name;     /* "game_get_item_buy_price_factor", ... */
	wbl_bool defined;     /* the module defines the script; when not, nothing runs */
	uint32_t param_count;
	int32_t params[6];    /* writable in PRE */
	uint32_t reserved0;
	int64_t result;       /* trigger result the engine reads back, writable in PRE and POST */
} wbl_script_call_event;

typedef uint32_t wbl_block_kind;
#define WBL_BLOCK_DIALOG_CONDITION 1u
#define WBL_BLOCK_DIALOG_CONSEQUENCE 2u
#define WBL_BLOCK_MENU_OPENED 3u
#define WBL_BLOCK_MENU_OPTION_CONDITION 4u
#define WBL_BLOCK_MENU_OPTION_CONSEQUENCE 5u

typedef struct wbl_block_event {
	uint32_t struct_size;
	wbl_block_kind kind;
	int32_t context;  /* the interpreter context the engine passed */
	wbl_bool passed;  /* conditions' outcome; POST: writable. PRE cancel returns it (default false). */
} wbl_block_event;

typedef struct wbl_agent_hit_event {
	uint32_t struct_size;
	int32_t victim_agent;
	int32_t attacker_agent;
	int32_t damage;       /* PRE: the engine's damage; POST: the damage applied. Writable in both. */
	int32_t hit_bone;
	int32_t missile_item; /* item kind of the missile, as the engine passes it */
} wbl_agent_hit_event;

typedef wbl_event_reply(WBL_CALL* wbl_event_callback)(const wbl_event* event, void* user_data);

/* Higher priority runs first. */
#define WBL_PRIORITY_HIGHEST 1000
#define WBL_PRIORITY_HIGH 500
#define WBL_PRIORITY_NORMAL 0
#define WBL_PRIORITY_LOW (-500)
#define WBL_PRIORITY_LOWEST (-1000)

typedef struct wbl_events_api {
	uint32_t struct_size;
	wbl_result(WBL_CALL* subscribe)(wbl_plugin self, wbl_event_id id, int32_t priority,
	                                wbl_event_callback callback, void* user_data, wbl_subscription* out);
	wbl_result(WBL_CALL* unsubscribe)(wbl_subscription subscription);
	/* Same name always yields the same id; the first caller becomes the owner. */
	wbl_result(WBL_CALL* register_event)(wbl_plugin self, const char* name, wbl_event_flags flags,
	                                     wbl_event_id* out_id);
	wbl_result(WBL_CALL* find_event)(const char* name, wbl_event_id* out_id);
	/* Runs subscribers on the calling thread. out_cancelled may be NULL. */
	wbl_result(WBL_CALL* post)(wbl_event_id id, const void* data, wbl_bool* out_cancelled);
} wbl_events_api;

/* --------------------------------------------------------------- hook api */

/*
 * Inline function hooks that several plugins can share. Detours on the same
 * target form a chain ordered by priority; each detour calls *original_slot to
 * continue, and the host rewrites that slot whenever the chain changes, so
 * the slot must stay valid until the hook is removed.
 */
typedef struct wbl_hooks_api {
	uint32_t struct_size;
	wbl_result(WBL_CALL* create)(wbl_plugin self, void* target, void* detour, int32_t priority,
	                             void** original_slot, wbl_hook* out);
	wbl_result(WBL_CALL* remove)(wbl_hook hook);
} wbl_hooks_api;

/* ------------------------------------------------------------- memory api */

typedef struct wbl_memory_api {
	uint32_t struct_size;
	/* Fault-safe: WBL_ERR_ACCESS_VIOLATION instead of a crash. */
	wbl_result(WBL_CALL* read)(uintptr_t address, void* out, uint32_t size);
	/* Lifts page protection when needed and flushes the instruction cache. */
	wbl_result(WBL_CALL* write)(uintptr_t address, const void* data, uint32_t size);
	wbl_bool(WBL_CALL* is_readable)(uintptr_t address, uint32_t size);
	/* "8B 0D ?? ?? ?? ?? 85 C9" over mb_warband.exe's image. */
	wbl_result(WBL_CALL* find_pattern)(const char* pattern, uintptr_t* out_address);
} wbl_memory_api;

/* ----------------------------------------------------------------- ui api */

/* Only valid inside WBL_EVENT_UI callbacks; elsewhere calls are ignored. */
typedef struct wbl_ui_api {
	uint32_t struct_size;
	/* Always pair with end_window, even when it returns false. */
	wbl_bool(WBL_CALL* begin_window)(const char* title, wbl_bool* open);
	void(WBL_CALL* end_window)(void);
	void(WBL_CALL* text)(const char* text);
	void(WBL_CALL* text_colored)(wbl_color color, const char* text);
	wbl_bool(WBL_CALL* button)(const char* label);
	wbl_bool(WBL_CALL* checkbox)(const char* label, wbl_bool* value);
	wbl_bool(WBL_CALL* slider_int)(const char* label, int32_t* value, int32_t min, int32_t max);
	wbl_bool(WBL_CALL* slider_float)(const char* label, float* value, float min, float max);
	wbl_bool(WBL_CALL* selectable)(const char* label, wbl_bool selected);
	wbl_bool(WBL_CALL* collapsing_header)(const char* label);
	void(WBL_CALL* same_line)(void);
	void(WBL_CALL* separator)(void);
	void(WBL_CALL* push_id)(const char* id);
	void(WBL_CALL* pop_id)(void);
} wbl_ui_api;

/* Screen-space drawing, valid inside WBL_EVENT_DRAW and WBL_EVENT_UI. */
typedef struct wbl_draw_api {
	uint32_t struct_size;
	void(WBL_CALL* line)(float x1, float y1, float x2, float y2, wbl_color color, float thickness);
	void(WBL_CALL* rect)(float x1, float y1, float x2, float y2, wbl_color color, float thickness);
	void(WBL_CALL* rect_filled)(float x1, float y1, float x2, float y2, wbl_color color);
	void(WBL_CALL* circle)(float x, float y, float radius, wbl_color color, float thickness);
	void(WBL_CALL* text)(float x, float y, wbl_color color, const char* text);
} wbl_draw_api;

/* --------------------------------------------------------------- host api */

typedef struct wbl_host_api {
	uint32_t struct_size;
	uint32_t abi_major;
	uint32_t abi_minor;
	const wbl_core_api* core;
	const wbl_events_api* events;
	const wbl_hooks_api* hooks;
	const wbl_memory_api* memory;
	const wbl_ui_api* ui;
	const wbl_draw_api* draw;
} wbl_host_api;

/* True when the table is new enough to contain `member`. */
#define WBL_HAS_MEMBER(table, type, member) \
	((table) != NULL && (table)->struct_size >= offsetof(type, member) + sizeof(((type*)0)->member))

/* ---------------------------------------------------------- plugin exports */

/*
 * A plugin is a DLL in plugins/ exporting these three functions. query must
 * not touch the host. load runs once on the runtime's worker thread with a
 * host table that lives until unload returns. Event callbacks run on the
 * thread that raised the event: game events on the game thread, frame/ui
 * events on whatever thread presents (mostly a render thread). Game state
 * may only be changed on the game thread (see run_on_game_thread).
 */
typedef const wbl_plugin_info*(WBL_CALL* wbl_plugin_query_fn)(void);
typedef wbl_result(WBL_CALL* wbl_plugin_load_fn)(const wbl_host_api* host, wbl_plugin self);
typedef void(WBL_CALL* wbl_plugin_unload_fn)(void);

#define WBL_PLUGIN_QUERY_SYMBOL "wbl_plugin_query"
#define WBL_PLUGIN_LOAD_SYMBOL "wbl_plugin_load"
#define WBL_PLUGIN_UNLOAD_SYMBOL "wbl_plugin_unload"

#define WBL_PLUGIN_QUERY WBL_EXTERN_C WBL_PLUGIN_EXPORT const wbl_plugin_info* WBL_CALL wbl_plugin_query(void)
#define WBL_PLUGIN_LOAD(host_param, self_param) \
	WBL_EXTERN_C WBL_PLUGIN_EXPORT wbl_result WBL_CALL wbl_plugin_load(const wbl_host_api* host_param, wbl_plugin self_param)
#define WBL_PLUGIN_UNLOAD WBL_EXTERN_C WBL_PLUGIN_EXPORT void WBL_CALL wbl_plugin_unload(void)

#ifdef __cplusplus
}
#endif

#endif /* WARBANDLIB_WARBANDLIB_H */
