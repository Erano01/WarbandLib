/* Compile-time lock of the published x86 layout. A failure here means an ABI break. */
#include "warbandlib/warbandlib.h"

#if UINTPTR_MAX == 0xFFFFFFFFu

#define WBL_LOCK_SIZE(type, size) _Static_assert(sizeof(type) == (size), #type " size changed")
#define WBL_LOCK_OFFSET(type, member, offset) \
	_Static_assert(offsetof(type, member) == (offset), #type "." #member " moved")

WBL_LOCK_SIZE(wbl_dependency, 16);
WBL_LOCK_OFFSET(wbl_dependency, flags, 12);

WBL_LOCK_SIZE(wbl_plugin_info, 40);
WBL_LOCK_OFFSET(wbl_plugin_info, id, 12);
WBL_LOCK_OFFSET(wbl_plugin_info, version, 20);
WBL_LOCK_OFFSET(wbl_plugin_info, dependency_count, 32);
WBL_LOCK_OFFSET(wbl_plugin_info, dependencies, 36);

WBL_LOCK_SIZE(wbl_game_info, 84);
WBL_LOCK_OFFSET(wbl_game_info, sha256, 16);

WBL_LOCK_SIZE(wbl_event, 16);
WBL_LOCK_SIZE(wbl_frame_event, 32);
WBL_LOCK_OFFSET(wbl_frame_event, frame_index, 24);
WBL_LOCK_SIZE(wbl_device_event, 16);

WBL_LOCK_SIZE(wbl_trigger_event, 88);
WBL_LOCK_OFFSET(wbl_trigger_event, params, 16);
WBL_LOCK_OFFSET(wbl_trigger_event, result, 80);
WBL_LOCK_SIZE(wbl_script_call_event, 56);
WBL_LOCK_OFFSET(wbl_script_call_event, params, 20);
WBL_LOCK_OFFSET(wbl_script_call_event, result, 48);
WBL_LOCK_SIZE(wbl_block_event, 16);
WBL_LOCK_SIZE(wbl_agent_hit_event, 24);

WBL_LOCK_SIZE(wbl_core_api, 48);
WBL_LOCK_OFFSET(wbl_core_api, get_interface, 36);
WBL_LOCK_OFFSET(wbl_core_api, run_on_game_thread, 40);
WBL_LOCK_OFFSET(wbl_core_api, is_game_thread, 44);
WBL_LOCK_SIZE(wbl_events_api, 24);
WBL_LOCK_SIZE(wbl_hooks_api, 12);
WBL_LOCK_SIZE(wbl_memory_api, 20);
WBL_LOCK_SIZE(wbl_ui_api, 60);
WBL_LOCK_SIZE(wbl_draw_api, 24);

WBL_LOCK_SIZE(wbl_host_api, 36);
WBL_LOCK_OFFSET(wbl_host_api, core, 12);
WBL_LOCK_OFFSET(wbl_host_api, draw, 32);

#endif

int wbl_abi_lock_anchor;
