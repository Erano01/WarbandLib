<template>
  <h1>Example plugins</h1>
  <p class="lead">Three plugins in <code>examples/</code>, built into <code>bin/plugins/</code>, that together exercise the whole ABI.</p>

  <h2><code>example.hello_c</code> — plain C</h2>
  <p>Proves the header is real C. Draws a banner every frame with the draw api, shows a menu window with a checkbox and a button, registers the custom event <code>example.hello_c.greeted</code> and posts it on click, publishes the interface <code>example.hello_c.api</code> v1, and hooks <code>Agent_SetHitPoints</code> at <code>WBL_PRIORITY_HIGH</code>, counting calls.</p>

  <h2><code>example.hp_editor</code> — C++</h2>
  <p>Declares an optional dependency on <code>example.hello_c</code>, so it loads after it, and reads that plugin's greet counter through its interface. Scans the heap for live agents on a worker thread with the memory api, lists their hit points, and changes them by calling the game's own <code>Agent_SetHitPoints</code>. It hooks the same function at <code>WBL_PRIORITY_NORMAL</code>, so a click goes through both plugins' detours before reaching the game: a live two-plugin hook chain.</p>

  <h2><code>example.overlay_quad</code> — raw Direct3D 9</h2>
  <p>Subscribes to <code>WBL_EVENT_FRAME</code> and draws with the device directly, inside a state block so the game's render state is untouched. F9 toggles it.</p>
</template>
