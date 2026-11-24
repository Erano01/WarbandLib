<script setup>
import CodeBlock from '../components/CodeBlock.vue'
const hello = `
#include "warbandlib/warbandlib.h"

WBL_PLUGIN_QUERY {
    static const wbl_plugin_info info = {
        sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 0,
        "me.hello", "Hello", "1.0.0", "me", "Says hello.", 0, NULL,
    };
    return &info;
}

WBL_PLUGIN_LOAD(host, self) {
    host->core->log(self, WBL_LOG_INFO, "hello, Calradia");
    return WBL_OK;
}

WBL_PLUGIN_UNLOAD {}`
</script>

<template>
  <h1>WarbandLib</h1>
  <p class="lead">A plugin runtime for Mount &amp; Blade: Warband. Many plugins, written by different people with different compilers, load side by side into one game, share hooks and events, and keep working when the runtime is updated.</p>

  <div class="cards">
    <div class="card"><h3>Stable C ABI</h3><p>One plain C header. Versioned, size-prefixed tables that only grow. Locked at compile time.</p></div>
    <div class="card"><h3>Plugins that coexist</h3><p>Dependencies, load order, and hook chains with priorities, so two plugins can touch the same game function.</p></div>
    <div class="card"><h3>Services between plugins</h3><p>Custom events and versioned interfaces: a networking plugin can be the foundation others build on.</p></div>
    <div class="card"><h3>Linux or Windows</h3><p>Cross-compile from Linux with clang-cl, or build natively with Visual Studio. Same presets, same output.</p></div>
  </div>

  <h2>Why</h2>
  <p>Warband's module system loads exactly one module, compiled into numbered tables, so two mods cannot simply be combined. Script extenders add features to that one module but give extensions no way to share the engine. WarbandLib is the layer underneath: it owns the hooks and the overlay, and lets every plugin register what it needs through one contract.</p>

  <h2>A whole plugin</h2>
  <CodeBlock :code="hello" lang="c" title="hello.c" />
  <p>Build it as a 32-bit DLL, drop it in <code>plugins/</code> next to <code>warbandlib_runtime.dll</code>, and it loads with the game. Continue with <RouterLink to="/getting-started">Getting started</RouterLink>.</p>
</template>
