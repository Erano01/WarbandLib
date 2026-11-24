<script setup>
import CodeBlock from '../components/CodeBlock.vue'
import Callout from '../components/Callout.vue'
const cmake = `
# Inside the WarbandLib tree, next to the examples:
warbandlib_add_plugin(my_plugin src/my_plugin.cpp)

# Elsewhere: the SDK is a single header, any 32-bit MSVC-ABI DLL works.
add_library(my_plugin SHARED src/my_plugin.cpp)
target_include_directories(my_plugin PRIVATE path/to/WarbandLib/sdk/include)
set_target_properties(my_plugin PROPERTIES PREFIX "")`
const info = `
static const wbl_dependency deps[] = {
    { sizeof(wbl_dependency), "coop.net", "0.3.0", WBL_DEPENDENCY_REQUIRED },
    { sizeof(wbl_dependency), "example.hello_c", NULL, WBL_DEPENDENCY_OPTIONAL },
};

WBL_PLUGIN_QUERY {
    static const wbl_plugin_info info = {
        sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 0,
        "me.campaign_sync", "Campaign sync", "0.1.0", "me",
        "Keeps world-map parties in step.", 2, deps,
    };
    return &info;
}`
const events = `
static const wbl_host_api* host;

static wbl_event_reply WBL_CALL on_ui(const wbl_event* e, void* user) {
    static wbl_bool enabled = WBL_TRUE;
    if (host->ui->begin_window("My plugin", NULL)) {
        host->ui->checkbox("Enabled", &enabled);
    }
    host->ui->end_window();
    return WBL_EVENT_CONTINUE;
}

WBL_PLUGIN_LOAD(h, self) {
    host = h;
    return host->events->subscribe(self, WBL_EVENT_UI, WBL_PRIORITY_NORMAL, on_ui, NULL, NULL);
}`
const hook = `
/* A __thiscall member function seen as __fastcall: this in ECX, EDX unused. */
typedef void (__fastcall *set_hp_fn)(void* agent, void* edx, float* hp);
static set_hp_fn original;

static void __fastcall set_hp_detour(void* agent, void* edx, float* hp) {
    /* before */
    original(agent, edx, hp);   /* the next detour in the chain, or the game */
    /* after */
}

/* in load: */
host->hooks->create(self, target, (void*)set_hp_detour, WBL_PRIORITY_NORMAL,
                    (void**)&original, NULL);`
const iface = `
/* provider */
typedef struct net_api_v1 {
    uint32_t struct_size;
    wbl_result (WBL_CALL *send)(uint32_t peer, const void* data, uint32_t size);
} net_api_v1;
static const net_api_v1 api = { sizeof(net_api_v1), net_send };
host->core->register_interface(self, "coop.net.api", 1, &api);

/* consumer, from WBL_EVENT_PLUGINS_LOADED */
const net_api_v1* net = host->core->get_interface("coop.net.api", 1, NULL);`
</script>

<template>
  <h1>Writing a plugin</h1>
  <p class="lead">A plugin is a 32-bit DLL that exports three functions and talks to the game only through the host table.</p>

  <h2>Build setup</h2>
  <CodeBlock :code="cmake" lang="cmake" title="CMakeLists.txt" />

  <h2>Identity and dependencies</h2>
  <p>The id is permanent: lower case, digits, <code>.</code> <code>_</code> <code>-</code>, up to 64 characters, ideally <code>author.name</code>. Versions are <code>major.minor.patch</code>. Required dependencies must be present, loaded and at least <code>min_version</code>; optional ones only affect ordering.</p>
  <CodeBlock :code="info" lang="c" />

  <h2>Lifecycle</h2>
  <ul>
    <li><strong>query</strong> returns a pointer to static info. It must not call the host.</li>
    <li><strong>load</strong> gets the host table and your handle. Subscribe, hook, register. Return <code>WBL_OK</code>, or an error to be unloaded cleanly.</li>
    <li><strong>unload</strong> is optional. Stop your own threads here; events, hooks and interfaces you still own are removed for you afterwards.</li>
  </ul>

  <h2>Events and menus</h2>
  <CodeBlock :code="events" lang="c" />

  <h2>Hooking a game function</h2>
  <CodeBlock :code="hook" lang="c" />
  <Callout kind="warn" title="Keep original_slot alive">
    <p>The host writes the next link of the chain into <code>original_slot</code> whenever another plugin joins or leaves, so it must be a global or otherwise outlive the hook. Never cache its value.</p>
  </Callout>
  <p>Higher priority detours are called first and see the call before lower ones. Use <code>WBL_PRIORITY_HIGH</code> to observe or veto, <code>LOW</code> to act last before the game.</p>

  <h2>Offering a service</h2>
  <CodeBlock :code="iface" lang="c" />
  <p>Version the table name's contract like the ABI: append members and raise the version; consumers ask for the minimum they need.</p>
</template>
