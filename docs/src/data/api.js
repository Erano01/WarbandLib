// Single source for the reference page. Keep in step with sdk/include/warbandlib/warbandlib.h.
export const tables = [
  {
    id: 'host',
    name: 'wbl_host_api',
    summary: 'Passed to wbl_plugin_load. Valid until wbl_plugin_unload returns. Each sub-table is versioned on its own.',
    members: [
      ['uint32_t struct_size', 'Size of the table the host filled in.'],
      ['uint32_t abi_major, abi_minor', 'ABI version of the running host.'],
      ['const wbl_core_api* core', 'Logging, game info, plugin lookup, interfaces.'],
      ['const wbl_events_api* events', 'Subscribing to and raising events.'],
      ['const wbl_hooks_api* hooks', 'Shared inline function hooks.'],
      ['const wbl_memory_api* memory', 'Fault-safe reads and writes, pattern search.'],
      ['const wbl_ui_api* ui', 'Menu widgets, inside WBL_EVENT_UI only.'],
      ['const wbl_draw_api* draw', 'Screen-space drawing, inside WBL_EVENT_DRAW and WBL_EVENT_UI.'],
    ],
  },
  {
    id: 'core',
    name: 'wbl_core_api',
    summary: 'Services every plugin needs.',
    members: [
      ['void log(wbl_plugin self, wbl_log_level level, const char* message)', 'Writes "[LEVEL] [plugin.id] message" to the runtime log.'],
      ['const char* runtime_version(void)', 'Runtime version string, e.g. "0.1.0".'],
      ['wbl_result get_game_info(wbl_game_info* out)', 'Fills image base/size and the exe SHA-256. Set out->struct_size first; the host fills only what fits.'],
      ['uintptr_t rebase(uintptr_t static_va)', 'Turns an address relative to the 0x00400000 image base into the live address.'],
      ['wbl_result get_data_directory(wbl_plugin self, char* buffer, uint32_t size, uint32_t* out_len)', 'plugins/<id>/ with a trailing separator, created on demand. WBL_ERR_BUFFER_TOO_SMALL reports the needed length.'],
      ['wbl_plugin find_plugin(const char* id)', 'A loaded plugin by id, or NULL.'],
      ['const wbl_plugin_info* get_plugin_info(wbl_plugin plugin)', 'The host-owned copy of that plugin\'s info.'],
      ['wbl_result register_interface(wbl_plugin self, const char* name, uint32_t version, const void* table)', 'Offers a versioned function table to other plugins. One provider per name; the owner may re-register a newer version.'],
      ['const void* get_interface(const char* name, uint32_t min_version, uint32_t* out_version)', 'The table if its version is at least min_version, else NULL.'],
    ],
  },
  {
    id: 'events',
    name: 'wbl_events_api',
    summary: 'Priority-ordered event bus shared by the host and all plugins.',
    members: [
      ['wbl_result subscribe(wbl_plugin self, wbl_event_id id, int32_t priority, wbl_event_callback cb, void* user, wbl_subscription* out)', 'Higher priority runs first; equal priorities run in subscription order. out may be NULL.'],
      ['wbl_result unsubscribe(wbl_subscription s)', 'Safe inside a callback; a removed callback is never called again.'],
      ['wbl_result register_event(wbl_plugin self, const char* name, wbl_event_flags flags, wbl_event_id* out_id)', 'Creates a custom event (id ≥ WBL_EVENT_FIRST_CUSTOM). The same name always gives the same id.'],
      ['wbl_result find_event(const char* name, wbl_event_id* out_id)', 'Looks up a custom event by name.'],
      ['wbl_result post(wbl_event_id id, const void* data, wbl_bool* out_cancelled)', 'Runs subscribers on the calling thread. Custom events only; built-ins are raised by the host.'],
    ],
  },
  {
    id: 'hooks',
    name: 'wbl_hooks_api',
    summary: 'Inline hooks that several plugins can put on the same function.',
    members: [
      ['wbl_result create(wbl_plugin self, void* target, void* detour, int32_t priority, void** original_slot, wbl_hook* out)', 'Adds detour to target\'s chain. The detour continues by calling *original_slot, which the host rewrites whenever the chain changes, so it must stay valid until removal.'],
      ['wbl_result remove(wbl_hook hook)', 'Unlinks the detour. Every hook a plugin still owns is removed when it unloads.'],
    ],
  },
  {
    id: 'memory',
    name: 'wbl_memory_api',
    summary: 'Memory access that reports errors instead of crashing the game.',
    members: [
      ['wbl_result read(uintptr_t address, void* out, uint32_t size)', 'WBL_ERR_ACCESS_VIOLATION on a bad address.'],
      ['wbl_result write(uintptr_t address, const void* data, uint32_t size)', 'Lifts page protection if needed and flushes the instruction cache for code.'],
      ['wbl_bool is_readable(uintptr_t address, uint32_t size)', 'Whether the whole range is committed and readable.'],
      ['wbl_result find_pattern(const char* pattern, uintptr_t* out_address)', 'First match of "8B 0D ?? ?? ?? ??"-style patterns in mb_warband.exe.'],
    ],
  },
  {
    id: 'ui',
    name: 'wbl_ui_api',
    summary: 'Immediate-mode menu widgets. Calls outside WBL_EVENT_UI, or from another thread, are ignored.',
    members: [
      ['wbl_bool begin_window(const char* title, wbl_bool* open)', 'Always pair with end_window, even when it returns false.'],
      ['void end_window(void)', ''],
      ['void text(const char*) / text_colored(wbl_color, const char*)', 'Pre-formatted UTF-8 text.'],
      ['wbl_bool button(const char* label)', 'True on the frame it was clicked.'],
      ['wbl_bool checkbox(const char* label, wbl_bool* value)', 'True when the value changed.'],
      ['wbl_bool slider_int(label, int32_t* v, min, max) / slider_float(label, float* v, min, max)', 'True when the value changed.'],
      ['wbl_bool selectable(const char* label, wbl_bool selected)', 'True when clicked.'],
      ['wbl_bool collapsing_header(const char* label)', 'True while expanded.'],
      ['void same_line(void) / separator(void) / push_id(const char*) / pop_id(void)', 'Layout and widget-id scoping.'],
    ],
  },
  {
    id: 'draw',
    name: 'wbl_draw_api',
    summary: 'Screen-space shapes drawn above the game, inside WBL_EVENT_DRAW and WBL_EVENT_UI. Colors are 0xAARRGGBB (WBL_RGBA).',
    members: [
      ['void line(x1, y1, x2, y2, wbl_color color, float thickness)', ''],
      ['void rect(x1, y1, x2, y2, wbl_color color, float thickness) / rect_filled(x1, y1, x2, y2, color)', ''],
      ['void circle(x, y, radius, wbl_color color, float thickness)', ''],
      ['void text(x, y, wbl_color color, const char* text)', ''],
    ],
  },
]

