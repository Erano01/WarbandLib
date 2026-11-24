<template>
  <h1>Architecture</h1>
  <p class="lead">One runtime DLL inside the game, any number of plugin DLLs, and a single C header between them.</p>

  <svg class="diagram" viewBox="0 0 720 300" role="img" aria-label="Layers: game, runtime, plugins">
    <rect x="20" y="230" width="680" height="50" rx="8" class="box game" />
    <text x="360" y="260" class="label">mb_warband.exe · d3d9.dll</text>
    <rect x="20" y="120" width="680" height="90" rx="8" class="box rt" />
    <text x="360" y="142" class="label">warbandlib_runtime.dll</text>
    <g class="mini">
      <rect x="40" y="155" width="120" height="40" rx="6" /><text x="100" y="180">plugin loader</text>
      <rect x="170" y="155" width="120" height="40" rx="6" /><text x="230" y="180">event bus</text>
      <rect x="300" y="155" width="120" height="40" rx="6" /><text x="360" y="180">hook chains</text>
      <rect x="430" y="155" width="120" height="40" rx="6" /><text x="490" y="180">interfaces</text>
      <rect x="560" y="155" width="120" height="40" rx="6" /><text x="620" y="180">overlay / ui</text>
    </g>
    <line x1="20" y1="100" x2="700" y2="100" class="abi" />
    <text x="360" y="92" class="abi-label">warbandlib.h — C ABI 1.x</text>
    <g class="mini plugin">
      <rect x="40" y="20" width="190" height="50" rx="6" /><text x="135" y="50">coop.net (C++)</text>
      <rect x="265" y="20" width="190" height="50" rx="6" /><text x="360" y="50">coop.campaign</text>
      <rect x="490" y="20" width="190" height="50" rx="6" /><text x="585" y="50">your plugin (C)</text>
    </g>
  </svg>

  <h2>Runtime</h2>
  <p>On injection the runtime starts a worker thread, fingerprints <code>mb_warband.exe</code> (SHA-256), loads every DLL in <code>plugins/</code> in dependency order, then hooks <code>IDirect3DDevice9::Present</code> and <code>Reset</code> through the vtable of a throwaway device, which d3d9.dll shares with the game's own. Each presented frame raises <code>WBL_EVENT_FRAME</code>, <code>WBL_EVENT_DRAW</code> and, while the menu is open, <code>WBL_EVENT_UI</code>.</p>

  <h2>Plugin loading</h2>
  <p>Each DLL must export <code>wbl_plugin_query</code> and <code>wbl_plugin_load</code>. The runtime reads the plugin info, refuses an ABI it cannot serve, an invalid or duplicate id, missing or too-old dependencies and cycles, and orders the rest so dependencies load first; ties keep file-name order. A plugin that fails is unloaded together with everything it registered, and plugins that require it fail too. Calls into plugin code are guarded, so a plugin that crashes in load is reported instead of taking the game down.</p>

  <h2>Hook chains</h2>
  <p>The first hook on a function installs one MinHook detour that jumps through a pointer the runtime owns. Every plugin detour on that function joins a chain ordered by priority, and each calls its <code>original_slot</code> to continue. Adding or removing a detour rewrites only those pointers, innermost first, so the function is never re-patched and the chain is valid at every moment, whatever the calling convention.</p>

  <h2>Events and interfaces</h2>
  <p>Subscribers run highest priority first. Plugins can register their own named events and post them, and can publish a versioned table of functions under a name for others to fetch with <code>get_interface</code>. That is how larger systems are meant to be split: one plugin owns networking, others build on its interface.</p>

  <h2>Threads</h2>
  <table>
    <thead><tr><th>Thread</th><th>Runs</th></tr></thead>
    <tbody>
      <tr><td>Runtime worker</td><td><code>wbl_plugin_load</code>, <code>wbl_plugin_unload</code>, <code>PLUGINS_LOADED</code>, <code>SHUTDOWN</code></td></tr>
      <tr><td>Game render thread</td><td><code>FRAME</code>, <code>DRAW</code>, <code>UI</code>, device reset events</td></tr>
      <tr><td>Whoever calls the hooked function</td><td>Your detours</td></tr>
      <tr><td>Whoever posts it</td><td>Custom events</td></tr>
    </tbody>
  </table>

  <h2>Shutdown</h2>
  <p>On the <code>.unload</code> flag the runtime restores the D3D9 vtable and window procedure, waits for the render thread to leave, raises <code>SHUTDOWN</code>, calls each plugin's unload in reverse load order, removes whatever each still owns, waits again, frees the plugin DLLs and finally itself.</p>
</template>

<style scoped>
.diagram { width: 100%; height: auto; margin: 16px 0 8px; }
.box { stroke: var(--border); stroke-width: 1.5; }
.game { fill: var(--surface-2); }
.rt { fill: var(--accent-soft); }
.label { text-anchor: middle; fill: var(--text); font: 600 15px var(--font); }
.mini rect { fill: var(--surface); stroke: var(--border); }
.mini text { text-anchor: middle; fill: var(--text); font: 13px var(--font); }
.plugin rect { stroke: var(--accent); }
.abi { stroke: var(--accent); stroke-width: 2; stroke-dasharray: 6 5; }
.abi-label { text-anchor: middle; fill: var(--accent); font: 600 13px var(--mono); }
</style>
