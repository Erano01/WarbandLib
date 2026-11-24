<script setup>
import CodeBlock from '../components/CodeBlock.vue'
import Callout from '../components/Callout.vue'
const check = `
/* A plugin built against ABI 1.3 that can also run on 1.0 hosts */
if (WBL_HAS_MEMBER(host->core, wbl_core_api, some_1_3_function)) {
    host->core->some_1_3_function(...);
} else {
    /* fall back */
}`
const info = `
static const wbl_plugin_info info = {
    sizeof(wbl_plugin_info),
    WBL_ABI_VERSION_MAJOR, /* must equal the host's major       */
    2,                     /* lowest host minor this plugin needs */
    ...
};`
</script>

<template>
  <h1>ABI stability rules</h1>
  <p class="lead">A plugin built today must keep loading after the runtime is updated. These rules are what make that true, and every change to <code>warbandlib.h</code> is checked against them.</p>

  <h2>What the contract is</h2>
  <ul>
    <li>Plain C, x86, <code>__cdecl</code> for every function and export. No C++ types, exceptions or allocator ownership cross the boundary.</li>
    <li>Fixed-size types only: <code>uint32_t</code>, <code>int32_t</code>, <code>uintptr_t</code>, <code>float</code>. Booleans are <code>wbl_bool</code> (32-bit), enumerations are <code>uint32_t</code> constants, never C <code>enum</code>.</li>
    <li>Strings are NUL-terminated UTF-8, owned by the caller. The host copies whatever it keeps.</li>
    <li>Handles (<code>wbl_plugin</code>, <code>wbl_hook</code>, <code>wbl_subscription</code>) are opaque.</li>
  </ul>

  <h2>How it grows</h2>
  <p>Every struct and function table starts with <code>uint32_t struct_size</code>. New members are only ever appended. A host reading an older plugin's struct uses only the bytes the plugin declared; a plugin talking to an older host checks <code>struct_size</code> before using a newer member.</p>
  <CodeBlock :code="check" lang="c" />
  <table>
    <thead><tr><th>Change</th><th>Version</th></tr></thead>
    <tbody>
      <tr><td>Append a member to a struct or table, add an event id, add a result code, add a sub-table to <code>wbl_host_api</code></td><td>minor + 1</td></tr>
      <tr><td>Remove, reorder, retype or resize anything; change a calling convention or the meaning of a value</td><td>major + 1</td></tr>
    </tbody>
  </table>
  <CodeBlock :code="info" lang="c" title="declaring what the plugin needs" />
  <p>The host refuses a plugin whose <code>abi_major</code> differs from its own, or whose <code>abi_minor</code> is higher than its own.</p>

  <h2>History</h2>
  <table>
    <thead><tr><th>ABI</th><th>Runtime</th><th>Added</th></tr></thead>
    <tbody>
      <tr><td>1.0</td><td>0.1.0</td><td>Plugins, events, hooks, memory, ui and draw tables.</td></tr>
      <tr><td>1.1</td><td>0.2.0</td><td>Game events with PRE/POST phases and their payloads; <code>wbl_core_api.run_on_game_thread</code> and <code>is_game_thread</code>. 1.0 plugins run unchanged.</td></tr>
    </tbody>
  </table>

  <h2>Enforcement</h2>
  <p><code>sdk/abi_lock.c</code> is compiled into every Windows build and asserts the size and member offsets of every public type on x86. Moving a member or changing a size stops the build. When a minor version appends members, the lock is extended, never edited.</p>
  <Callout kind="note" title="Explicit padding">
    <p>Where alignment would differ between compilers, the header pads explicitly (for example <code>wbl_frame_event.reserved0</code> keeps its 64-bit counter 8-aligned everywhere).</p>
  </Callout>
</template>