export const events = [
  ['WBL_EVENT_FRAME', '1', 'wbl_frame_event', 'Render thread, every frame, back buffer bound inside BeginScene/EndScene. Raw Direct3D 9; restore any state you change.'],
  ['WBL_EVENT_DRAW', '2', 'wbl_frame_event', 'Render thread, every frame, inside the overlay frame. draw api.'],
  ['WBL_EVENT_UI', '3', 'wbl_frame_event', 'Render thread, while the menu is open (INSERT). ui and draw api.'],
  ['WBL_EVENT_DEVICE_RESET_BEGIN', '4', 'wbl_device_event', 'Before IDirect3DDevice9::Reset. Release D3DPOOL_DEFAULT resources.'],
  ['WBL_EVENT_DEVICE_RESET_END', '5', 'wbl_device_event', 'After Reset, with its HRESULT.'],
  ['WBL_EVENT_PLUGINS_LOADED', '6', 'NULL', 'Worker thread, once every plugin\'s load has returned. Look up other plugins\' interfaces here.'],
  ['WBL_EVENT_SHUTDOWN', '7', 'NULL', 'Worker thread, before plugins are unloaded.'],
]

export const results = [
  ['WBL_OK', '0'],
  ['WBL_ERR_INVALID_ARGUMENT', '-1'],
  ['WBL_ERR_NOT_FOUND', '-2'],
  ['WBL_ERR_ALREADY_EXISTS', '-3'],
  ['WBL_ERR_UNSUPPORTED', '-4'],
  ['WBL_ERR_ACCESS_VIOLATION', '-5'],
  ['WBL_ERR_HOOK_FAILED', '-6'],
  ['WBL_ERR_VERSION_MISMATCH', '-7'],
  ['WBL_ERR_BUFFER_TOO_SMALL', '-8'],
  ['WBL_ERR_WRONG_CONTEXT', '-9'],
  ['WBL_ERR_INTERNAL', '-100'],
]
